#include "application/application_controller.h"
#include "application/enhance_worker.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/datasets/dataset_partition_id.h"
#include "core/domain/validation/pair_validation_status.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using qart::application::ApplicationController;
using qart::application::ThermalProcessingMode;
using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;
using qart::core::domain::validation::PairValidationStatus;

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TemporaryDirectory final
{
public:
    TemporaryDirectory()
    {
        const auto stamp = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
        path_ = std::filesystem::temp_directory_path()
            / ("qart_test_enhance_worker_" + stamp);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void writeTestImage(
    const std::filesystem::path& path,
    int width,
    int height,
    int channels)
{
    std::filesystem::create_directories(path.parent_path());
    cv::Mat img(height, width, CV_MAKETYPE(CV_8U, channels));
    cv::RNG rng(12345 + width + channels);
    rng.fill(img, cv::RNG::UNIFORM, 10, 240);
    if (!cv::imwrite(path.string(), img)) {
        throw std::runtime_error("Failed to write test image: " + path.string());
    }
}

struct CapturedBatchStep {
    EnhanceJobRequest request;
    QImage resultImage;
    bool success = false;
    QString errorMessage;
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    try {
        TemporaryDirectory tempDir;
        const std::filesystem::path root = tempDir.path();

        const DatasetPair validPair(
            "roadscene/all/0001",
            "roadscene",
            DatasetPartitionId("all"),
            "0001",
            "raw/roadscene/vi/0001.png",
            "raw/roadscene/ir/0001.png"
        );

        const DatasetPair mismatchPair(
            "roadscene/all/0002",
            "roadscene",
            DatasetPartitionId("all"),
            "0002",
            "raw/roadscene/vi/0002.png",
            "raw/roadscene/ir/0002.png"
        );

        const DatasetPair missingThermalPair(
            "roadscene/all/0003",
            "roadscene",
            DatasetPartitionId("all"),
            "0003",
            "raw/roadscene/vi/0003.png",
            "raw/roadscene/ir/0003_missing.png"
        );

        writeTestImage(root / validPair.visibleRelativePath(), 64, 48, 3);
        writeTestImage(root / validPair.thermalRelativePath(), 64, 48, 1);

        writeTestImage(root / mismatchPair.visibleRelativePath(), 80, 60, 3);
        writeTestImage(root / mismatchPair.thermalRelativePath(), 64, 48, 3);

        writeTestImage(root / missingThermalPair.visibleRelativePath(), 64, 48, 3);

        // 1. Guard: validatePairAtRoot & getOrValidatePair without datasetRoot MUST throw,
        // never silently return MissingThermal.
        {
            bool threwEmptyRoot = false;
            try {
                (void)ApplicationController::validatePairAtRoot(
                    std::filesystem::path(),
                    validPair
                );
            } catch (const std::invalid_argument&) {
                threwEmptyRoot = true;
            }
            require(
                threwEmptyRoot,
                "validatePairAtRoot with empty datasetRoot did not throw invalid_argument"
            );

            ApplicationController uninitializedController;
            bool threwController = false;
            try {
                (void)uninitializedController.getOrValidatePair(validPair);
            } catch (const std::invalid_argument&) {
                threwController = true;
            }
            require(
                threwController,
                "getOrValidatePair with empty datasetRoot did not throw invalid_argument"
            );
        }

        // 2. validatePairAtRoot(root, pair) & cacheValidationResult round-trip.
        {
            const auto res = ApplicationController::validatePairAtRoot(root, validPair);
            require(
                res.status() == PairValidationStatus::Valid,
                "Valid pair validation failed when datasetRoot was provided"
            );
            require(res.visibleDecoded() && res.thermalDecoded(), "Expected decoded vis/thr");

            ApplicationController receiver;
            receiver.cacheValidationResult(res);
            const auto stored = receiver.findCachedValidationResult(validPair.pairId());
            require(stored.has_value() && *stored == res, "cacheValidationResult round-trip failed");
        }

        // 3. PreviewWorkerThread on uncached validPair with explicit datasetRoot.
        {
            PreviewJobRequest req;
            req.sessionId = 1;
            req.requestId = 10;
            req.itemIndex = 0;
            req.pair = validPair;
            req.validationResult = std::nullopt; // Explicitly uncached!
            req.datasetRoot = root;

            PreviewWorkerThread worker(req);
            PreviewJobResult received;
            bool finished = false;
            QEventLoop loop;
            QObject::connect(&worker, &PreviewWorkerThread::previewFinished,
                             &app, [&](const PreviewJobResult& res) {
                                 received = res;
                                 finished = true;
                                 loop.quit();
                             });
            worker.start();
            loop.exec();
            worker.wait();

            require(finished, "PreviewWorkerThread did not emit previewFinished");
            require(received.errorMessage.isEmpty(), "Unexpected errorMessage in valid preview");
            require(received.validationResult.has_value(), "Preview result missing validationResult");
            require(
                received.validationResult->status() == PairValidationStatus::Valid,
                "Uncached PreviewWorkerThread did not validate pair as Valid"
            );
            require(
                received.validationResult->thermalDecoded(),
                "Uncached PreviewWorkerThread reported thermalDecoded == false"
            );
            require(received.visFileExists && received.thrFileExists, "Expected preview files exist");
            require(received.visDecodeSuccess && received.thrDecodeSuccess, "Expected preview images decoded");
            require(!received.visibleImage.isNull() && !received.thermalImage.isNull(), "Preview QImages null");
            require(!received.channelDesc.isEmpty(), "Preview channelDesc empty");
            require(received.sourceStats.valid && received.sourceStats.mean > 0.0
                        && received.sourceStats.entropy > 0.0,
                    "Preview sourceStats missing or invalid");
        }

        // 4. PreviewWorkerThread on empty datasetRoot must report explicit errorMessage,
        // NOT MissingThermal!
        {
            PreviewJobRequest req;
            req.sessionId = 1;
            req.requestId = 11;
            req.itemIndex = 0;
            req.pair = validPair;
            req.validationResult = std::nullopt;
            req.datasetRoot = std::filesystem::path();

            PreviewWorkerThread worker(req);
            PreviewJobResult received;
            QEventLoop loop;
            QObject::connect(&worker, &PreviewWorkerThread::previewFinished,
                             &app, [&](const PreviewJobResult& res) {
                                 received = res;
                                 loop.quit();
                             });
            worker.start();
            loop.exec();
            worker.wait();

            require(!received.errorMessage.isEmpty(), "Empty datasetRoot preview must return errorMessage");
            require(!received.validationResult.has_value(), "Empty datasetRoot preview must not synthesize MissingThermal");
        }

        // 5. BatchEnhanceWorkerThread on uncached pairs:
        //    - validPair (uncached) -> validates in worker and processes N5 cleanly
        //    - mismatchPair (uncached) -> DimensionMismatch, thermalDecoded() == true -> still processes N5 cleanly!
        //    - missingThermalPair (uncached) -> skips via MissingThermal
        //    - uncached missing datasetRoot -> counts as error, not skipped!
        {
            std::vector<EnhanceJobRequest> requests;

            auto makeReq = [&](const DatasetPair& p, const std::filesystem::path& reqRoot) {
                EnhanceJobRequest r;
                r.sessionId = 2;
                r.requestId = 20;
                r.pairId = QString::fromStdString(p.pairId());
                r.datasetId = "roadscene";
                r.datasetRoot = reqRoot;
                r.thermalFullPath = reqRoot / p.thermalRelativePath();
                r.mode = ThermalProcessingMode::N5PostHalf;
                r.pair = p;
                r.validationResult = std::nullopt; // Explicitly uncached!
                return r;
            };

            requests.push_back(makeReq(validPair, root));
            requests.push_back(makeReq(mismatchPair, root));
            requests.push_back(makeReq(missingThermalPair, root));
            requests.push_back(makeReq(validPair, std::filesystem::path()));

            BatchEnhanceWorkerThread batchWorker(2, 20, requests);
            std::vector<CapturedBatchStep> steps;
            bool batchDone = false;
            int finalTotal = 0, finalSucc = 0, finalErr = 0, finalSkip = 0, finalCancel = 0;

            QEventLoop loop;
            QObject::connect(&batchWorker, &BatchEnhanceWorkerThread::jobStepFinished,
                             &app, [&](quint64,
                                       quint64,
                                       int,
                                       int,
                                       const EnhanceJobRequest& req,
                                       const QImage& resultImg,
                                       qint64,
                                       bool success,
                                       const QString& errMsg,
                                       int,
                                       int,
                                       int,
                                       int) {
                                 steps.push_back({req, resultImg, success, errMsg});
                             });
            QObject::connect(&batchWorker, &BatchEnhanceWorkerThread::batchFinished,
                             &app, [&](quint64, quint64, int t, int s, int e, int sk, int c) {
                                 batchDone = true;
                                 finalTotal = t;
                                 finalSucc = s;
                                 finalErr = e;
                                 finalSkip = sk;
                                 finalCancel = c;
                                 loop.quit();
                             });
            batchWorker.start();
            loop.exec();
            batchWorker.wait();

            require(batchDone, "BatchEnhanceWorkerThread did not emit batchFinished");
            require(finalTotal == 4, "Expected total == 4");
            require(finalSucc == 2, "Expected 2 uncached valid/mismatch pairs to succeed");
            require(finalSkip == 1, "Expected 1 missingThermal pair to skip");
            require(finalErr == 1, "Expected 1 missing datasetRoot item to error");
            require(finalCancel == 0, "Expected 0 cancelled");
            require(steps.size() == 4, "Expected 4 batch steps");

            // Step 0: valid uncached pair
            require(steps[0].success && !steps[0].request.isSkipped, "Step 0 failed or skipped");
            require(steps[0].request.validationResult.has_value()
                        && steps[0].request.validationResult->status() == PairValidationStatus::Valid,
                    "Step 0 validation status incorrect");
            require(!steps[0].request.sourcePreview.isNull() && steps[0].request.sourceChannels == 1,
                    "Step 0 did not produce in-memory sourcePreview & channels");
            require(!steps[0].resultImage.isNull()
                        && steps[0].resultImage.format() == QImage::Format_Grayscale8,
                    "Step 0 resultImage invalid");
            require(steps[0].request.metrics.valid, "Step 0 metrics not populated");
            require(steps[0].request.metrics.maxAbsDiff > 0 && steps[0].request.metrics.changedPixelPercent > 0.0,
                    "Step 0 N5 metrics expected non-zero enhancement delta");
            require(!steps[0].request.diffHeatmap.isNull()
                        && steps[0].request.diffHeatmap.size() == steps[0].resultImage.size(),
                    "Step 0 diffHeatmap missing or wrong size");
            require(steps[0].request.appliedParamsSummary.contains("CLAHE"),
                    "Step 0 appliedParamsSummary missing N5 summary");

            // Step 1: mismatched dims uncached pair (still thermal-valid in Phase 2)
            require(steps[1].success && !steps[1].request.isSkipped, "Step 1 (mismatch thermal) failed");
            require(steps[1].request.validationResult.has_value()
                        && steps[1].request.validationResult->status() == PairValidationStatus::DimensionMismatch,
                    "Step 1 validation status expected DimensionMismatch");
            require(!steps[1].request.sourcePreview.isNull() && steps[1].request.sourceChannels == 3,
                    "Step 1 missing 3-ch sourcePreview");
            require(!steps[1].resultImage.isNull(), "Step 1 resultImage null");
            require(steps[1].request.metrics.valid && !steps[1].request.diffHeatmap.isNull(),
                    "Step 1 metrics or diffHeatmap invalid");

            // Step 2: genuinely missing thermal on disk
            require(!steps[2].success && steps[2].request.isSkipped, "Step 2 should be skipped");
            require(steps[2].request.validationResult.has_value()
                        && steps[2].request.validationResult->status() == PairValidationStatus::MissingThermal,
                    "Step 2 should report MissingThermal");

            // Step 3: missing datasetRoot -> explicit error, not skipped!
            require(!steps[3].success && !steps[3].request.isSkipped,
                    "Step 3 missing datasetRoot must be error, not skipped");
        }

        // 6. SingleEnhanceWorkerThread metrics & diffHeatmap contract for N5 vs BYPASS.
        {
            EnhanceJobRequest n5Req;
            n5Req.sessionId = 3;
            n5Req.requestId = 30;
            n5Req.pairId = QString::fromStdString(validPair.pairId());
            n5Req.datasetId = "roadscene";
            n5Req.datasetRoot = root;
            n5Req.thermalFullPath = root / validPair.thermalRelativePath();
            n5Req.mode = ThermalProcessingMode::N5PostHalf;

            SingleEnhanceWorkerThread n5Worker(n5Req);
            EnhanceJobRequest n5OutReq;
            QImage n5Img;
            bool n5Ok = false;
            QEventLoop n5Loop;
            QObject::connect(&n5Worker, &SingleEnhanceWorkerThread::jobFinished,
                             &app, [&](const EnhanceJobRequest& req,
                                       const QImage& img,
                                       qint64,
                                       bool success,
                                       const QString&) {
                                 n5OutReq = req;
                                 n5Img = img;
                                 n5Ok = success;
                                 n5Loop.quit();
                             });
            n5Worker.start();
            n5Loop.exec();
            n5Worker.wait();

            require(n5Ok && !n5Img.isNull(), "SingleEnhanceWorkerThread N5 failed");
            require(n5OutReq.metrics.valid, "Single N5 metrics invalid");
            require(n5OutReq.metrics.maxAbsDiff > 0 && n5OutReq.metrics.mae > 0.0,
                    "Single N5 metrics expected positive diff");
            require(n5OutReq.metrics.inputMean > 0.0 && n5OutReq.metrics.inputEntropy > 0.0,
                    "Single N5 input baseline metrics missing");
            require(!n5OutReq.diffHeatmap.isNull()
                        && n5OutReq.diffHeatmap.size() == n5Img.size(),
                    "Single N5 diffHeatmap invalid");
            require(n5OutReq.appliedParamsSummary.contains("GF Base"),
                    "Single N5 appliedParamsSummary missing GF Base");

            EnhanceJobRequest bypassReq = n5Req;
            bypassReq.requestId = 31;
            bypassReq.mode = ThermalProcessingMode::Bypass;

            SingleEnhanceWorkerThread bypassWorker(bypassReq);
            EnhanceJobRequest bypassOutReq;
            QImage bypassImg;
            bool bypassOk = false;
            QEventLoop bypassLoop;
            QObject::connect(&bypassWorker, &SingleEnhanceWorkerThread::jobFinished,
                             &app, [&](const EnhanceJobRequest& req,
                                       const QImage& img,
                                       qint64,
                                       bool success,
                                       const QString&) {
                                 bypassOutReq = req;
                                 bypassImg = img;
                                 bypassOk = success;
                                 bypassLoop.quit();
                             });
            bypassWorker.start();
            bypassLoop.exec();
            bypassWorker.wait();

            require(bypassOk && !bypassImg.isNull(), "SingleEnhanceWorkerThread BYPASS failed");
            require(bypassOutReq.metrics.valid, "Single BYPASS metrics invalid");
            require(bypassOutReq.metrics.maxAbsDiff == 0
                        && bypassOutReq.metrics.mae == 0.0
                        && bypassOutReq.metrics.changedPixelPercent == 0.0,
                    "Single BYPASS metrics expected zero delta");
            require(bypassOutReq.appliedParamsSummary.contains("BYPASS"),
                    "Single BYPASS appliedParamsSummary missing BYPASS");
        }

        std::cout << "Enhance worker uncached preview/batch contract passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
