#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "core/datasets/llvip_adapter.h"
#include "core/datasets/msrs_adapter.h"
#include "core/datasets/roadscene_adapter.h"
#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_policy.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/imaging/opencv_image_decoder.h"
#include "core/manifests/csv_validation_manifest_writer.h"
#include "core/manifests/validation_manifest.h"
#include "core/manifests/validation_manifest_row.h"
#include "core/validation/pair_validator.h"

#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QPixmap>
#include <QResizeEvent>
#include <algorithm>
#include <filesystem>
#include <vector>

using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::datasets::DatasetPartitionId;
using qart::core::domain::validation::PairValidationPolicy;
using qart::core::domain::validation::PairValidationResult;
using qart::core::domain::validation::PairValidationStatus;
using qart::core::imaging::OpenCvImageDecoder;
using qart::core::manifests::CsvValidationManifestWriter;
using qart::core::manifests::ValidationManifest;
using qart::core::manifests::ValidationManifestRow;
using qart::core::validation::PairValidator;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initial state
    ui->txtDatasetDir->clear();
    ui->lstImagePairs->clear();
    ui->lblPairCount->setText("Tổng số: 0 cặp ảnh");
    ui->lblCurrentPairId->setText("Cặp ảnh: Chưa chọn");
    ui->lblPairStatus->setText("Trạng thái: --");

    // Connect Signals and Slots
    connect(ui->btnBrowseDataset, &QPushButton::clicked, this, &MainWindow::onBrowseDatasetClicked);
    connect(ui->btnSelectSingleImage, &QPushButton::clicked, this, &MainWindow::onSelectSingleImageClicked);
    connect(ui->lstImagePairs, &QListWidget::itemClicked, this, &MainWindow::onPairSelected);
    connect(ui->txtSearchPair, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(ui->btnExportCsv, &QPushButton::clicked, this, &MainWindow::onExportCsvClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // Re-render current image when window is resized
    if (ui->lstImagePairs->currentItem()) {
        onPairSelected(ui->lstImagePairs->currentItem());
    }
}

void MainWindow::onBrowseDatasetClicked()
{
    const QString selectedDir = QFileDialog::getExistingDirectory(
        this,
        "Chọn thư mục bộ dữ liệu (LLVIP, MSRS, RoadScene...)",
        ui->txtDatasetDir->text().isEmpty() ? QDir::homePath() : ui->txtDatasetDir->text()
    );

    if (!selectedDir.isEmpty()) {
        loadDatasetFromDirectory(selectedDir);
    }
}

void MainWindow::loadDatasetFromDirectory(const QString &dirPath)
{
    ui->txtDatasetDir->setText(dirPath);
    ui->lstImagePairs->clear();
    manifestRows_.clear();
    datasetRoot_ = dirPath.toStdString();

    ui->lblSystemStatus->setText("Đang quét và bắt cặp các tập tin ảnh...");
    QCoreApplication::processEvents();

    std::vector<DatasetPair> pairs;
    std::string datasetIdName = "custom_dataset";

    // Try Dataset Adapters (LLVIP, MSRS, RoadScene) or Fallback Scan
    try {
        qart::core::datasets::LLVIPAdapter llvipAdapter;
        try {
            llvipAdapter.validateLayout(datasetRoot_);
            pairs = llvipAdapter.discoverPairs(datasetRoot_, DatasetPartitionId("train"));
            datasetIdName = llvipAdapter.datasetId();
        } catch (...) {
            qart::core::datasets::MSRSAdapter msrsAdapter;
            try {
                msrsAdapter.validateLayout(datasetRoot_);
                pairs = msrsAdapter.discoverPairs(datasetRoot_, DatasetPartitionId("train"));
                datasetIdName = msrsAdapter.datasetId();
            } catch (...) {
                qart::core::datasets::RoadSceneAdapter roadSceneAdapter;
                try {
                    roadSceneAdapter.validateLayout(datasetRoot_);
                    pairs = roadSceneAdapter.discoverPairs(datasetRoot_, DatasetPartitionId("default"));
                    datasetIdName = roadSceneAdapter.datasetId();
                } catch (...) {
                    // Direct Directory Scan
                    int index = 1;
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(datasetRoot_)) {
                        if (entry.is_regular_file()) {
                            std::string ext = entry.path().extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                            if (ext == ".jpg" || ext == ".png" || ext == ".bmp" || ext == ".jpeg") {
                                std::string relPath = std::filesystem::relative(entry.path(), datasetRoot_).string();
                                std::string stem = entry.path().stem().string();
                                std::string pairId = "pair_" + std::to_string(index++);
                                
                                pairs.emplace_back(
                                    pairId,
                                    datasetIdName,
                                    DatasetPartitionId("default"),
                                    stem,
                                    relPath,
                                    relPath
                                );
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
        // Ignored
    }

    if (pairs.empty()) {
        ui->lblSystemStatus->setText("Không tìm thấy file ảnh nào trong thư mục được chọn.");
        ui->lblPairCount->setText("Tổng số: 0 cặp ảnh");
        return;
    }

    // Validate Pairs using PairValidator & OpenCvImageDecoder
    OpenCvImageDecoder decoder;
    PairValidationPolicy policy(
        {".jpg", ".jpeg", ".png", ".bmp"},
        {".jpg", ".jpeg", ".png", ".bmp"}
    );
    PairValidator validator(decoder, policy);

    for (const auto& pair : pairs) {
        PairValidationResult result = validator.validatePair(datasetRoot_, pair);
        manifestRows_.push_back(ValidationManifestRow::fromResult(result));
    }

    // Populate List Widget
    for (std::size_t i = 0; i < manifestRows_.size(); ++i) {
        const auto& row = manifestRows_[i];
        QString statusTag = QString::fromStdString(row.status());
        QString itemText = QString("[%1] %2 (%3)")
                               .arg(statusTag)
                               .arg(QString::fromStdString(row.sourceStem()))
                               .arg(QString::fromStdString(row.pairId()));

        auto* item = new QListWidgetItem(itemText, ui->lstImagePairs);
        item->setData(Qt::UserRole, static_cast<int>(i));

        if (row.status() == "VALID" || row.status() == "OK") {
            item->setForeground(QBrush(QColor("#2E7D32")));
        } else {
            item->setForeground(QBrush(QColor("#E65100")));
        }
    }

    ui->lblPairCount->setText(QString("Tổng số: %1 cặp ảnh").arg(manifestRows_.size()));
    ui->lblSystemStatus->setText(QString("Tải thành công %1 cặp ảnh. Chọn một cặp trong danh sách để xem.").arg(manifestRows_.size()));

    // Select first pair by default if available
    if (ui->lstImagePairs->count() > 0) {
        ui->lstImagePairs->setCurrentRow(0);
        onPairSelected(ui->lstImagePairs->item(0));
    }
}

void MainWindow::onSelectSingleImageClicked()
{
    const QString imageFile = QFileDialog::getOpenFileName(
        this,
        "Chọn 1 tập tin ảnh (Hệ thống sẽ tự động tìm ảnh cặp tương ứng)",
        QDir::homePath(),
        "Images (*.jpg *.jpeg *.png *.bmp)"
    );

    if (imageFile.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(imageFile);
    QDir parentDir = fileInfo.dir();

    // Auto-detect dataset root (navigate parent directories)
    QDir searchDir = parentDir;
    QString detectedRoot = searchDir.absolutePath();
    for (int i = 0; i < 3; ++i) {
        if (searchDir.exists("visible") || searchDir.exists("infrared") || searchDir.exists("raw")) {
            detectedRoot = searchDir.absolutePath();
            break;
        }
        searchDir.cdUp();
    }

    loadDatasetFromDirectory(detectedRoot);

    // Auto-select the corresponding stem in the list
    QString targetStem = fileInfo.completeBaseName();
    for (int i = 0; i < ui->lstImagePairs->count(); ++i) {
        auto* item = ui->lstImagePairs->item(i);
        if (item->text().contains(targetStem)) {
            ui->lstImagePairs->setCurrentRow(i);
            onPairSelected(item);
            ui->lblSystemStatus->setText(QString("Tự động tìm thấy và chọn cặp ảnh cho '%1'").arg(targetStem));
            break;
        }
    }
}

void MainWindow::onPairSelected(QListWidgetItem *item)
{
    if (!item) {
        return;
    }

    int index = item->data(Qt::UserRole).toInt();
    if (index < 0 || index >= static_cast<int>(manifestRows_.size())) {
        return;
    }

    displayPair(manifestRows_[index]);
}

void MainWindow::displayPair(const ValidationManifestRow &row)
{
    ui->lblCurrentPairId->setText(QString("Cặp ảnh: %1 (Stem: %2)")
                                      .arg(QString::fromStdString(row.pairId()))
                                      .arg(QString::fromStdString(row.sourceStem())));

    // Status Badge Styling
    QString statusStr = QString::fromStdString(row.status());
    if (statusStr == "VALID" || statusStr == "OK") {
        ui->lblPairStatus->setText("Trạng thái: HỢP LỆ (VALID)");
        ui->lblPairStatus->setStyleSheet("background-color: #C8E6C9; color: #1B5E20; padding: 4px 10px; border-radius: 4px; font-weight: bold;");
    } else {
        ui->lblPairStatus->setText(QString("Trạng thái: %1").arg(statusStr));
        ui->lblPairStatus->setStyleSheet("background-color: #FFECB3; color: #E65100; padding: 4px 10px; border-radius: 4px; font-weight: bold;");
    }

    // Resolve Full Paths
    std::filesystem::path visFullPath = datasetRoot_ / row.visibleRelativePath();
    std::filesystem::path thrFullPath = datasetRoot_ / row.thermalRelativePath();

    // Render Visible Image (A1)
    renderImageToLabel(ui->lblVisibleImage, QString::fromStdString(visFullPath.string()));
    QString visDimStr = "--";
    if (row.visibleWidth().has_value() && row.visibleHeight().has_value()) {
        visDimStr = QString("%1 x %2 px").arg(*row.visibleWidth()).arg(*row.visibleHeight());
    }
    ui->lblVisibleInfo->setText(QString("File: %1\nKích thước: %2")
                                    .arg(QString::fromStdString(row.visibleRelativePath()), visDimStr));

    // Render Thermal Image (A2)
    renderImageToLabel(ui->lblThermalImage, QString::fromStdString(thrFullPath.string()));
    QString thrDimStr = "--";
    if (row.thermalWidth().has_value() && row.thermalHeight().has_value()) {
        thrDimStr = QString("%1 x %2 px").arg(*row.thermalWidth()).arg(*row.thermalHeight());
    }
    ui->lblThermalInfo->setText(QString("File: %1\nKích thước: %2")
                                    .arg(QString::fromStdString(row.thermalRelativePath()), thrDimStr));
}

void MainWindow::renderImageToLabel(QLabel *label, const QString &imagePath)
{
    if (!QFile::exists(imagePath)) {
        label->setText("⚠️ Không tìm thấy file ảnh trên đĩa");
        label->setPixmap(QPixmap());
        return;
    }

    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        label->setText("⚠️ Không thể giải mã file ảnh");
        label->setPixmap(QPixmap());
        return;
    }

    // Smoothly scale image to fit label container
    QSize targetSize = label->size();
    if (targetSize.width() < 50 || targetSize.height() < 50) {
        targetSize = QSize(400, 300);
    }

    QPixmap scaledPixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    label->setPixmap(scaledPixmap);
}

void MainWindow::onSearchTextChanged(const QString &query)
{
    const QString q = query.trimmed();
    for (int i = 0; i < ui->lstImagePairs->count(); ++i) {
        auto* item = ui->lstImagePairs->item(i);
        bool match = q.isEmpty() || item->text().contains(q, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void MainWindow::onExportCsvClicked()
{
    if (manifestRows_.empty()) {
        QMessageBox::information(this, "Thông báo", "Chưa có dữ liệu cặp ảnh để xuất báo cáo.");
        return;
    }

    const QString defaultPath = QString::fromStdString((datasetRoot_ / "validation_manifest_report.csv").string());
    const QString saveFile = QFileDialog::getSaveFileName(
        this,
        "Xuất báo cáo Manifest CSV",
        defaultPath,
        "CSV Files (*.csv)"
    );

    if (saveFile.isEmpty()) {
        return;
    }

    ValidationManifest manifest("2.0", manifestRows_);
    CsvValidationManifestWriter writer;

    try {
        const auto writtenPath = writer.write(manifest, saveFile.toStdString(), true);
        (void)writtenPath;
        QMessageBox::information(this, "Thành công", QString("Đã xuất báo cáo CSV thành công tại:\n%1").arg(saveFile));
        ui->lblSystemStatus->setText(QString("Đã xuất báo cáo CSV: %1").arg(saveFile));
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Lỗi xuất CSV", QString("Không thể xuất file CSV: %1").arg(ex.what()));
    }
}
