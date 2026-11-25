#ifndef ENHANCE_WORKER_H
#define ENHANCE_WORKER_H

#include <QThread>
#include <QImage>
#include <QMetaType>
#include <QString>
#include <filesystem>
#include <vector>
#include <atomic>
#include <memory>
#include <optional>
#include "application/application_controller.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_result.h"

struct ThermalPlaneStats {
    bool valid = false;
    double mean = 0.0;
    double stdDev = 0.0;
    double entropy = 0.0;
    double edgeGradient = 0.0;
    double piqe = 0.0;
    int minVal = 0;
    int maxVal = 0;
};
Q_DECLARE_METATYPE(ThermalPlaneStats)

struct ThermalEnhancementMetrics {
    bool valid = false;
    double inputMean = 0.0;
    double outputMean = 0.0;
    double inputStd = 0.0;
    double outputStd = 0.0;
    double inputEntropy = 0.0;
    double outputEntropy = 0.0;
    double inputEdgeGrad = 0.0;
    double outputEdgeGrad = 0.0;
    double inputPiqe = 0.0;
    double outputPiqe = 0.0;
    int inputMin = 0;
    int inputMax = 0;
    int outputMin = 0;
    int outputMax = 0;
    double psnr = 0.0;
    double mae = 0.0;
    int maxAbsDiff = 0;
    double changedPixelPercent = 0.0;
};
Q_DECLARE_METATYPE(ThermalEnhancementMetrics)

struct EnhanceJobRequest {
    quint64 sessionId = 0;
    quint64 requestId = 0;
    QString pairId;
    QString datasetId;
    std::filesystem::path thermalFullPath;
    std::filesystem::path datasetRoot;
    qart::application::ThermalProcessingMode mode = qart::application::ThermalProcessingMode::Bypass;
    bool isSkipped = false;
    QString skipReason;
    std::optional<qart::core::domain::datasets::DatasetPair> pair;
    std::optional<qart::core::domain::validation::PairValidationResult> validationResult;
    QImage sourcePreview;
    int sourceChannels = 0;
    ThermalEnhancementMetrics metrics;
    QImage diffHeatmap;
    QString appliedParamsSummary;
};
Q_DECLARE_METATYPE(EnhanceJobRequest)

// Request & Result for asynchronous preview image decoding (off GUI thread)
struct PreviewJobRequest {
    quint64 sessionId = 0;
    quint64 requestId = 0;
    int itemIndex = -1;
    std::optional<qart::core::domain::datasets::DatasetPair> pair;
    std::optional<qart::core::domain::validation::PairValidationResult> validationResult;
    std::filesystem::path datasetRoot;
    std::optional<qart::application::ThermalProcessingMode> rehydrateMode;
};
Q_DECLARE_METATYPE(PreviewJobRequest)

struct PreviewJobResult {
    quint64 sessionId = 0;
    quint64 requestId = 0;
    int itemIndex = -1;
    QString pairId;
    std::optional<qart::core::domain::validation::PairValidationResult> validationResult;
    QImage visibleImage;
    QImage thermalImage;
    bool visFileExists = false;
    bool thrFileExists = false;
    bool visDecodeSuccess = false;
    bool thrDecodeSuccess = false;
    int irChannels = 0;
    ThermalPlaneStats sourceStats;
    QString channelDesc;
    QString errorMessage;
    bool rehydratedEnhancement = false;
    qart::application::ThermalProcessingMode rehydratedMode = qart::application::ThermalProcessingMode::Bypass;
    QImage rehydratedEnhancedImage;
    QImage rehydratedDiffHeatmap;
    ThermalEnhancementMetrics rehydratedMetrics;
    QString rehydratedAppliedParams;
    qint64 rehydratedElapsedMs = -1;
};
Q_DECLARE_METATYPE(PreviewJobResult)

// Worker thread for background preview loading (no GUI blocking)
class PreviewWorkerThread : public QThread
{
    Q_OBJECT

public:
    explicit PreviewWorkerThread(PreviewJobRequest request, QObject *parent = nullptr);
    ~PreviewWorkerThread() override;

    [[nodiscard]] quint64 sessionId() const noexcept { return request_.sessionId; }
    [[nodiscard]] quint64 requestId() const noexcept { return request_.requestId; }

signals:
    void previewFinished(const PreviewJobResult &result);

protected:
    void run() override;

private:
    PreviewJobRequest request_;
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
    explicit BatchEnhanceWorkerThread(quint64 sessionId,
                                      quint64 batchId,
                                      std::vector<EnhanceJobRequest> requests,
                                      QObject *parent = nullptr);
    ~BatchEnhanceWorkerThread() override;

    void setUiBackpressure(std::shared_ptr<std::atomic<int>> pendingUiSteps,
                           bool throttlePreviewImages,
                           int previewIntervalMs = 150,
                           int maxPendingUiSteps = 4);

    void requestCancel();
    [[nodiscard]] bool isCancelRequested() const { return cancelRequested_.load(); }
    [[nodiscard]] quint64 sessionId() const noexcept { return sessionId_; }
    [[nodiscard]] quint64 batchId() const noexcept { return batchId_; }

signals:
    void jobStepFinished(quint64 sessionId,
                         quint64 batchId,
                         int index,
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

    void batchFinished(quint64 sessionId,
                       quint64 batchId,
                       int totalCount,
                       int successCount,
                       int errorCount,
                       int skippedCount,
                       int cancelledCount);

protected:
    void run() override;

private:
    quint64 sessionId_ = 0;
    quint64 batchId_ = 0;
    std::vector<EnhanceJobRequest> requests_;
    std::atomic<bool> cancelRequested_{false};
    std::shared_ptr<std::atomic<int>> pendingUiSteps_;
    bool throttlePreviewImages_ = false;
    int previewIntervalMs_ = 150;
    int maxPendingUiSteps_ = 4;
};

struct FullValidationJobResult {
    quint64 sessionId = 0;
    std::vector<qart::core::domain::validation::PairValidationResult> results;
    bool cancelled = false;
    QString errorMessage;
};
Q_DECLARE_METATYPE(FullValidationJobResult)

// Worker thread for full-dataset validation off the GUI thread
class FullValidationWorkerThread : public QThread
{
    Q_OBJECT

public:
    explicit FullValidationWorkerThread(
        quint64 sessionId,
        std::filesystem::path datasetRoot,
        std::vector<qart::core::domain::datasets::DatasetPair> pairs,
        QObject *parent = nullptr);
    ~FullValidationWorkerThread() override;

    void requestCancel();
    [[nodiscard]] quint64 sessionId() const noexcept { return sessionId_; }

signals:
    void validationProgress(quint64 sessionId, int completed, int total);
    void validationFinished(const FullValidationJobResult &result);

protected:
    void run() override;

private:
    quint64 sessionId_ = 0;
    std::filesystem::path datasetRoot_;
    std::vector<qart::core::domain::datasets::DatasetPair> pairs_;
    std::atomic<bool> cancelRequested_{false};
};

#endif // ENHANCE_WORKER_H
