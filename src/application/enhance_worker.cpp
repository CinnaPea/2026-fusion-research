#include "enhance_worker.h"
#include "core/enhancement/thermal_input.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <QElapsedTimer>
#include <algorithm>
#include <array>
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

ThermalPlaneStats computePlaneStats(const cv::Mat &gray8)
{
    ThermalPlaneStats stats;
    if (gray8.empty() || gray8.type() != CV_8UC1) {
        return stats;
    }

    stats.valid = true;

    cv::Scalar meanScalar;
    cv::Scalar stdScalar;
    cv::meanStdDev(gray8, meanScalar, stdScalar);
    stats.mean = meanScalar[0];
    stats.stdDev = stdScalar[0];

    double minV = 0.0;
    double maxV = 0.0;
    cv::minMaxLoc(gray8, &minV, &maxV);
    stats.minVal = static_cast<int>(std::round(minV));
    stats.maxVal = static_cast<int>(std::round(maxV));

    // Exact 256-bin Shannon entropy (bits/pixel)
    std::array<std::size_t, 256> hist{};
    for (int r = 0; r < gray8.rows; ++r) {
        const auto *rowPtr = gray8.ptr<unsigned char>(r);
        for (int c = 0; c < gray8.cols; ++c) {
            hist[rowPtr[c]]++;
        }
    }
    const double totalPx = static_cast<double>(gray8.total());
    if (totalPx > 0.0) {
        double entropy = 0.0;
        for (std::size_t count : hist) {
            if (count > 0) {
                const double p = static_cast<double>(count) / totalPx;
                entropy -= p * std::log2(p);
            }
        }
        stats.entropy = entropy;
    }

    // Mean Sobel gradient magnitude normalized by 4.0 (0..255 edge strength scale)
    cv::Mat gx;
    cv::Mat gy;
    cv::Sobel(gray8, gx, CV_32F, 1, 0, 3, 1.0, 0.0, cv::BORDER_REFLECT_101);
    cv::Sobel(gray8, gy, CV_32F, 0, 1, 3, 1.0, 0.0, cv::BORDER_REFLECT_101);
    cv::Mat mag;
    cv::magnitude(gx, gy, mag);
    stats.edgeGradient = cv::mean(mag)[0] / 4.0;

    return stats;
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

void populateThermalMetricsAndDiff(
    const cv::Mat &prepared,
    const cv::Mat &output,
    qart::application::ThermalProcessingMode mode,
    EnhanceJobRequest &target)
{
    target.appliedParamsSummary = (mode == qart::application::ThermalProcessingMode::N5PostHalf)
                                      ? QString("GF Base 7×7 | CLAHE(2.0, 8×8) | Post-Half 3×3 (×0.5)")
                                      : QString("BYPASS (Giữ nguyên đầu vào grayscale)");

    if (prepared.empty() || output.empty() || prepared.size() != output.size()) {
        target.metrics = ThermalEnhancementMetrics{};
        target.diffHeatmap = QImage();
        return;
    }

    const ThermalPlaneStats inStats = computePlaneStats(prepared);
    const ThermalPlaneStats outStats = computePlaneStats(output);

    cv::Mat absDiff;
    cv::absdiff(output, prepared, absDiff);

    double maxDiffD = 0.0;
    cv::minMaxLoc(absDiff, nullptr, &maxDiffD);
    const int maxAbsDiff = static_cast<int>(std::round(maxDiffD));
    const double mae = cv::mean(absDiff)[0];
    const double totalPx = static_cast<double>(absDiff.total());
    const double changedPct = (totalPx > 0.0)
                                  ? (100.0 * static_cast<double>(cv::countNonZero(absDiff)) / totalPx)
                                  : 0.0;

    ThermalEnhancementMetrics m;
    m.valid = true;
    m.inputMean = inStats.mean;
    m.outputMean = outStats.mean;
    m.inputStd = inStats.stdDev;
    m.outputStd = outStats.stdDev;
    m.inputEntropy = inStats.entropy;
    m.outputEntropy = outStats.entropy;
    m.inputEdgeGrad = inStats.edgeGradient;
    m.outputEdgeGrad = outStats.edgeGradient;
    m.inputMin = inStats.minVal;
    m.inputMax = inStats.maxVal;
    m.outputMin = outStats.minVal;
    m.outputMax = outStats.maxVal;
    m.mae = mae;
    m.maxAbsDiff = maxAbsDiff;
    m.changedPixelPercent = changedPct;

    target.metrics = m;
    target.diffHeatmap = buildDiffHeatmap(absDiff, maxAbsDiff);
}

ThermalExecutionOutput executeThermalRequest(
    qart::application::ApplicationController &workerController,
    const std::filesystem::path &thermalPath,
    qart::application::ThermalProcessingMode mode,
    EnhanceJobRequest *requestTarget = nullptr,
    bool captureSourcePreview = false)
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

    // Use the already-decoded source. Never create QPixmap in this thread.
    if (requestTarget != nullptr && captureSourcePreview) {
        requestTarget->sourcePreview = QImage();
        requestTarget->sourceChannels = raw.channels();

        if (raw.type() == CV_8UC1) {
            requestTarget->sourcePreview =
                QImage(
                    raw.data,
                    raw.cols,
                    raw.rows,
                    static_cast<int>(raw.step),
                    QImage::Format_Grayscale8
                ).copy();
        } else if (raw.type() == CV_8UC3) {
            cv::Mat rgb;
            cv::cvtColor(raw, rgb, cv::COLOR_BGR2RGB);

            requestTarget->sourcePreview =
                QImage(
                    rgb.data,
                    rgb.cols,
                    rgb.rows,
                    rgb.step,
                    QImage::Format_RGB888
                ).copy();
        } else if (raw.type() == CV_8UC4) {
            cv::Mat rgba;
            cv::cvtColor(raw, rgba, cv::COLOR_BGRA2RGBA);

            requestTarget->sourcePreview =
                QImage(
                    rgba.data,
                    rgba.cols,
                    rgba.rows,
                    rgba.step,
                    QImage::Format_RGBA8888
                ).copy();
        }
    }

    const auto result = workerController.processThermal(raw, mode);
    const cv::Mat &outMat = result.outputImage;

    if (outMat.empty() || outMat.type() != CV_8UC1) {
        return {
            false,
            QImage(),
            "Kết quả thuật toán rỗng hoặc sai định dạng pixel CV_8UC1."
        };
    }

    if (requestTarget != nullptr) {
        const cv::Mat prepared = qart::core::enhancement::prepareThermalUint8Plane(raw);
        populateThermalMetricsAndDiff(prepared, outMat, mode, *requestTarget);
    }

    QImage qImg(outMat.data,
                outMat.cols,
                outMat.rows,
                static_cast<int>(outMat.step),
                QImage::Format_Grayscale8);
    return {true, qImg.copy(), QString()};
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

    try {
        // Dedicated controller instance isolated from the UI thread's controller.
        qart::application::ApplicationController workerController;

        for (int i = 0; i < total; ++i) {
            if (cancelRequested_.load() || isInterruptionRequested()) {
                cancelledCount = total - i;
                break;
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
                    emit jobStepFinished(sessionId_,
                                         batchId_,
                                         i,
                                         total,
                                         job,
                                         QImage(),
                                         0,
                                         false,
                                         job.skipReason.isEmpty() ? "Bỏ qua (không đạt điều kiện kiểm định)" : job.skipReason,
                                         successCount,
                                         errorCount,
                                         skippedCount,
                                         cancelledCount);
                    continue;
                }

                const auto execResult = executeThermalRequest(
                    workerController,
                    job.thermalFullPath,
                    job.mode,
                    &job,
                    true
                );
                const qint64 elapsedMs = timer.elapsed();

                if (execResult.success) {
                    successCount++;
                } else {
                    errorCount++;
                }

                emit jobStepFinished(sessionId_,
                                     batchId_,
                                     i,
                                     total,
                                     job,
                                     execResult.image,
                                     elapsedMs,
                                     execResult.success,
                                     execResult.errorMessage,
                                     successCount,
                                     errorCount,
                                     skippedCount,
                                     cancelledCount);
                // Release large transient images in worker vector so long batches keep flat RAM.
                job.sourcePreview = QImage();
                job.diffHeatmap = QImage();
            } catch (const std::exception &ex) {
                errorCount++;
                emit jobStepFinished(sessionId_,
                                     batchId_,
                                     i,
                                     total,
                                     job,
                                     QImage(),
                                     timer.elapsed(),
                                     false,
                                     QString("Lỗi xử lý: %1").arg(ex.what()),
                                     successCount,
                                     errorCount,
                                     skippedCount,
                                     cancelledCount);
            } catch (...) {
                errorCount++;
                emit jobStepFinished(sessionId_,
                                     batchId_,
                                     i,
                                     total,
                                     job,
                                     QImage(),
                                     timer.elapsed(),
                                     false,
                                     "Lỗi không xác định khi xử lý mục hàng loạt.",
                                     successCount,
                                     errorCount,
                                     skippedCount,
                                     cancelledCount);
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
    result.enhancementRequestId = request_.enhancementRequestId;
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

        // 3. Decode thermal image off GUI thread
        const auto thrPath = request_.datasetRoot / request_.pair->thermalRelativePath();
        if (std::filesystem::exists(thrPath, ec) && std::filesystem::is_regular_file(thrPath, ec)) {
            result.thrFileExists = true;
            QImage thrImg(QString::fromStdString(thrPath.string()));
            if (!thrImg.isNull()) {
                result.thrDecodeSuccess = true;
                result.thermalImage = std::move(thrImg);
            }

            // Inspect channels, dimensions, and baseline thermal stats off GUI thread
            int inspectW = 0;
            int inspectH = 0;
            try {
                const cv::Mat inspectMat = cv::imread(thrPath.string(), cv::IMREAD_UNCHANGED);
                if (!inspectMat.empty()) {
                    result.irChannels = inspectMat.channels();
                    inspectW = inspectMat.cols;
                    inspectH = inspectMat.rows;
                    const cv::Mat prepared = qart::core::enhancement::prepareThermalUint8Plane(inspectMat);
                    result.sourceStats = computePlaneStats(prepared);
                }
            } catch (...) {
                // Keep inspected channel count if stats preparation fails (e.g. non-uint8 preview)
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
                && result.validationResult->thermalDecoded())
            {
                QElapsedTimer rehydrateTimer;
                rehydrateTimer.start();

                try {
                    qart::application::ApplicationController workerController;

                    EnhanceJobRequest tempReq;
                    tempReq.mode = *request_.rehydrateMode;

                    const auto execResult = executeThermalRequest(
                        workerController,
                        thrPath,
                        *request_.rehydrateMode,
                        &tempReq,
                        false
                    );

                    if (execResult.success && !execResult.image.isNull()) {
                        result.rehydratedEnhancement = true;
                        result.rehydratedMode = *request_.rehydrateMode;
                        result.rehydratedEnhancedImage = execResult.image;
                        result.rehydratedDiffHeatmap =
                            std::move(tempReq.diffHeatmap);
                        result.rehydratedMetrics = tempReq.metrics;
                        result.rehydratedAppliedParams =
                            std::move(tempReq.appliedParamsSummary);
                    } else {
                        result.rehydrationError =
                            execResult.errorMessage.isEmpty()
                                ? QStringLiteral(
                                      "Không thể tái tạo kết quả xử lý.")
                                : execResult.errorMessage;
                    }
                } catch (const std::exception &ex) {
                    result.rehydrationError =
                        QString("Lỗi tái tạo kết quả: %1")
                            .arg(QString::fromUtf8(ex.what()));
                } catch (...) {
                    result.rehydrationError =
                        QStringLiteral(
                            "Lỗi không xác định khi tái tạo kết quả.");
                }

                result.rehydratedElapsedMs = rehydrateTimer.elapsed();
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
