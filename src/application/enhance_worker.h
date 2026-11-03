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
    quint64 sessionId = 0;
    quint64 requestId = 0;
    QString pairId;
    QString datasetId;
    std::filesystem::path thermalFullPath;
    qart::application::ThermalProcessingMode mode = qart::application::ThermalProcessingMode::Bypass;
    bool isSkipped = false;
    QString skipReason;
};

// Worker thread for single-pair processing
class SingleEnhanceWorkerThread : public QThread
{
    Q_OBJECT

public:
    explicit SingleEnhanceWorkerThread(EnhanceJobRequest request, QObject *parent = nullptr);
    ~SingleEnhanceWorkerThread() override;

signals:
    void jobFinished(const EnhanceJobRequest &request,
                     const QImage &resultImage,
                     qint64 elapsedMs,
                     bool success,
                     const QString &errorMessage);

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
                         int totalCount,
                         const EnhanceJobRequest &request,
                         const QImage &resultImage,
                         qint64 elapsedMs,
                         bool success,
                         const QString &errorMessage,
                         int successCount,
                         int errorCount,
                         int skippedCount,
                         int cancelledCount);

    void batchFinished(int totalCount,
                       int successCount,
                       int errorCount,
                       int skippedCount,
                       int cancelledCount);

protected:
    void run() override;

private:
    std::vector<EnhanceJobRequest> requests_;
    std::atomic<bool> cancelRequested_{false};
};

#endif // ENHANCE_WORKER_H
