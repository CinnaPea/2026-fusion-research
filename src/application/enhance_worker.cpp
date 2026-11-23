#include "enhance_worker.h"

#include <opencv2/imgcodecs.hpp>
#include <QElapsedTimer>
#include <exception>
#include <system_error>

namespace {

struct ThermalExecutionOutput {
    bool success = false;
    QImage image;
    QString errorMessage;
};

ThermalExecutionOutput executeThermalRequest(
    qart::application::ApplicationController &workerController,
    const std::filesystem::path &thermalPath,
    qart::application::ThermalProcessingMode mode)
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

    const auto result = workerController.processThermal(raw, mode);
    const cv::Mat &outMat = result.outputImage;

    if (outMat.empty() || outMat.type() != CV_8UC1) {
        return {
            false,
            QImage(),
            "Kết quả thuật toán rỗng hoặc sai định dạng pixel CV_8UC1."
        };
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
            request_.mode
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

            const auto &job = requests_[i];
            QElapsedTimer timer;
            timer.start();

            try {
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
                    job.mode
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
