#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QLabel>
#include <QString>
#include <QMenu>
#include <filesystem>
#include <optional>
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
    QString processingMethod; // "N5" or "BYPASS"
    QString processingVersion; // "N5PostHalf-v1.0" or "BYPASS-v1.0"
    QString status; // "SUCCESS", "ERROR", "CANCELLED"
    std::optional<qint64> elapsedMs;
    QString errorMessage;
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
    void resizeEvent(QResizeEvent *event) override;

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

    // Worker thread response slots
    void onSingleWorkerFinished(const QString &pairId,
                                const QImage &resultImage,
                                qint64 elapsedMs,
                                bool success,
                                const QString &errorMessage,
                                int mode);

    void onBatchStepFinished(int index,
                             const QString &pairId,
                             const QImage &resultImage,
                             qint64 elapsedMs,
                             bool success,
                             const QString &errorMessage,
                             int mode,
                             int successCount,
                             int errorCount,
                             int cancelledCount);

    void onBatchWorkerFinished(int successCount, int errorCount, int cancelledCount);

private:
    void setupUiControls();
    void loadDatasetFromDirectory(const QString &dirPath);
    void applyFiltersAndModes();
    void displayPair(const PairItemEntry &entry);
    void renderImageToLabel(QLabel *label, const QString &imagePath, const QString &modality);

    // N5 enhancement helpers
    void updateQualityN5Tab(const PairItemEntry &entry);
    void setN5UiState(N5UiState state, qint64 elapsedMs = 0, const QString &detailMsg = QString());
    void renderCachedN5Images();
    void appendLog(const QString &message);

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

    qart::application::ApplicationController appController_;

    // N5 state & caching members
    N5UiState n5State_ = N5UiState::NotRun;
    bool isBatchEnhancing_ = false;
    QPixmap cachedN5OriginalPixmap_;
    QPixmap cachedN5EnhancedPixmap_;
    QString activeN5PairId_;

    // Threading
    SingleEnhanceWorkerThread *singleWorker_ = nullptr;
    BatchEnhanceWorkerThread *batchWorker_ = nullptr;

    // Storage for Phase 2 results
    std::vector<Phase2Record> phase2Results_;
};
#endif // MAINWINDOW_H
