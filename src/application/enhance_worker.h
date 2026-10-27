#ifndef ENHANCE_WORKER_H
#define ENHANCE_WORKER_H

#include <QThread>
#include <QImage>
#include <QString>
#include <filesystem>
#include <vector>
#include <atomic>
#include "application/application_controller.h"

struct EnhanceJobRequest {
    QString pairId;
    std::filesystem::path thermalFullPath;
    qart::application::ThermalProcessingMode mode = qart::application::ThermalProcessingMode::N5PostHalf;
};

// Worker thread for single-pair processing
class SingleEnhanceWorkerThread : public QThread
{
    Q_OBJECT

public:
    explicit SingleEnhanceWorkerThread(EnhanceJobRequest request, QObject *parent = nullptr);
    ~SingleEnhanceWorkerThread() override;

signals:
    void jobFinished(const QString &pairId,
                     const QImage &resultImage,
                     qint64 elapsedMs,
                     bool success,
                     const QString &errorMessage,
                     int mode);

protected:
    void run() override;

private:
    EnhanceJobRequest request_;
};

// Worker thread for batch processing with safe cancellation
class BatchEnhanceWorkerThread : public QThread
{
    Q_OBJECT

public:
    explicit BatchEnhanceWorkerThread(std::vector<EnhanceJobRequest> requests, QObject *parent = nullptr);
    ~BatchEnhanceWorkerThread() override;

    void requestCancel();
    [[nodiscard]] bool isCancelRequested() const { return cancelRequested_.load(); }

signals:
    void jobStepFinished(int index,
                         const QString &pairId,
                         const QImage &resultImage,
                         qint64 elapsedMs,
                         bool success,
                         const QString &errorMessage,
                         int mode,
                         int successCount,
                         int errorCount,
                         int cancelledCount);

    void batchFinished(int successCount, int errorCount, int cancelledCount);

protected:
    void run() override;

private:
    std::vector<EnhanceJobRequest> requests_;
    std::atomic<bool> cancelRequested_{false};
};

#endif // ENHANCE_WORKER_H
