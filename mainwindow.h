#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QLabel>
#include <QString>
#include <filesystem>
#include <optional>
#include <vector>

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/manifests/validation_manifest_row.h"
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
    void onSearchTextChanged(const QString &query) const;
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
    void displayPair(const PairItemEntry &entry) const;
    static void renderImageToLabel(QLabel *label, const QString &imagePath);

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
};
#endif // MAINWINDOW_H
