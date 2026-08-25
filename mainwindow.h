#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QLabel>
#include <QString>
#include <filesystem>
#include <vector>

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/manifests/validation_manifest_row.h"
#include "core/visualization/preview_pair_selector.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

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

private:
    void setupUiControls();
    void loadDatasetFromDirectory(const QString &dirPath);
    void applyFiltersAndModes();
    void displayPair(const qart::core::manifests::ValidationManifestRow &row);
    void renderImageToLabel(QLabel *label, const QString &imagePath, bool isSuccess);

    [[nodiscard]]
    std::filesystem::path resolveDatasetRoot(const std::filesystem::path &inputPath) const;

    [[nodiscard]]
    qart::core::domain::validation::PairValidationStatus safeParseStatus(const std::string &statusStr) const;

    [[nodiscard]]
    QString getStatusTagText(qart::core::domain::validation::PairValidationStatus status) const;

    [[nodiscard]]
    QString getStatusColorHex(qart::core::domain::validation::PairValidationStatus status) const;

    [[nodiscard]]
    QString getStatusDescription(qart::core::domain::validation::PairValidationStatus status) const;

    Ui::MainWindow *ui;
    std::filesystem::path datasetRoot_;
    std::vector<qart::core::domain::validation::PairValidationResult> allValidationResults_;
    std::vector<qart::core::manifests::ValidationManifestRow> manifestRows_;
    QString currentDatasetId_;
    bool isUpdatingControls_ = false;
};
#endif // MAINWINDOW_H
