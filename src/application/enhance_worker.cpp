#include "enhance_worker.h"
#include "core/enhancement/thermal_input.h"
#include "core/enhancement/thermal_quality_metrics.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <exception>
#include <system_error>
#include <stdexcept>

namespace {

struct ThermalExecutionOutput {
    bool success = false;
    QImage image;
    QString errorMessage;
};

ThermalPlaneStats toWorkerPlaneStats(const qart::core::enhancement::PlaneQualityStats &src)
{
    ThermalPlaneStats stats;
    stats.valid = src.valid;
    stats.mean = src.mean;
    stats.stdDev = src.stdDev;
    stats.entropy = src.informationEntropy;
    stats.edgeGradient = src.averageGradient;
    stats.piqe = src.piqe;
    stats.minVal = src.minVal;
    stats.maxVal = src.maxVal;
    return stats;
}

ThermalPlaneStats computePlaneStats(const cv::Mat &gray8)
{
    return toWorkerPlaneStats(qart::core::enhancement::computePlaneQualityStats(gray8));
}

QImage buildDiffHeatmap(const cv::Mat &absDiff, int maxAbsDiff)
{
    if (absDiff.empty() || absDiff.type() != CV_8UC1) {
        return QImage();
    }

    cv::Mat stretched(absDiff.size(), CV_8UC1, cv::Scalar(0));
    if (maxAbsDiff > 0) {
        const double scale = 255.0 / static_cast<double>(maxAbsDiff);
        absDiff.convertTo(stretched, CV_8UC1, scale);
    }

    cv::Mat colorBgr;
    cv::applyColorMap(stretched, colorBgr, cv::COLORMAP_INFERNO);
    if (maxAbsDiff == 0) {
        // Ensure zero change renders cleanly as pure dark background
        colorBgr.setTo(cv::Scalar(0, 0, 0));
    }

    cv::Mat colorRgb;
    cv::cvtColor(colorBgr, colorRgb, cv::COLOR_BGR2RGB);
    return QImage(colorRgb.data,
                  colorRgb.cols,
                  colorRgb.rows,
                  static_cast<int>(colorRgb.step),
                  QImage::Format_RGB888).copy();
}

QImage matToQImageCopy(const cv::Mat &raw)
{
    if (raw.empty()) {
        return QImage();
    }
    if (raw.type() == CV_8UC1) {
        return QImage(
            raw.data,
            raw.cols,
            raw.rows,
            static_cast<int>(raw.step),
            QImage::Format_Grayscale8
        ).copy();
    }
    if (raw.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(raw, rgb, cv::COLOR_BGR2RGB);
        return QImage(
            rgb.data,
            rgb.cols,
            rgb.rows,
            static_cast<int>(rgb.step),
            QImage::Format_RGB888
        ).copy();
    }
    if (raw.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(raw, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(
            rgba.data,
            rgba.cols,
            rgba.rows,
            static_cast<int>(rgba.step),
            QImage::Format_RGBA8888
        ).copy();
    }
    return QImage();
}

void populateThermalMetricsAndDiff(
    const cv::Mat &prepared,
    const cv::Mat &output,
    qart::application::ThermalProcessingMode mode,
    EnhanceJobRequest &target,
    bool includeDiffHeatmap)
{
    if (mode == qart::application::ThermalProcessingMode::N5PostHalf) {
        target.appliedParamsSummary = QString("GF Base 7×7 | CLAHE(2.0, 8×8) | Post-Half 3×3 (×0.5)");
    } else if (mode == qart::application::ThermalProcessingMode::MsgfExperimental) {
        target.appliedParamsSummary = QString("MSGF r={3,8,16,32} | CLAHE(2.0, 8×8) | g={0.18,0.12,0.06,0.03}");
    } else {
        target.appliedParamsSummary = QString("BYPASS (Giữ nguyên đầu vào grayscale)");
    }

    const auto pairMetrics =
        qart::core::enhancement::computePairQualityMetrics(prepared, output);
    if (!pairMetrics.valid) {
        target.metrics = ThermalEnhancementMetrics{};
        target.diffHeatmap = QImage();
        return;
    }

    ThermalEnhancementMetrics m;
    m.valid = true;
    m.inputMean = pairMetrics.input.mean;
    m.outputMean = pairMetrics.output.mean;
    m.inputStd = pairMetrics.input.stdDev;
    m.outputStd = pairMetrics.output.stdDev;
    m.inputEntropy = pairMetrics.input.informationEntropy;
    m.outputEntropy = pairMetrics.output.informationEntropy;
    m.inputEdgeGrad = pairMetrics.input.averageGradient;
    m.outputEdgeGrad = pairMetrics.output.averageGradient;
    m.inputPiqe = pairMetrics.input.piqe;
    m.outputPiqe = pairMetrics.output.piqe;
    m.inputMin = pairMetrics.input.minVal;
    m.inputMax = pairMetrics.input.maxVal;
    m.outputMin = pairMetrics.output.minVal;
    m.outputMax = pairMetrics.output.maxVal;
    m.psnr = pairMetrics.psnr;
    m.mae = pairMetrics.mae;
    m.maxAbsDiff = pairMetrics.maxAbsDiff;
    m.changedPixelPercent = pairMetrics.changedPixelPercent;

    target.metrics = m;
    if (includeDiffHeatmap) {
        cv::Mat absDiff;
        cv::absdiff(output, prepared, absDiff);
        target.diffHeatmap = buildDiffHeatmap(absDiff, pairMetrics.maxAbsDiff);
    } else {
        target.diffHeatmap = QImage();
    }
}

ThermalExecutionOutput executeThermalMat(
    qart::application::ApplicationController &workerController,
    const cv::Mat &raw,
    qart::application::ThermalProcessingMode mode,
    EnhanceJobRequest *requestTarget = nullptr,
    bool captureSourcePreview = false,
    bool includeVisualOutputs = true)
{
    if (raw.empty()) {
        return {false, QImage(), QString("Ảnh nhiệt đầu vào rỗng.")};
    }

    if (requestTarget != nullptr) {
        requestTarget->sourceChannels = raw.channels();
        if (captureSourcePreview && includeVisualOutputs) {
            requestTarget->sourcePreview = matToQImageCopy(raw);
        } else {
            requestTarget->sourcePreview = QImage();
        }
    }

    const cv::Mat prepared = qart::core::enhancement::prepareThermalUint8Plane(raw);
    const auto result = workerController.processThermal(prepared, mode);
    const cv::Mat &outMat = result.outputImage;

    if (outMat.empty() || outMat.type() != CV_8UC1) {
        return {
            false,
            QImage(),
            "Kết quả thuật toán rỗng hoặc sai định dạng pixel CV_8UC1."
        };
    }

    if (requestTarget != nullptr) {
        populateThermalMetricsAndDiff(prepared, outMat, mode, *requestTarget, includeVisualOutputs);
    }

    if (!includeVisualOutputs) {
        return {true, QImage(), QString()};
    }

    QImage qImg(outMat.data,
                outMat.cols,
                outMat.rows,
                static_cast<int>(outMat.step),
                QImage::Format_Grayscale8);
    return {true, qImg.copy(), QString()};
}

ThermalExecutionOutput executeThermalRequest(
    qart::application::ApplicationController &workerController,
    const std::filesystem::path &thermalPath,
    qart::application::ThermalProcessingMode mode,
    EnhanceJobRequest *requestTarget = nullptr,
    bool captureSourcePreview = false,
    bool includeVisualOutputs = true)
{
    std::error_code fsErr;
    const bool exists = std::filesystem::exists(thermalPath, fsErr);
    if (fsErr) {
        return {
            false,
            QImage(),
            QString("Lỗi kiểm tra đường dẫn ảnh nhiệt (%1): %2")
                .arg(QString::fromStdString(thermalPath.string()),
                     QString::fromStdString(fsErr.message()))
        };
    }
    if (!exists) {
        return {
            false,
            QImage(),
            QString("Không tìm thấy file ảnh nhiệt trên đĩa: %1")
                .arg(QString::fromStdString(thermalPath.string()))
        };
    }

    const bool isRegular = std::filesystem::is_regular_file(thermalPath, fsErr);
    if (fsErr || !isRegular) {
        return {
            false,
            QImage(),
            QString("Đường dẫn ảnh nhiệt không phải tệp hợp lệ: %1")
                .arg(QString::fromStdString(thermalPath.string()))
        };
    }

    const cv::Mat raw = cv::imread(thermalPath.string(), cv::IMREAD_UNCHANGED);
    if (raw.empty()) {
        return {
            false,
            QImage(),
            QString("Không thể giải mã dữ liệu ảnh nhiệt qua OpenCV: %1")
                .arg(QString::fromStdString(thermalPath.filename().string()))
        };
    }

    return executeThermalMat(
        workerController,
        raw,
        mode,
        requestTarget,
        captureSourcePreview,
        includeVisualOutputs
    );
}

} // namespace

// ---------------------------------------------------------------------------
// SingleEnhanceWorkerThread
// ---------------------------------------------------------------------------

SingleEnhanceWorkerThread::SingleEnhanceWorkerThread(EnhanceJobRequest request, QObject *parent)
    : QThread(parent)
    , request_(std::move(request))
{
    qRegisterMetaType<ThermalEnhancementMetrics>("ThermalEnhancementMetrics");
    qRegisterMetaType<EnhanceJobRequest>("EnhanceJobRequest");
}

SingleEnhanceWorkerThread::~SingleEnhanceWorkerThread()
{
    requestInterruption();
    if (isRunning()) {
        wait();
    }
}

void SingleEnhanceWorkerThread::run()
{
    QElapsedTimer timer;
    timer.start();

    try {
        if (isInterruptionRequested()) {
            return;
        }

        // Dedicated controller instance isolated from the UI thread's controller.
        qart::application::ApplicationController workerController;
        const auto execResult = executeThermalRequest(
            workerController,
            request_.thermalFullPath,
            request_.mode,
            &request_,
            false
        );
        const qint64 elapsedMs = timer.elapsed();

        if (isInterruptionRequested()) {
            return;
        }

        emit jobFinished(request_,
                         execResult.image,
                         elapsedMs,
                         execResult.success,
                         execResult.errorMessage);
    } catch (const std::exception &ex) {
        emit jobFinished(request_,
                         QImage(),
                         timer.elapsed(),
                         false,
                         QString("Lỗi xử lý ảnh nhiệt: %1").arg(ex.what()));
    } catch (...) {
        emit jobFinished(request_,
                         QImage(),
                         timer.elapsed(),
                         false,
                         "Lỗi không xác định trong luồng xử lý ảnh nhiệt.");
    }
}

// ---------------------------------------------------------------------------
// BatchEnhanceWorkerThread
// ---------------------------------------------------------------------------

BatchEnhanceWorkerThread::BatchEnhanceWorkerThread(quint64 sessionId,
                                                   quint64 batchId,
                                                   std::vector<EnhanceJobRequest> requests,
                                                   QObject *parent)
    : QThread(parent)
    , sessionId_(sessionId)
    , batchId_(batchId)
    , requests_(std::move(requests))
{
    qRegisterMetaType<ThermalEnhancementMetrics>("ThermalEnhancementMetrics");
    qRegisterMetaType<EnhanceJobRequest>("EnhanceJobRequest");
}

BatchEnhanceWorkerThread::~BatchEnhanceWorkerThread()
{
    requestCancel();
    requestInterruption();
    if (isRunning()) {
        wait();
    }
}

void BatchEnhanceWorkerThread::setUiBackpressure(
    std::shared_ptr<std::atomic<int>> pendingUiSteps,
    bool throttlePreviewImages,
    int previewIntervalMs,
    int maxPendingUiSteps)
{
    pendingUiSteps_ = std::move(pendingUiSteps);
    throttlePreviewImages_ = throttlePreviewImages;
    previewIntervalMs_ = std::max(0, previewIntervalMs);
    maxPendingUiSteps_ = std::max(1, maxPendingUiSteps);
}

void BatchEnhanceWorkerThread::requestCancel()
{
    cancelRequested_.store(true);
}

void BatchEnhanceWorkerThread::run()
{
    int successCount = 0;
    int errorCount = 0;
    int skippedCount = 0;
    int cancelledCount = 0;
    const int total = static_cast<int>(requests_.size());

    auto emitStep = [&](int idx,
                        EnhanceJobRequest &jobRef,
                        const QImage &img,
                        qint64 elapsed,
                        bool ok,
                        const QString &errMsg) {
        if (pendingUiSteps_) {
            pendingUiSteps_->fetch_add(1, std::memory_order_acq_rel);
        }
        emit jobStepFinished(sessionId_,
                             batchId_,
                             idx,
                             total,
                             jobRef,
                             img,
                             elapsed,
                             ok,
                             errMsg,
                             successCount,
                             errorCount,
                             skippedCount,
                             cancelledCount);
    };

    try {
        // Dedicated controller instance isolated from the UI thread's controller.
        qart::application::ApplicationController workerController;
        QElapsedTimer previewTimer;
        bool hasEmittedPreview = false;

        for (int i = 0; i < total; ++i) {
            if (cancelRequested_.load() || isInterruptionRequested()) {
                cancelledCount = total - i;
                break;
            }

            // Bounded non-blocking backpressure: keep GUI responsive without ever risking deadlock on cancel.
            if (pendingUiSteps_) {
                while (pendingUiSteps_->load(std::memory_order_acquire) >= maxPendingUiSteps_) {
                    if (cancelRequested_.load() || isInterruptionRequested()) {
                        break;
                    }
                    QThread::msleep(2);
                }
                if (cancelRequested_.load() || isInterruptionRequested()) {
                    cancelledCount = total - i;
                    break;
                }
            }

            auto &job = requests_[i];
            QElapsedTimer timer;
            timer.start();

            try {
                // Validate uncached jobs against their captured dataset root.
                if (!job.isSkipped && !job.validationResult.has_value()) {
                    if (!job.pair.has_value()) {
                        throw std::runtime_error(
                            "Cannot validate batch job: dataset pair is missing."
                        );
                    }

                    job.validationResult =
                        qart::application::ApplicationController::validatePairAtRoot(
                            job.datasetRoot,
                            *job.pair
                        );

                    if (!job.validationResult->thermalDecoded()) {
                        job.isSkipped = true;

                        const std::string reason =
                            job.validationResult->message().value_or(
                                std::string(
                                    qart::core::domain::validation::toString(
                                        job.validationResult->status()
                                    )
                                )
                            );

                        job.skipReason =
                            QString("Validation failed: %1")
                                .arg(QString::fromStdString(reason));
                    }
                }

                if (job.isSkipped) {
                    skippedCount++;
                    emitStep(
                        i,
                        job,
                        QImage(),
                        0,
                        false,
                        job.skipReason.isEmpty() ? "Bỏ qua (không đạt điều kiện kiểm định)" : job.skipReason
                    );
                    continue;
                }

                const int pendingNow = pendingUiSteps_
                    ? pendingUiSteps_->load(std::memory_order_acquire)
                    : 0;
                const bool includeVisualOutputs = !throttlePreviewImages_
                    || (i == 0)
                    || (i == total - 1)
                    || !hasEmittedPreview
                    || (previewTimer.elapsed() >= previewIntervalMs_ && pendingNow <= 1);

                const auto execResult = executeThermalRequest(
                    workerController,
                    job.thermalFullPath,
                    job.mode,
                    &job,
                    includeVisualOutputs,
                    includeVisualOutputs
                );
                const qint64 elapsedMs = timer.elapsed();

                if (execResult.success) {
                    successCount++;
                    if (includeVisualOutputs) {
                        hasEmittedPreview = true;
                        previewTimer.restart();
                    }
                } else {
                    errorCount++;
                }

                emitStep(
                    i,
                    job,
                    execResult.image,
                    elapsedMs,
                    execResult.success,
                    execResult.errorMessage
                );
                // Release large transient images in worker vector so long batches keep flat RAM.
                job.sourcePreview = QImage();
                job.diffHeatmap = QImage();
            } catch (const std::exception &ex) {
                errorCount++;
                emitStep(
                    i,
                    job,
                    QImage(),
                    timer.elapsed(),
                    false,
                    QString("Lỗi xử lý: %1").arg(ex.what())
                );
            } catch (...) {
                errorCount++;
                emitStep(
                    i,
                    job,
                    QImage(),
                    timer.elapsed(),
                    false,
                    "Lỗi không xác định khi xử lý mục hàng loạt."
                );
            }
        }
    } catch (...) {
        const int completed = successCount + errorCount + skippedCount;
        if (completed < total) {
            errorCount += (total - completed);
        }
    }

    emit batchFinished(sessionId_, batchId_, total, successCount, errorCount, skippedCount, cancelledCount);
}

// ---------------------------------------------------------------------------
// PreviewWorkerThread
// ---------------------------------------------------------------------------

PreviewWorkerThread::PreviewWorkerThread(PreviewJobRequest request, QObject *parent)
    : QThread(parent)
    , request_(std::move(request))
{
    qRegisterMetaType<ThermalPlaneStats>("ThermalPlaneStats");
    qRegisterMetaType<PreviewJobRequest>("PreviewJobRequest");
    qRegisterMetaType<PreviewJobResult>("PreviewJobResult");
}

PreviewWorkerThread::~PreviewWorkerThread()
{
    requestInterruption();
    if (isRunning()) {
        wait();
    }
}

void PreviewWorkerThread::run()
{
    PreviewJobResult result;
    result.sessionId = request_.sessionId;
    result.requestId = request_.requestId;
    result.itemIndex = request_.itemIndex;
    if (request_.pair.has_value()) {
        result.pairId = QString::fromStdString(request_.pair->pairId());
    }

    try {
        if (isInterruptionRequested()) return;

        if (!request_.pair.has_value()) {
            result.errorMessage = "Thiếu thông tin cặp ảnh trong yêu cầu preview.";
            emit previewFinished(result);
            return;
        }

        if (request_.datasetRoot.empty()) {
            result.errorMessage = "Cannot validate pair: dataset root is empty.";
            emit previewFinished(result);
            return;
        }

        // Validate using the root captured from the GUI session.
        if (request_.validationResult.has_value()) {
            result.validationResult = *request_.validationResult;
        } else {
            result.validationResult =
                qart::application::ApplicationController::validatePairAtRoot(
                    request_.datasetRoot,
                    *request_.pair
                );
        }

        if (isInterruptionRequested()) return;

        // 2. Decode visible image off GUI thread
        const auto visPath = request_.datasetRoot / request_.pair->visibleRelativePath();
        std::error_code ec;
        if (std::filesystem::exists(visPath, ec) && std::filesystem::is_regular_file(visPath, ec)) {
            result.visFileExists = true;
            QImage visImg(QString::fromStdString(visPath.string()));
            if (!visImg.isNull()) {
                result.visDecodeSuccess = true;
                result.visibleImage = std::move(visImg);
            }
        }

        if (isInterruptionRequested()) return;

        // 3. Decode thermal image once off GUI thread
        const auto thrPath = request_.datasetRoot / request_.pair->thermalRelativePath();
        if (std::filesystem::exists(thrPath, ec) && std::filesystem::is_regular_file(thrPath, ec)) {
            result.thrFileExists = true;

            int inspectW = 0;
            int inspectH = 0;
            cv::Mat inspectMat;
            try {
                inspectMat = cv::imread(thrPath.string(), cv::IMREAD_UNCHANGED);
                if (!inspectMat.empty()) {
                    result.irChannels = inspectMat.channels();
                    inspectW = inspectMat.cols;
                    inspectH = inspectMat.rows;

                    QImage thrImg = matToQImageCopy(inspectMat);
                    if (thrImg.isNull()) {
                        thrImg = QImage(QString::fromStdString(thrPath.string()));
                    }
                    if (!thrImg.isNull()) {
                        result.thrDecodeSuccess = true;
                        result.thermalImage = std::move(thrImg);
                    }

                    const cv::Mat prepared = qart::core::enhancement::prepareThermalUint8Plane(inspectMat);
                    result.sourceStats = computePlaneStats(prepared);
                } else {
                    QImage thrImg(QString::fromStdString(thrPath.string()));
                    if (!thrImg.isNull()) {
                        result.thrDecodeSuccess = true;
                        result.thermalImage = std::move(thrImg);
                    }
                }
            } catch (...) {
                // Fallback QImage preview if stats preparation fails (e.g. non-uint8 preview)
                if (!result.thrDecodeSuccess) {
                    QImage thrImg(QString::fromStdString(thrPath.string()));
                    if (!thrImg.isNull()) {
                        result.thrDecodeSuccess = true;
                        result.thermalImage = std::move(thrImg);
                    }
                }
            }

            const int dispW = result.thrDecodeSuccess ? result.thermalImage.width() : inspectW;
            const int dispH = result.thrDecodeSuccess ? result.thermalImage.height() : inspectH;

            // Construct concise modality / channel description for left column
            if (result.irChannels == 3) {
                result.channelDesc = QString("Ảnh gốc: %1 × %2 px | 3 kênh (BGR → Gray8)\n"
                                             "[Giữ nguyên biểu diễn, không tự giải mã bảng màu]")
                                         .arg(dispW)
                                         .arg(dispH);
            } else if (result.irChannels == 1) {
                result.channelDesc = QString("Ảnh gốc: %1 × %2 px | 1 kênh đơn sắc (Gray8)")
                                         .arg(dispW)
                                         .arg(dispH);
            } else {
                result.channelDesc = QString("Ảnh gốc: %1 × %2 px | %3 kênh (Thermal A2)")
                                         .arg(dispW)
                                         .arg(dispH)
                                         .arg(result.irChannels);
            }

            if (request_.rehydrateMode.has_value()
                && result.validationResult.has_value()
                && result.validationResult->thermalDecoded()) {
                QElapsedTimer rehydrateTimer;
                rehydrateTimer.start();
                qart::application::ApplicationController workerController;
                EnhanceJobRequest tempReq;
                tempReq.mode = *request_.rehydrateMode;
                const auto execResult = !inspectMat.empty()
                    ? executeThermalMat(
                        workerController,
                        inspectMat,
                        *request_.rehydrateMode,
                        &tempReq,
                        false,
                        true
                    )
                    : executeThermalRequest(
                        workerController,
                        thrPath,
                        *request_.rehydrateMode,
                        &tempReq,
                        false,
                        true
                    );
                if (execResult.success && !execResult.image.isNull()) {
                    result.rehydratedEnhancement = true;
                    result.rehydratedMode = *request_.rehydrateMode;
                    result.rehydratedEnhancedImage = execResult.image;
                    result.rehydratedDiffHeatmap = std::move(tempReq.diffHeatmap);
                    result.rehydratedMetrics = tempReq.metrics;
                    result.rehydratedAppliedParams = std::move(tempReq.appliedParamsSummary);
                    result.rehydratedElapsedMs = rehydrateTimer.elapsed();
                }
            }
        }

        if (isInterruptionRequested()) return;

        emit previewFinished(result);
    } catch (const std::exception &ex) {
        result.errorMessage = ex.what();
        emit previewFinished(result);
    } catch (...) {
        result.errorMessage = "Lỗi không xác định khi giải mã ảnh preview.";
        emit previewFinished(result);
    }
}

// ---------------------------------------------------------------------------
// FullValidationWorkerThread
// ---------------------------------------------------------------------------

FullValidationWorkerThread::FullValidationWorkerThread(
    quint64 sessionId,
    std::filesystem::path datasetRoot,
    std::vector<qart::core::domain::datasets::DatasetPair> pairs,
    QObject *parent)
    : QThread(parent)
    , sessionId_(sessionId)
    , datasetRoot_(std::move(datasetRoot))
    , pairs_(std::move(pairs))
{
    qRegisterMetaType<FullValidationJobResult>("FullValidationJobResult");
}

FullValidationWorkerThread::~FullValidationWorkerThread()
{
    requestCancel();
    requestInterruption();
    if (isRunning()) {
        wait();
    }
}

void FullValidationWorkerThread::requestCancel()
{
    cancelRequested_.store(true);
}

void FullValidationWorkerThread::run()
{
    FullValidationJobResult out;
    out.sessionId = sessionId_;

    const int total = static_cast<int>(pairs_.size());
    out.results.reserve(pairs_.size());

    QElapsedTimer progressTimer;
    progressTimer.start();

    try {
        for (int i = 0; i < total; ++i) {
            if (cancelRequested_.load() || isInterruptionRequested()) {
                out.cancelled = true;
                break;
            }

            out.results.push_back(
                qart::application::ApplicationController::validatePairAtRoot(
                    datasetRoot_,
                    pairs_[static_cast<std::size_t>(i)]
                )
            );

            const int completed = i + 1;
            if (completed == total
                || (completed % 25 == 0 && progressTimer.elapsed() >= 90)) {
                emit validationProgress(sessionId_, completed, total);
                progressTimer.restart();
            }
        }
    } catch (const std::exception &ex) {
        out.errorMessage = QString::fromUtf8(ex.what());
    } catch (...) {
        out.errorMessage = QStringLiteral("Lỗi không xác định khi kiểm định toàn bộ cặp ảnh.");
    }

    emit validationFinished(out);
}
