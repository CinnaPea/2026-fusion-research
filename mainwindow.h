#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QLabel>
#include <QString>
#include <QMenu>
#include <QPointer>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/manifests/validation_manifest_row.h"
#include "application/application_controller.h"
#include "application/enhance_worker.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct PairItemEntry {
    qart::core::domain::datasets::DatasetPair pair;
    std::optional<qart::core::domain::validation::PairValidationResult> validationResult;
    QString sourceTag; // "ALL", "AUTO", "MANUAL"
};

struct Phase2SourceSelection
{
    QString datasetId;
    QString pairId;
    std::filesystem::path thermalPath;
};

// 5 standardized process statuses
enum class N5UiState {
    NotRun,     // Chưa chạy
    Processing, // Đang xử lý
    Completed,  // Hoàn tất
    Error,      // Lỗi
    Cancelled   // Đã hủy
};

struct Phase2Record {
    QString pairId;
    QString datasetId;
    QString partitionId;
    QString sourceStem;
    QString thermalRelPath;
    QString processingMethod; // "N5", "MSGF", or "BYPASS"
    QString processingVersion; // "N5PostHalf-v1.0", "MSGF-exp-v1.0", or "BYPASS-v1.0"
    QString status; // "SUCCESS", "ERROR", "CANCELLED"
    std::optional<qint64> elapsedMs;
    QString errorMessage;
    std::optional<ThermalEnhancementMetrics> metrics;
};

struct CachedPairEnhancement {
    qart::application::ThermalProcessingMode mode = qart::application::ThermalProcessingMode::Bypass;
    QString methodStr;
    qint64 elapsedMs = -1;
    QString appliedParamsSummary;
    ThermalEnhancementMetrics metrics;
    QImage resultImage;
    QImage diffHeatmap;
    mutable quint64 lastUsedSeq = 0;

    [[nodiscard]] qsizetype imageBytes() const noexcept {
        return resultImage.sizeInBytes() + diffHeatmap.sizeInBytes();
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    [[nodiscard]]
    std::optional<Phase2SourceSelection> selectedPhase2Source() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onBrowseDatasetClicked();
    void onSelectSingleImageClicked();
    void onPairSelected(QListWidgetItem *item);
    void onSearchTextChanged(const QString &query) const;
    void onExportCsvClicked();
    void onExportPhase2Clicked();
    void onDatasetChanged(int index);
    void onPartitionChanged(int index);
    void onViewModeChanged(int index);
    void onSampleCountChanged(int value);
    void onFullValidationClicked();
    void onAddToQueueClicked();
    void onClearQueueClicked();
    void onToggleCollapseClicked();

    // Tab and N5 enhancement slots
    void onWorkspaceTabChanged(int index);
    void onApplyN5Clicked();
    void onEnhanceAllClicked();
    void onResetN5Clicked();
    void onAlgorithmParamChanged();
    void onResetAlgorithmParamsClicked();

    // Worker thread response slots
    void onSingleWorkerFinished(const EnhanceJobRequest &request,
                                const QImage &resultImage,
                                qint64 elapsedMs,
                                bool success,
                                const QString &errorMessage);

    void onBatchStepFinished(quint64 sessionId,
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

    void onBatchWorkerFinished(quint64 sessionId,
                               quint64 batchId,
                               int totalCount,
                               int successCount,
                               int errorCount,
                               int skippedCount,
                               int cancelledCount);

    void onPreviewFinished(const PreviewJobResult &result);

private:
    void setupUiControls();
    void applyThemeAwareStyles();
    void loadDatasetFromDirectory(const QString &dirPath);
    void applyFiltersAndModes();
    void displayPair(const PairItemEntry &entry);
    void updateListItemDisplay(QListWidgetItem *listItem, const PairItemEntry &entry) const;
    void clearTab1PreviewUi();

    // N5 / MSGF / BYPASS enhancement helpers
    [[nodiscard]] qart::application::ThermalProcessingMode selectedThermalProcessingMode() const;
    [[nodiscard]] static QString thermalModeDisplayName(qart::application::ThermalProcessingMode mode);
    [[nodiscard]] static QString thermalModeVersion(const QString &methodStr);
    void setN5UiState(N5UiState state, qint64 elapsedMs = 0, const QString &detailMsg = QString(), const QString &methodOverride = QString());
    void renderCachedN5Images();
    void clearN5EnhancedCache();
    void updateEnhancedResultDisplay(const EnhanceJobRequest &request,
                                     const QImage &resultImage,
                                     qint64 elapsedMs,
                                     const QString &methodStr);
    void storePairEnhancementResult(const EnhanceJobRequest &request,
                                    const QImage &resultImage,
                                    qint64 elapsedMs,
                                    const QString &methodStr);
    void trimPairEnhancementPixelCache(const std::string &protectedPairId);
    void clearAllPairEnhancementCache();
    void removePairEnhancementCache(const QString &pairId);
    void refreshPairListDisplayById(const QString &pairId, int hintIndex = -1);
    [[nodiscard]]
    const CachedPairEnhancement *findPairEnhancement(const QString &pairId) const;
    void displayCachedPairEnhancement(const CachedPairEnhancement &cached);
    [[nodiscard]]
    static QString formatOriginalMetricsSummary(const QString &baseInfo,
                                                  const ThermalPlaneStats &stats);
    [[nodiscard]]
    static QString formatEnhancementMetricsSummary(const QString &methodStr,
                                                   qint64 elapsedMs,
                                                   const QString &appliedParams,
                                                   const ThermalEnhancementMetrics &metrics,
                                                   bool showingDiffMap);
    [[nodiscard]]
    static QString formatEnhancementLogMetrics(const ThermalEnhancementMetrics &metrics);
    void appendLog(const QString &message);
    void updateBatchButtonText();

    // Thread lifecycle & invalidation helpers
    void retireSingleWorker();
    void retireBatchWorker();
    void retirePreviewWorker();
    void pruneFinishedRetiredWorkers();
    void stopAllWorkersAndWait();
    void invalidateActiveSessionAndWorkers(bool clearOriginalViewer = true);
    void renderImageFromMemory(QLabel *label, const QImage &image);

    [[nodiscard]]
    static QString getStatusTagText(qart::core::domain::validation::PairValidationStatus status);

    [[nodiscard]]
    static QString getStatusColorHex(qart::core::domain::validation::PairValidationStatus status);

    [[nodiscard]]
    static QString getStatusDescription(qart::core::domain::validation::PairValidationStatus status);

    Ui::MainWindow *ui;

    // Currently filtered and displayed item entries
    std::vector<PairItemEntry> displayedItems_;

    bool isUpdatingControls_ = false;
    bool isSyncingParams_ = false;

    qart::application::ApplicationController appController_;

    // N5 single-image state & caching members (decoupled from batch state)
    N5UiState n5State_ = N5UiState::NotRun;
    bool activeThermalValid_ = false;
    bool isBatchEnhancing_ = false;
    bool isBatchCancelling_ = false;
    int batchTotalCount_ = 0;
    QPixmap cachedN5OriginalPixmap_;
    QPixmap cachedN5EnhancedPixmap_;
    QPixmap cachedN5DiffPixmap_;
    QString activeN5OriginalBaseInfo_;
    ThermalPlaneStats activeN5OriginalStats_;
    ThermalEnhancementMetrics lastThermalMetrics_;
    QString lastAppliedParams_;
    qint64 lastElapsedMs_ = -1;
    QString lastMethodStr_;
    QString activeN5PairId_;

    // Threading
    QPointer<SingleEnhanceWorkerThread> singleWorker_;
    QPointer<BatchEnhanceWorkerThread> batchWorker_;
    QPointer<PreviewWorkerThread> previewWorker_;
    std::vector<QPointer<QThread>> pendingRetiredWorkers_;
    quint64 currentSessionId_ = 0;
    quint64 currentRequestId_ = 0;
    quint64 currentBatchId_ = 0;
    quint64 currentPreviewRequestId_ = 0;

    // In-memory decoded preview cache
    int activePreviewIndex_ = -1;
    int cachedPreviewIndex_ = -1;
    QImage cachedVisibleImage_;
    QImage cachedThermalImage_;

    // Storage for Phase 2 results
    std::vector<Phase2Record> phase2Results_;
    std::unordered_map<std::string, CachedPairEnhancement> pairEnhancementCache_;
    mutable quint64 cacheAccessSeq_ = 0;
    qsizetype cachedEnhancementPixelBytes_ = 0;
    static constexpr qsizetype kMaxEnhancementCacheBytes = 192LL * 1024 * 1024;
};
#endif // MAINWINDOW_H
