#include "enhance_worker.h"

#include <opencv2/imgcodecs.hpp>
#include <QElapsedTimer>
#include "core/enhancement/n5_thermal_enhancer.h"
#include "core/enhancement/thermal_input.h"

// ---------------------------------------------------------------------------
// SingleEnhanceWorkerThread
// ---------------------------------------------------------------------------

SingleEnhanceWorkerThread::SingleEnhanceWorkerThread(EnhanceJobRequest request, QObject *parent)
    : QThread(parent)
    , request_(std::move(request))
{
}

SingleEnhanceWorkerThread::~SingleEnhanceWorkerThread()
{
    if (isRunning()) {
        requestInterruption();
        wait(2000);
    }
}

void SingleEnhanceWorkerThread::run()
{
    QElapsedTimer timer;
    timer.start();

    if (isInterruptionRequested()) {
        return;
    }

    if (!std::filesystem::exists(request_.thermalFullPath)) {
        emit jobFinished(request_,
                         QImage(),
                         0,
                         false,
                         QString("Không tìm thấy file ảnh nhiệt trên đĩa: %1")
                             .arg(QString::fromStdString(request_.thermalFullPath.string())));
        return;
    }

    cv::Mat raw = cv::imread(request_.thermalFullPath.string(), cv::IMREAD_UNCHANGED);
    if (raw.empty()) {
        emit jobFinished(request_,
                         QImage(),
                         0,
                         false,
                         QString("Không thể giải mã dữ liệu ảnh nhiệt qua OpenCV: %1")
                             .arg(QString::fromStdString(request_.thermalFullPath.filename().string())));
        return;
    }

    if (isInterruptionRequested()) {
        return;
    }

    try {
        cv::Mat prepared = qart::core::enhancement::prepareThermalUint8Plane(raw);
        cv::Mat outMat;

        if (request_.mode == qart::application::ThermalProcessingMode::Bypass) {
            outMat = qart::core::enhancement::bypassThermal(prepared);
        } else {
            // Dedicated enhancer instance per worker execution for absolute thread safety
            qart::core::enhancement::N5ThermalEnhancer enhancer;
            auto res = enhancer.enhance(prepared);
            outMat = std::move(res.outputImage);
        }

        const qint64 elapsedMs = timer.elapsed();

        if (isInterruptionRequested()) {
            return;
        }

        if (outMat.empty() || outMat.type() != CV_8UC1) {
            emit jobFinished(request_,
                             QImage(),
                             elapsedMs,
                             false,
                             "Kết quả thuật toán rỗng hoặc sai định dạng pixel CV_8UC1.");
            return;
        }

        QImage qImg(outMat.data, outMat.cols, outMat.rows, static_cast<int>(outMat.step), QImage::Format_Grayscale8);
        emit jobFinished(request_,
                         qImg.copy(),
                         elapsedMs,
                         true,
                         QString());
    } catch (const std::exception &ex) {
        emit jobFinished(request_,
                         QImage(),
                         timer.elapsed(),
                         false,
                         QString("Lỗi xử lý thuật toán: %1").arg(ex.what()));
    }
}

// ---------------------------------------------------------------------------
// BatchEnhanceWorkerThread
// ---------------------------------------------------------------------------

BatchEnhanceWorkerThread::BatchEnhanceWorkerThread(std::vector<EnhanceJobRequest> requests, QObject *parent)
    : QThread(parent)
    , requests_(std::move(requests))
{
}

BatchEnhanceWorkerThread::~BatchEnhanceWorkerThread()
{
    requestCancel();
    if (isRunning()) {
        wait(3000);
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

    // Single persistent enhancer instance for the sequential batch run
    qart::core::enhancement::N5ThermalEnhancer enhancer;

    for (int i = 0; i < total; ++i) {
        if (cancelRequested_.load()) {
            cancelledCount = total - i;
            break;
        }

        const auto &job = requests_[i];
        QElapsedTimer timer;
        timer.start();

        if (job.isSkipped) {
            skippedCount++;
            emit jobStepFinished(i,
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

        if (!std::filesystem::exists(job.thermalFullPath)) {
            errorCount++;
            emit jobStepFinished(i,
                                 total,
                                 job,
                                 QImage(),
                                 0,
                                 false,
                                 QString("Không tìm thấy file: %1").arg(QString::fromStdString(job.thermalFullPath.filename().string())),
                                 successCount,
                                 errorCount,
                                 skippedCount,
                                 cancelledCount);
            continue;
        }

        cv::Mat raw = cv::imread(job.thermalFullPath.string(), cv::IMREAD_UNCHANGED);
        if (raw.empty()) {
            errorCount++;
            emit jobStepFinished(i,
                                 total,
                                 job,
                                 QImage(),
                                 0,
                                 false,
                                 QString("Không thể giải mã ảnh: %1").arg(QString::fromStdString(job.thermalFullPath.filename().string())),
                                 successCount,
                                 errorCount,
                                 skippedCount,
                                 cancelledCount);
            continue;
        }

        try {
            cv::Mat prepared = qart::core::enhancement::prepareThermalUint8Plane(raw);
            cv::Mat outMat;

            if (job.mode == qart::application::ThermalProcessingMode::Bypass) {
                outMat = qart::core::enhancement::bypassThermal(prepared);
            } else {
                auto res = enhancer.enhance(prepared);
                outMat = std::move(res.outputImage);
            }

            const qint64 elapsedMs = timer.elapsed();

            if (outMat.empty() || outMat.type() != CV_8UC1) {
                errorCount++;
                emit jobStepFinished(i,
                                     total,
                                     job,
                                     QImage(),
                                     elapsedMs,
                                     false,
                                     "Kết quả thuật toán rỗng.",
                                     successCount,
                                     errorCount,
                                     skippedCount,
                                     cancelledCount);
            } else {
                successCount++;
                QImage qImg(outMat.data, outMat.cols, outMat.rows, static_cast<int>(outMat.step), QImage::Format_Grayscale8);
                emit jobStepFinished(i,
                                     total,
                                     job,
                                     qImg.copy(),
                                     elapsedMs,
                                     true,
                                     QString(),
                                     successCount,
                                     errorCount,
                                     skippedCount,
                                     cancelledCount);
            }
        } catch (const std::exception &ex) {
            errorCount++;
            emit jobStepFinished(i,
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
        }
    }

    emit batchFinished(total, successCount, errorCount, skippedCount, cancelledCount);
}
