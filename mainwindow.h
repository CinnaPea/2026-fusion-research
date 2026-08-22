#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QLabel>
#include <filesystem>
#include <vector>

#include "core/manifests/validation_manifest_row.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_result.h"

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

private:
    void loadDatasetFromDirectory(const QString &dirPath);
    void displayPair(const qart::core::manifests::ValidationManifestRow &row);
    void renderImageToLabel(QLabel *label, const QString &imagePath);

    Ui::MainWindow *ui;
    std::filesystem::path datasetRoot_;
    std::vector<qart::core::manifests::ValidationManifestRow> manifestRows_;
};
#endif // MAINWINDOW_H
