#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "core/datasets/llvip_adapter.h"
#include "core/datasets/msrs_adapter.h"
#include "core/datasets/roadscene_adapter.h"
#include "core/domain/datasets/dataset_exceptions.h"
#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_policy.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"
#include "core/imaging/opencv_image_decoder.h"
#include "core/manifests/csv_validation_manifest_writer.h"
#include "core/manifests/validation_manifest.h"
#include "core/manifests/validation_manifest_row.h"
#include "core/validation/pair_validator.h"
#include "core/visualization/preview_pair_selector.h"

#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QPixmap>
#include <QResizeEvent>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
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

    setupUiControls();

    // Initial state
    ui->txtDatasetDir->clear();
    ui->lstImagePairs->clear();
    ui->lblPairCount->setText("Tổng số: 0 cặp ảnh");
    ui->lblCurrentPairId->setText("Cặp ảnh: Chưa chọn");
    ui->lblPairStatus->setText("Trạng thái: --");
    ui->lblPairStatusMessage->setText("");

    // Connect Signals and Slots
    connect(ui->btnBrowseDataset, &QPushButton::clicked, this, &MainWindow::onBrowseDatasetClicked);
    connect(ui->btnSelectSingleImage, &QPushButton::clicked, this, &MainWindow::onSelectSingleImageClicked);
    connect(ui->lstImagePairs, &QListWidget::itemClicked, this, &MainWindow::onPairSelected);
    connect(ui->txtSearchPair, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(ui->btnExportCsv, &QPushButton::clicked, this, &MainWindow::onExportCsvClicked);

    connect(ui->cbDatasetSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDatasetChanged);
    connect(ui->cbPartitionSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPartitionChanged);
    connect(ui->cbViewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onViewModeChanged);
    connect(ui->spnSampleCount, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSampleCountChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUiControls()
{
    isUpdatingControls_ = true;

    // Dataset combo box items
    ui->cbDatasetSelect->clear();
    ui->cbDatasetSelect->addItem("Tự động phát hiện", "");
    ui->cbDatasetSelect->addItem("LLVIP Dataset", "llvip");
    ui->cbDatasetSelect->addItem("MSRS Dataset", "msrs");
    ui->cbDatasetSelect->addItem("RoadScene Dataset", "roadscene");

    // Partition combo box items
    ui->cbPartitionSelect->clear();
    ui->cbPartitionSelect->addItem("Tất cả Partition", "all");
    ui->cbPartitionSelect->addItem("train", "train");
    ui->cbPartitionSelect->addItem("test", "test");
    ui->cbPartitionSelect->addItem("default", "default");

    // View Mode combo box items
    ui->cbViewMode->clear();
    ui->cbViewMode->addItem("Chế độ 1: Tự chọn mẫu (Sample)", "sample");
    ui->cbViewMode->addItem("Chế độ 2: Duyệt tất cả (Full)", "full");

    ui->spnSampleCount->setRange(2, 500);
    ui->spnSampleCount->setValue(5);

    isUpdatingControls_ = false;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    try {
        QMainWindow::resizeEvent(event);

        if (ui->lstImagePairs->currentItem()) {
            onPairSelected(ui->lstImagePairs->currentItem());
        }
    } catch (...) {
        // Suppress any resize exceptions
    }
}

std::filesystem::path MainWindow::resolveDatasetRoot(const std::filesystem::path &inputPath) const
{
    if (!std::filesystem::exists(inputPath)) {
        return inputPath;
    }

    std::error_code ec;
    std::filesystem::path current = std::filesystem::canonical(inputPath, ec);
    if (ec) {
        current = inputPath;
    }

    // If inputPath directly contains "raw" (e.g. QART_Datasets)
    if (std::filesystem::exists(current / "raw") && std::filesystem::is_directory(current / "raw")) {
        return current;
    }

    // If inputPath is "raw" itself
    if (current.filename() == "raw" && current.has_parent_path()) {
        return current.parent_path();
    }

    // If inputPath is "llvip", "msrs", or "roadscene"
    std::string folderName = current.filename().string();
    std::transform(folderName.begin(), folderName.end(), folderName.begin(), ::tolower);

    if (folderName == "llvip" || folderName == "msrs" || folderName == "roadscene") {
        if (current.has_parent_path()) {
            std::filesystem::path parent = current.parent_path();
            if (parent.filename() == "raw" && parent.has_parent_path()) {
                return parent.parent_path();
            }
            return parent;
        }
    }

    // Traverse up parent directories (up to 4 levels) to find any ancestor containing "raw"
    std::filesystem::path search = current;
    for (int i = 0; i < 4; ++i) {
        if (std::filesystem::exists(search / "raw") && std::filesystem::is_directory(search / "raw")) {
            return search;
        }
        if (!search.has_parent_path() || search == search.parent_path()) {
            break;
        }
        search = search.parent_path();
    }

    return current;
}

void MainWindow::onBrowseDatasetClicked()
{
    try {
        const QString selectedDir = QFileDialog::getExistingDirectory(
            this,
            "Chọn thư mục bộ dữ liệu (LLVIP, MSRS, RoadScene...)",
            ui->txtDatasetDir->text().isEmpty() ? QDir::homePath() : ui->txtDatasetDir->text()
        );

        if (!selectedDir.isEmpty()) {
            loadDatasetFromDirectory(selectedDir);
        }
    } catch (const std::exception &ex) {
        QMessageBox::critical(this, "Lỗi Tải Thư Mục", QString("Đã xảy ra lỗi khi chọn thư mục:\n%1").arg(ex.what()));
    } catch (...) {
        QMessageBox::critical(this, "Lỗi Tải Thư Mục", "Đã xảy ra lỗi không xác định khi chọn thư mục.");
    }
}

void MainWindow::loadDatasetFromDirectory(const QString &dirPath)
{
    try {
        ui->txtDatasetDir->setText(dirPath);
        ui->lstImagePairs->clear();
        manifestRows_.clear();
        allValidationResults_.clear();

        datasetRoot_ = resolveDatasetRoot(dirPath.toStdString());

        ui->lblSystemStatus->setText("Đang quét và kiểm định các tập tin ảnh...");
        QCoreApplication::processEvents();

        std::vector<DatasetPair> discoveredPairs;
        std::string detectedDatasetId = "";

        QString selectedDataset = ui->cbDatasetSelect->currentData().toString();

        // 1. Try LLVIP Adapter
        if (selectedDataset.isEmpty() || selectedDataset == "llvip") {
            qart::core::datasets::LLVIPAdapter llvipAdapter;
            try {
                llvipAdapter.validateLayout(datasetRoot_);
                detectedDatasetId = llvipAdapter.datasetId();
                for (const auto &partition : llvipAdapter.supportedPartitions()) {
                    auto pairs = llvipAdapter.discoverPairs(datasetRoot_, partition);
                    discoveredPairs.insert(discoveredPairs.end(), pairs.begin(), pairs.end());
                }
            } catch (...) {
                // Layout validation failed for LLVIP
            }
        }

        // 2. Try MSRS Adapter
        if (discoveredPairs.empty() && (selectedDataset.isEmpty() || selectedDataset == "msrs")) {
            qart::core::datasets::MSRSAdapter msrsAdapter;
            try {
                msrsAdapter.validateLayout(datasetRoot_);
                detectedDatasetId = msrsAdapter.datasetId();
                for (const auto &partition : msrsAdapter.supportedPartitions()) {
                    auto pairs = msrsAdapter.discoverPairs(datasetRoot_, partition);
                    discoveredPairs.insert(discoveredPairs.end(), pairs.begin(), pairs.end());
                }
            } catch (...) {
                // Layout validation failed for MSRS
            }
        }

        // 3. Try RoadScene Adapter
        if (discoveredPairs.empty() && (selectedDataset.isEmpty() || selectedDataset == "roadscene")) {
            qart::core::datasets::RoadSceneAdapter roadSceneAdapter;
            try {
                roadSceneAdapter.validateLayout(datasetRoot_);
                detectedDatasetId = roadSceneAdapter.datasetId();
                for (const auto &partition : roadSceneAdapter.supportedPartitions()) {
                    auto pairs = roadSceneAdapter.discoverPairs(datasetRoot_, partition);
                    discoveredPairs.insert(discoveredPairs.end(), pairs.begin(), pairs.end());
                }
            } catch (...) {
                // Layout validation failed for RoadScene
            }
        }

        // If no valid dataset adapter matched
        if (discoveredPairs.empty()) {
            ui->lblSystemStatus->setText("❌ Không thể nhận diện tập dữ liệu hợp lệ.");
            ui->lblPairCount->setText("Tổng số: 0 cặp ảnh");
            ui->lblCurrentPairId->setText("Cặp ảnh: Chưa chọn");
            ui->lblPairStatus->setText("Trạng thái: --");
            ui->lblPairStatusMessage->setText("");

            QMessageBox::warning(
                this,
                "Không nhận diện được Dataset",
                QString("Không thể nhận diện tập dữ liệu chuẩn (LLVIP, MSRS, RoadScene) tại đường dẫn:\n%1\n\nVui lòng kiểm tra lại cấu trúc thư mục chứa các tập dữ liệu chuẩn!")
                    .arg(dirPath)
            );
            return;
        }

        currentDatasetId_ = QString::fromStdString(detectedDatasetId);

        // Update controls reflectively
        isUpdatingControls_ = true;
        if (selectedDataset.isEmpty()) {
            int idx = ui->cbDatasetSelect->findData(currentDatasetId_);
            if (idx >= 0) {
                ui->cbDatasetSelect->setCurrentIndex(idx);
            }
        }

        // Update cbPartitionSelect items based on dataset
        QString curPartition = ui->cbPartitionSelect->currentData().toString();
        ui->cbPartitionSelect->clear();
        ui->cbPartitionSelect->addItem("Tất cả Partition", "all");

        if (currentDatasetId_ == "llvip" || currentDatasetId_ == "msrs") {
            ui->cbPartitionSelect->addItem("train", "train");
            ui->cbPartitionSelect->addItem("test", "test");
        } else if (currentDatasetId_ == "roadscene") {
            ui->cbPartitionSelect->addItem("default", "default");
        }

        int partIdx = ui->cbPartitionSelect->findData(curPartition);
        if (partIdx >= 0) {
            ui->cbPartitionSelect->setCurrentIndex(partIdx);
        }
        isUpdatingControls_ = false;

        // Validate Pairs using PairValidator & OpenCvImageDecoder
        OpenCvImageDecoder decoder;
        PairValidationPolicy policy(
            {".jpg", ".jpeg", ".png", ".bmp"},
            {".jpg", ".jpeg", ".png", ".bmp"}
        );
        PairValidator validator(decoder, policy);

        for (const auto &pair : discoveredPairs) {
            PairValidationResult result = validator.validatePair(datasetRoot_, pair);
            allValidationResults_.push_back(result);
        }

        applyFiltersAndModes();
    } catch (const std::exception &ex) {
        QMessageBox::critical(this, "Lỗi Nạp Dataset", QString("Đã xảy ra lỗi khi kiểm định dữ liệu:\n%1").arg(ex.what()));
    } catch (...) {
        QMessageBox::critical(this, "Lỗi Nạp Dataset", "Đã xảy ra lỗi không xác định khi nạp dataset.");
    }
}

void MainWindow::applyFiltersAndModes()
{
    try {
        ui->lstImagePairs->clear();
        manifestRows_.clear();

        if (allValidationResults_.empty()) {
            ui->lblPairCount->setText("Tổng số: 0 cặp ảnh");
            ui->lblSystemStatus->setText("Chưa có dữ liệu cặp ảnh.");
            return;
        }

        // 1. Filter by Partition
        QString selectedPartition = ui->cbPartitionSelect->currentData().toString();
        std::vector<PairValidationResult> filteredResults;

        if (selectedPartition.isEmpty() || selectedPartition == "all") {
            filteredResults = allValidationResults_;
        } else {
            for (const auto &res : allValidationResults_) {
                if (res.pair().partitionId().value() == selectedPartition.toStdString()) {
                    filteredResults.push_back(res);
                }
            }
        }

        // 2. Filter by View Mode (Sample Mode vs Full Mode)
        QString viewMode = ui->cbViewMode->currentData().toString();
        std::vector<PairValidationResult> displayResults;

        if (viewMode == "sample") {
            int countPerPartition = std::max(2, ui->spnSampleCount->value());

            // First, try using PreviewPairSelector C++ core
            bool selectorSucceeded = false;
            try {
                qart::core::visualization::PreviewPairSelector selector;
                qart::core::visualization::PreviewSelectionRequest request(countPerPartition, {});
                auto selection = selector.select(filteredResults, request);
                displayResults = selection.allResults();
                selectorSucceeded = !displayResults.empty();
            } catch (...) {
                selectorSucceeded = false;
            }

            // Fail-safe sampling fallback if PreviewPairSelector threw an exception
            if (!selectorSucceeded) {
                displayResults.clear();
                std::map<std::string, std::vector<PairValidationResult>> partitionGroups;
                for (const auto &res : filteredResults) {
                    partitionGroups[res.pair().partitionId().value()].push_back(res);
                }

                for (const auto &[partId, groupItems] : partitionGroups) {
                    if (groupItems.empty()) continue;

                    int total = static_cast<int>(groupItems.size());
                    if (total <= countPerPartition) {
                        displayResults.insert(displayResults.end(), groupItems.begin(), groupItems.end());
                    } else {
                        int denom = countPerPartition - 1;
                        int finalIdx = total - 1;
                        for (int i = 0; i < countPerPartition; ++i) {
                            std::size_t idx = static_cast<std::size_t>(i * finalIdx / denom);
                            if (idx < groupItems.size()) {
                                displayResults.push_back(groupItems[idx]);
                            }
                        }
                    }
                }
            }

            if (displayResults.empty()) {
                displayResults = filteredResults;
            }
        } else {
            displayResults = filteredResults;
        }

        // 3. Populate manifest rows and list widget
        for (const auto &res : displayResults) {
            ValidationManifestRow row = ValidationManifestRow::fromResult(res);
            manifestRows_.push_back(row);

            QString statusTag = getStatusTagText(res.status());
            QString itemText = QString("[%1] %2 (%3)")
                                   .arg(statusTag)
                                   .arg(QString::fromStdString(row.sourceStem()))
                                   .arg(QString::fromStdString(row.pairId()));

            auto *item = new QListWidgetItem(itemText, ui->lstImagePairs);
            item->setData(Qt::UserRole, static_cast<int>(manifestRows_.size() - 1));

            QString colorHex = getStatusColorHex(res.status());
            item->setForeground(QBrush(QColor(colorHex)));
        }

        ui->lblPairCount->setText(QString("Tổng số: %1 cặp ảnh (%2)")
                                      .arg(manifestRows_.size())
                                      .arg(viewMode == "sample" ? "Chế độ lấy mẫu" : "Duyệt tất cả"));

        ui->lblSystemStatus->setText(QString("Tải thành công %1 cặp ảnh (%2). Chọn 1 cặp trong danh sách để xem.")
                                         .arg(manifestRows_.size())
                                         .arg(currentDatasetId_.toUpper()));

        // Select first item if available
        if (ui->lstImagePairs->count() > 0) {
            ui->lstImagePairs->setCurrentRow(0);
            onPairSelected(ui->lstImagePairs->item(0));
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi áp dụng bộ lọc: %1").arg(ex.what()));
    } catch (...) {
        ui->lblSystemStatus->setText("Lỗi không xác định khi áp dụng bộ lọc.");
    }
}

PairValidationStatus MainWindow::safeParseStatus(const std::string &statusStr) const
{
    try {
        return qart::core::domain::validation::pairValidationStatusFromString(statusStr);
    } catch (...) {
        return PairValidationStatus::Valid;
    }
}

QString MainWindow::getStatusTagText(PairValidationStatus status) const
{
    switch (status) {
        case PairValidationStatus::Valid:
            return "VALID";
        case PairValidationStatus::MissingVisible:
            return "THIẾU ANH MÀU";
        case PairValidationStatus::MissingThermal:
            return "THIẾU ANH NHIỆT";
        case PairValidationStatus::UnsupportedVisibleFormat:
            return "FORMAT ANH MÀU KHÔNG HỖ TRỢ";
        case PairValidationStatus::UnsupportedThermalFormat:
            return "FORMAT ANH NHIỆT KHÔNG HỖ TRỢ";
        case PairValidationStatus::VisibleDecodeFailed:
            return "LỖI ĐỌC ANH MÀU";
        case PairValidationStatus::ThermalDecodeFailed:
            return "LỖI ĐỌC ANH NHIỆT";
        case PairValidationStatus::DimensionMismatch:
            return "SAI KÍCH THƯỚC";
        case PairValidationStatus::UnexpectedVisibleDimensions:
            return "SAI RES VISIBLE";
        case PairValidationStatus::UnexpectedThermalDimensions:
            return "SAI RES THERMAL";
        case PairValidationStatus::DuplicateVisibleStem:
            return "TRÙNG STEM VISIBLE";
        case PairValidationStatus::DuplicateThermalStem:
            return "TRÙNG STEM THERMAL";
        default:
            return "LỖI KIỂM ĐỊNH";
    }
}

QString MainWindow::getStatusColorHex(PairValidationStatus status) const
{
    switch (status) {
        case PairValidationStatus::Valid:
            return "#2E7D32"; // Dark Green
        case PairValidationStatus::DimensionMismatch:
        case PairValidationStatus::UnexpectedVisibleDimensions:
        case PairValidationStatus::UnexpectedThermalDimensions:
            return "#E65100"; // Dark Amber / Orange
        case PairValidationStatus::MissingVisible:
        case PairValidationStatus::MissingThermal:
        case PairValidationStatus::VisibleDecodeFailed:
        case PairValidationStatus::ThermalDecodeFailed:
            return "#C62828"; // Red
        default:
            return "#EF6C00"; // Deep Orange
    }
}

QString MainWindow::getStatusDescription(PairValidationStatus status) const
{
    switch (status) {
        case PairValidationStatus::Valid:
            return "Cặp ảnh hợp lệ, cùng độ phân giải pixel và giải mã thành công.";
        case PairValidationStatus::MissingVisible:
            return "Không tìm thấy tập tin ảnh màu (Visible) tương ứng trong thư mục.";
        case PairValidationStatus::MissingThermal:
            return "Không tìm thấy tập tin ảnh nhiệt (Thermal) tương ứng trong thư mục.";
        case PairValidationStatus::UnsupportedVisibleFormat:
            return "Định dạng tập tin ảnh màu không được hỗ trợ (.jpg, .png, .bmp).";
        case PairValidationStatus::UnsupportedThermalFormat:
            return "Định dạng tập tin ảnh nhiệt không được hỗ trợ (.jpg, .png, .bmp).";
        case PairValidationStatus::VisibleDecodeFailed:
            return "Không thể giải mã dữ liệu ảnh màu bằng OpenCV.";
        case PairValidationStatus::ThermalDecodeFailed:
            return "Không thể giải mã dữ liệu ảnh nhiệt bằng OpenCV.";
        case PairValidationStatus::DimensionMismatch:
            return "⚠️ CẢNH BÁO: Kích thước pixel ảnh Màu và ảnh Nhiệt không khớp nhau.";
        case PairValidationStatus::UnexpectedVisibleDimensions:
            return "Kích thước ảnh màu không nằm trong tiêu chuẩn quy định.";
        case PairValidationStatus::UnexpectedThermalDimensions:
            return "Kích thước ảnh nhiệt không nằm trong tiêu chuẩn quy định.";
        case PairValidationStatus::DuplicateVisibleStem:
            return "Tên stem ảnh màu bị lặp lại trong cùng partition.";
        case PairValidationStatus::DuplicateThermalStem:
            return "Tên stem ảnh nhiệt bị lặp lại trong cùng partition.";
        default:
            return "Phát hiện sự cố kiểm định không xác định.";
    }
}

void MainWindow::onDatasetChanged(int index)
{
    Q_UNUSED(index);
    if (isUpdatingControls_) return;

    try {
        if (!ui->txtDatasetDir->text().isEmpty()) {
            loadDatasetFromDirectory(ui->txtDatasetDir->text());
        }
    } catch (...) {
        // Suppress slot exceptions
    }
}

void MainWindow::onPartitionChanged(int index)
{
    Q_UNUSED(index);
    if (isUpdatingControls_) return;

    try {
        applyFiltersAndModes();
    } catch (...) {
        // Suppress slot exceptions
    }
}

void MainWindow::onViewModeChanged(int index)
{
    Q_UNUSED(index);
    if (isUpdatingControls_) return;

    try {
        bool isSample = (ui->cbViewMode->currentData().toString() == "sample");
        ui->lblSampleCountLabel->setVisible(isSample);
        ui->spnSampleCount->setVisible(isSample);

        applyFiltersAndModes();
    } catch (...) {
        // Suppress slot exceptions
    }
}

void MainWindow::onSampleCountChanged(int value)
{
    Q_UNUSED(value);
    if (isUpdatingControls_) return;

    try {
        if (ui->cbViewMode->currentData().toString() == "sample") {
            applyFiltersAndModes();
        }
    } catch (...) {
        // Suppress slot exceptions
    }
}

void MainWindow::onSelectSingleImageClicked()
{
    try {
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
        std::filesystem::path inputPath(imageFile.toStdString());
        std::filesystem::path resolvedRoot = resolveDatasetRoot(inputPath);

        loadDatasetFromDirectory(QString::fromStdString(resolvedRoot.string()));

        // Auto-select the corresponding stem in the list
        QString targetStem = fileInfo.completeBaseName();
        for (int i = 0; i < ui->lstImagePairs->count(); ++i) {
            auto *item = ui->lstImagePairs->item(i);
            if (item->text().contains(targetStem)) {
                ui->lstImagePairs->setCurrentRow(i);
                onPairSelected(item);
                ui->lblSystemStatus->setText(QString("Tự động tìm thấy và chọn cặp ảnh cho stem '%1'").arg(targetStem));
                break;
            }
        }
    } catch (const std::exception &ex) {
        QMessageBox::critical(this, "Lỗi Chọn Ảnh", QString("Đã xảy ra lỗi khi chọn ảnh:\n%1").arg(ex.what()));
    } catch (...) {
        QMessageBox::critical(this, "Lỗi Chọn Ảnh", "Đã xảy ra lỗi không xác định khi chọn ảnh.");
    }
}

void MainWindow::onPairSelected(QListWidgetItem *item)
{
    try {
        if (!item) {
            return;
        }

        int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= static_cast<int>(manifestRows_.size())) {
            return;
        }

        displayPair(manifestRows_[index]);
    } catch (...) {
        // Suppress selection slot exceptions
    }
}

void MainWindow::displayPair(const ValidationManifestRow &row)
{
    try {
        ui->lblCurrentPairId->setText(QString("Cặp ảnh: %1 (Stem: %2 | Partition: %3)")
                                          .arg(QString::fromStdString(row.pairId()))
                                          .arg(QString::fromStdString(row.sourceStem()))
                                          .arg(QString::fromStdString(row.partitionId())));

        PairValidationStatus status = safeParseStatus(row.status());
        QString tagText = getStatusTagText(status);
        QString colorHex = getStatusColorHex(status);
        QString descText = getStatusDescription(status);

        ui->lblPairStatus->setText(QString("Trạng thái: %1").arg(tagText));
        ui->lblPairStatus->setStyleSheet(QString("background-color: %1; color: #FFFFFF; padding: 4px 10px; border-radius: 4px; font-weight: bold;")
                                             .arg(colorHex));

        // Show detailed validation message
        if (row.message().has_value() && !row.message()->empty()) {
            ui->lblPairStatusMessage->setText(QString("💬 Chi tiết: %1 | %2")
                                                  .arg(QString::fromStdString(*row.message()))
                                                  .arg(descText));
        } else {
            ui->lblPairStatusMessage->setText(QString("💬 Chi tiết: %1").arg(descText));
        }

        // Resolve Full Paths
        std::filesystem::path visFullPath = datasetRoot_ / row.visibleRelativePath();
        std::filesystem::path thrFullPath = datasetRoot_ / row.thermalRelativePath();

        // Render Visible Image (A1) - Always attempt rendering even if status is dimension mismatch
        bool visOk = row.visibleDecoded();
        renderImageToLabel(ui->lblVisibleImage, QString::fromStdString(visFullPath.string()), visOk);

        QString visDimStr = "--";
        if (row.visibleWidth().has_value() && row.visibleHeight().has_value()) {
            visDimStr = QString("%1 x %2 px").arg(*row.visibleWidth()).arg(*row.visibleHeight());
        }
        ui->lblVisibleInfo->setText(QString("File: %1\nKích thước: %2 | Decoding: %3")
                                        .arg(QString::fromStdString(row.visibleRelativePath()))
                                        .arg(visDimStr)
                                        .arg(visOk ? "OK" : "Lỗi"));

        // Render Thermal Image (A2) - Always attempt rendering even if status is dimension mismatch
        bool thrOk = row.thermalDecoded();
        renderImageToLabel(ui->lblThermalImage, QString::fromStdString(thrFullPath.string()), thrOk);

        QString thrDimStr = "--";
        if (row.thermalWidth().has_value() && row.thermalHeight().has_value()) {
            thrDimStr = QString("%1 x %2 px").arg(*row.thermalWidth()).arg(*row.thermalHeight());
        }
        ui->lblThermalInfo->setText(QString("File: %1\nKích thước: %2 | Decoding: %3")
                                        .arg(QString::fromStdString(row.thermalRelativePath()))
                                        .arg(thrDimStr)
                                        .arg(thrOk ? "OK" : "Lỗi"));
    } catch (...) {
        // Suppress display exceptions
    }
}

void MainWindow::renderImageToLabel(QLabel *label, const QString &imagePath, bool isSuccess)
{
    Q_UNUSED(isSuccess);

    try {
        if (!QFile::exists(imagePath)) {
            label->setText("⚠️ Không tìm thấy file ảnh trên đĩa");
            label->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
            label->setPixmap(QPixmap());
            return;
        }

        QPixmap pixmap(imagePath);
        if (pixmap.isNull()) {
            label->setText("⚠️ Không thể giải mã dữ liệu ảnh");
            label->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
            label->setPixmap(QPixmap());
            return;
        }

        QSize targetSize = label->size();
        if (targetSize.width() < 50 || targetSize.height() < 50) {
            targetSize = QSize(400, 300);
        }

        QPixmap scaledPixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        label->setPixmap(scaledPixmap);
        label->setStyleSheet("background-color: #1e1e1e; color: #888888; border: 1px solid #333333; border-radius: 6px;");
    } catch (...) {
        label->setText("⚠️ Lỗi hiển thị ảnh");
        label->setPixmap(QPixmap());
    }
}

void MainWindow::onSearchTextChanged(const QString &query)
{
    try {
        const QString q = query.trimmed();
        for (int i = 0; i < ui->lstImagePairs->count(); ++i) {
            auto *item = ui->lstImagePairs->item(i);
            bool match = q.isEmpty() || item->text().contains(q, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    } catch (...) {
        // Suppress search slot exceptions
    }
}

void MainWindow::onExportCsvClicked()
{
    try {
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

        const auto writtenPath = writer.write(manifest, saveFile.toStdString(), true);
        (void)writtenPath;
        QMessageBox::information(this, "Thành công", QString("Đã xuất báo cáo CSV thành công tại:\n%1").arg(saveFile));
        ui->lblSystemStatus->setText(QString("Đã xuất báo cáo CSV: %1").arg(saveFile));
    } catch (const std::exception &ex) {
        QMessageBox::critical(this, "Lỗi xuất CSV", QString("Không thể xuất file CSV: %1").arg(ex.what()));
    } catch (...) {
        QMessageBox::critical(this, "Lỗi xuất CSV", "Không thể xuất file CSV do lỗi không xác định.");
    }
}
