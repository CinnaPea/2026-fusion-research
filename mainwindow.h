#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QLabel>
#include <QString>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_policy.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/configuration/dataset_configuration.h"
#include "core/manifests/validation_manifest_row.h"
#include "core/visualization/preview_pair_selector.h"
#include "application/application_controller.h"

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onBrowseDatasetClicked();
    void onSelectSingleImageClicked();
    void onPairSelected(QListWidgetItem *item);
    void onSearchTextChanged(const QString &query);
    void onExportCsvClicked();
    void onDatasetChanged(int index);
    void onPartitionChanged(int index);
    void onViewModeChanged(int index);
    void onSampleCountChanged(int value);
    void onFullValidationClicked();
    void onAddToQueueClicked();
    void onClearQueueClicked();

private:
    void setupUiControls();
    void loadDatasetFromDirectory(const QString &dirPath);
    void applyFiltersAndModes();
    void displayPair(const PairItemEntry &entry);
    void renderImageToLabel(QLabel *label, const QString &imagePath);


    [[nodiscard]]
    qart::core::domain::validation::PairValidationResult
    getOrValidatePair(const qart::core::domain::datasets::DatasetPair &pair);

    [[nodiscard]]
    qart::core::domain::validation::PairValidationStatus
    safeParseStatus(const std::string &statusStr) const;

    [[nodiscard]]
    QString getStatusTagText(qart::core::domain::validation::PairValidationStatus status) const;

    [[nodiscard]]
    QString getStatusColorHex(qart::core::domain::validation::PairValidationStatus status) const;

    [[nodiscard]]
    QString getStatusDescription(qart::core::domain::validation::PairValidationStatus status) const;

    Ui::MainWindow *ui;

    // On-demand validation cache by pairId
    std::map<std::string, qart::core::domain::validation::PairValidationResult> validatedResultsCache_;

    // Currently filtered and displayed item entries
    std::vector<PairItemEntry> displayedItems_;

    // Manual Selection Queue (Mode C)
    std::vector<qart::core::domain::datasets::DatasetPair> manualSelectionQueue_;

    // Export rows
    std::vector<qart::core::manifests::ValidationManifestRow> manifestRows_;

    bool isUpdatingControls_ = false;

    qart::application::ApplicationController appController_;
};
#endif // MAINWINDOW_H
