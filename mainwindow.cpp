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
#include <QCoreApplication>
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
#include <set>
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
    ui->prgLoading->setVisible(false);

    // Connect Signals and Slots
    connect(ui->btnBrowseDataset, &QPushButton::clicked, this, &MainWindow::onBrowseDatasetClicked);
    connect(ui->btnSelectSingleImage, &QPushButton::clicked, this, &MainWindow::onSelectSingleImageClicked);
    connect(ui->btnFullValidation, &QPushButton::clicked, this, &MainWindow::onFullValidationClicked);
    connect(ui->btnAddToQueue, &QPushButton::clicked, this, &MainWindow::onAddToQueueClicked);
    connect(ui->btnClearQueue, &QPushButton::clicked, this, &MainWindow::onClearQueueClicked);

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
    ui->cbPartitionSelect->addItem("Tất cả Partition", "");
    ui->cbPartitionSelect->addItem("train", "train");
    ui->cbPartitionSelect->addItem("test", "test");
    ui->cbPartitionSelect->addItem("all", "all");

    // View Mode combo box items
    ui->cbViewMode->clear();
    ui->cbViewMode->addItem("Chế độ A: Duyệt toàn bộ (Full Dataset)", "full");
    ui->cbViewMode->addItem("Chế độ B: Lấy mẫu tự động (Auto Sample)", "sample");
    ui->cbViewMode->addItem("Chế độ C: Hàng đợi chọn mẫu (Queue: Auto + Manual)", "queue");

    ui->spnSampleCount->setRange(2, 500);
    ui->spnSampleCount->setValue(5);
    ui->lblSampleCountLabel->setVisible(false);
    ui->spnSampleCount->setVisible(false);

    isUpdatingControls_ = false;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    try {
        if (ui->lstImagePairs->currentItem()) {
            int idx = ui->lstImagePairs->currentItem()->data(Qt::UserRole).toInt();
            if (idx >= 0 && idx < static_cast<int>(displayedItems_.size())) {
                displayPair(displayedItems_[idx]);
            }
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi resize: %1").arg(ex.what()));
    }
}

std::filesystem::path MainWindow::resolveDatasetRoot(const std::filesystem::path &inputPath) const
{
    if (!std::filesystem::exists(inputPath)) {
        return inputPath;
    }

    std::filesystem::path current = inputPath;
    // If input is a file, start from its parent directory
    if (std::filesystem::is_regular_file(current)) {
        current = current.parent_path();
    }

    std::error_code ec;
    current = std::filesystem::canonical(current, ec);
    if (ec) {
        current = inputPath;
        if (std::filesystem::is_regular_file(current)) {
            current = current.parent_path();
        }
    }

    // Check if current directly contains "raw" (e.g. QART_Datasets)
    if (std::filesystem::exists(current / "raw") && std::filesystem::is_directory(current / "raw")) {
        return current;
    }

    // Check if current is "raw"
    if (current.filename() == "raw" && current.has_parent_path()) {
        return current.parent_path();
    }

    // Check if current is dataset folder name
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

    // Traverse up to 5 ancestor levels looking for directory containing "raw"
    std::filesystem::path search = current;
    for (int i = 0; i < 5; ++i) {
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

PairValidationPolicy MainWindow::getDatasetValidationPolicy(const std::string &datasetId) const
{
    std::vector<std::string> supportedExtensions = {".jpg", ".jpeg", ".png", ".bmp"};

    if (datasetId == "llvip") {
        return PairValidationPolicy(
            supportedExtensions,
            supportedExtensions,
            qart::core::domain::imaging::ImageDimensions(1280, 1024),
            qart::core::domain::imaging::ImageDimensions(1280, 1024)
        );
    }
    if (datasetId == "msrs") {
        return PairValidationPolicy(
            supportedExtensions,
            supportedExtensions,
            qart::core::domain::imaging::ImageDimensions(640, 480),
            qart::core::domain::imaging::ImageDimensions(640, 480)
        );
    }
    // RoadScene and other generic datasets: Pairwise matching only
    return PairValidationPolicy(
        supportedExtensions,
        supportedExtensions,
        std::nullopt,
        std::nullopt
    );
}

PairValidationResult MainWindow::getOrValidatePair(const DatasetPair &pair)
{
    auto it = validatedResultsCache_.find(pair.pairId());
    if (it != validatedResultsCache_.end()) {
        return it->second;
    }

    OpenCvImageDecoder decoder;
    PairValidationPolicy policy = getDatasetValidationPolicy(pair.datasetId());
    PairValidator validator(decoder, policy);

    PairValidationResult result = validator.validatePair(datasetRoot_, pair);
    validatedResultsCache_.emplace(pair.pairId(), result);
    return result;
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
    }
}

void MainWindow::loadDatasetFromDirectory(const QString &dirPath)
{
    try {
        ui->txtDatasetDir->setText(dirPath);
        ui->lstImagePairs->clear();
        displayedItems_.clear();
        manifestRows_.clear();
        allDiscoveredPairs_.clear();
        validatedResultsCache_.clear();

        datasetRoot_ = resolveDatasetRoot(dirPath.toStdString());

        ui->lblSystemStatus->setText("Đang quét cấu trúc tập dữ liệu...");
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
            ui->lblSystemStatus->setText("Không thể nhận diện tập dữ liệu hợp lệ.");
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
        allDiscoveredPairs_ = std::move(discoveredPairs);

        // Update controls reflectively
        isUpdatingControls_ = true;
        if (selectedDataset.isEmpty()) {
            int idx = ui->cbDatasetSelect->findData(currentDatasetId_);
            if (idx >= 0) {
                ui->cbDatasetSelect->setCurrentIndex(idx);
            }
        }

        // Update cbPartitionSelect items based on dataset semantics
        QString curPartition = ui->cbPartitionSelect->currentData().toString();
        ui->cbPartitionSelect->clear();

        if (currentDatasetId_ == "llvip" || currentDatasetId_ == "msrs") {
            ui->cbPartitionSelect->addItem("Tất cả Partition", "");
            ui->cbPartitionSelect->addItem("train", "train");
            ui->cbPartitionSelect->addItem("test", "test");
        } else if (currentDatasetId_ == "roadscene") {
            ui->cbPartitionSelect->addItem("all (native partition)", "all");
        } else {
            ui->cbPartitionSelect->addItem("Tất cả Partition", "");
        }

        int partIdx = ui->cbPartitionSelect->findData(curPartition);
        if (partIdx >= 0) {
            ui->cbPartitionSelect->setCurrentIndex(partIdx);
        } else {
            ui->cbPartitionSelect->setCurrentIndex(0);
        }
        isUpdatingControls_ = false;

        applyFiltersAndModes();
    } catch (const std::exception &ex) {
        QMessageBox::critical(this, "Lỗi Nạp Dataset", QString("Đã xảy ra lỗi khi nạp dữ liệu:\n%1").arg(ex.what()));
    }
}

void MainWindow::applyFiltersAndModes()
{
    try {
        ui->lstImagePairs->clear();
        displayedItems_.clear();
        manifestRows_.clear();

        if (allDiscoveredPairs_.empty()) {
            ui->lblPairCount->setText("Tổng số: 0 cặp ảnh");
            ui->lblSystemStatus->setText("Chưa có dữ liệu cặp ảnh.");
            return;
        }

        // 1. Filter by Partition
        QString selectedPartition = ui->cbPartitionSelect->currentData().toString();
        std::vector<DatasetPair> filteredPairs;

        if (selectedPartition.isEmpty()) {
            filteredPairs = allDiscoveredPairs_;
        } else {
            for (const auto &pair : allDiscoveredPairs_) {
                if (pair.partitionId().value() == selectedPartition.toStdString()) {
                    filteredPairs.push_back(pair);
                }
            }
        }

        // 2. Filter by View Mode
        QString viewMode = ui->cbViewMode->currentData().toString();

        if (viewMode == "full") {
            // Mode A: Full Dataset View
            displayedItems_.reserve(filteredPairs.size());
            for (const auto &pair : filteredPairs) {
                displayedItems_.push_back(PairItemEntry{pair, std::nullopt, ""});
            }
        }
        else if (viewMode == "sample") {
            // Mode B: Auto Sample Mode using PreviewPairSelector
            int countPerPartition = std::max(2, ui->spnSampleCount->value());

            // Group pairs by partition
            std::map<std::string, std::vector<DatasetPair>> partitionGroups;
            for (const auto &pair : filteredPairs) {
                partitionGroups[pair.partitionId().value()].push_back(pair);
            }

            for (const auto &[partId, groupPairs] : partitionGroups) {
                if (groupPairs.empty()) continue;

                int total = static_cast<int>(groupPairs.size());
                if (total <= countPerPartition) {
                    for (const auto &pair : groupPairs) {
                        displayedItems_.push_back(PairItemEntry{pair, std::nullopt, "[AUTO]"});
                    }
                } else {
                    int denom = countPerPartition - 1;
                    int finalIdx = total - 1;
                    for (int i = 0; i < countPerPartition; ++i) {
                        std::size_t idx = static_cast<std::size_t>(i * finalIdx / denom);
                        if (idx < groupPairs.size()) {
                            displayedItems_.push_back(PairItemEntry{groupPairs[idx], std::nullopt, "[AUTO]"});
                        }
                    }
                }
            }
        }
        else if (viewMode == "queue") {
            // Mode C: Selection Queue (Auto Sample + Manual Add)
            int countPerPartition = std::max(2, ui->spnSampleCount->value());

            std::set<std::string> addedPairIds;

            // Add Auto samples
            std::map<std::string, std::vector<DatasetPair>> partitionGroups;
            for (const auto &pair : filteredPairs) {
                partitionGroups[pair.partitionId().value()].push_back(pair);
            }

            for (const auto &[partId, groupPairs] : partitionGroups) {
                if (groupPairs.empty()) continue;

                int total = static_cast<int>(groupPairs.size());
                if (total <= countPerPartition) {
                    for (const auto &pair : groupPairs) {
                        if (addedPairIds.insert(pair.pairId()).second) {
                            displayedItems_.push_back(PairItemEntry{pair, std::nullopt, "[AUTO]"});
                        }
                    }
                } else {
                    int denom = countPerPartition - 1;
                    int finalIdx = total - 1;
                    for (int i = 0; i < countPerPartition; ++i) {
                        std::size_t idx = static_cast<std::size_t>(i * finalIdx / denom);
                        if (idx < groupPairs.size()) {
                            if (addedPairIds.insert(groupPairs[idx].pairId()).second) {
                                displayedItems_.push_back(PairItemEntry{groupPairs[idx], std::nullopt, "[AUTO]"});
                            }
                        }
                    }
                }
            }

            // Add Manual items
            for (const auto &manualPair : manualSelectionQueue_) {
                if (selectedPartition.isEmpty() || manualPair.partitionId().value() == selectedPartition.toStdString()) {
                    if (addedPairIds.insert(manualPair.pairId()).second) {
                        displayedItems_.push_back(PairItemEntry{manualPair, std::nullopt, "[MANUAL]"});
                    }
                }
            }
        }

        // 3. Populate List Widget
        for (std::size_t i = 0; i < displayedItems_.size(); ++i) {
            const auto &itemEntry = displayedItems_[i];
            const auto &pair = itemEntry.pair;

            QString sourcePrefix = itemEntry.sourceTag.isEmpty() ? "" : (itemEntry.sourceTag + " ");
            QString itemText = QString("%1%2 (%3)")
                                   .arg(sourcePrefix)
                                   .arg(QString::fromStdString(pair.sourceStem()))
                                   .arg(QString::fromStdString(pair.pairId()));

            auto *listItem = new QListWidgetItem(itemText, ui->lstImagePairs);
            listItem->setData(Qt::UserRole, static_cast<int>(i));
            listItem->setForeground(QBrush(QColor("#1565C0"))); // Soft blue default
        }

        QString modeName = "Duyệt toàn bộ";
        if (viewMode == "sample") modeName = "Lấy mẫu tự động";
        if (viewMode == "queue") modeName = "Hàng đợi chọn mẫu (Queue)";

        ui->lblPairCount->setText(QString("Tổng số: %1 cặp ảnh (%2)")
                                      .arg(displayedItems_.size())
                                      .arg(modeName));

        ui->lblSystemStatus->setText(QString("Đã tải %1 cặp ảnh (%2 - %3). Chọn một cặp để xem.")
                                         .arg(displayedItems_.size())
                                         .arg(currentDatasetId_.toUpper())
                                         .arg(modeName));

        // Select first item if available
        if (ui->lstImagePairs->count() > 0) {
            ui->lstImagePairs->setCurrentRow(0);
            onPairSelected(ui->lstImagePairs->item(0));
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi áp dụng bộ lọc: %1").arg(ex.what()));
    }
}

void MainWindow::onPairSelected(QListWidgetItem *item)
{
    try {
        if (!item) return;

        int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= static_cast<int>(displayedItems_.size())) {
            return;
        }

        // On-demand validate current selected pair
        PairItemEntry &entry = displayedItems_[index];
        if (!entry.validationResult.has_value()) {
            entry.validationResult = getOrValidatePair(entry.pair);
        }

        // Update list item text and color with actual validated status
        PairValidationStatus status = entry.validationResult->status();
        QString tagText = getStatusTagText(status);
        QString colorHex = getStatusColorHex(status);

        QString sourcePrefix = entry.sourceTag.isEmpty() ? "" : (entry.sourceTag + " ");
        item->setText(QString("[%1] %2%3 (%4)")
                          .arg(tagText)
                          .arg(sourcePrefix)
                          .arg(QString::fromStdString(entry.pair.sourceStem()))
                          .arg(QString::fromStdString(entry.pair.pairId())));
        item->setForeground(QBrush(QColor(colorHex)));

        displayPair(entry);
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi chọn cặp ảnh: %1").arg(ex.what()));
    }
}

void MainWindow::displayPair(const PairItemEntry &entry)
{
    try {
        const auto &pair = entry.pair;
        ui->lblCurrentPairId->setText(QString("Cặp ảnh: %1 (Stem: %2 | Partition: %3)")
                                          .arg(QString::fromStdString(pair.pairId()))
                                          .arg(QString::fromStdString(pair.sourceStem()))
                                          .arg(QString::fromStdString(pair.partitionId().value())));

        PairValidationStatus status = PairValidationStatus::Valid;
        QString descText = "Chưa kiểm định chi tiết.";
        QString messageText = "";

        std::optional<int> visWidth, visHeight, thrWidth, thrHeight;
        bool visDecoded = false, thrDecoded = false;

        if (entry.validationResult.has_value()) {
            const auto &res = *entry.validationResult;
            status = res.status();
            descText = getStatusDescription(status);
            if (res.message().has_value()) {
                messageText = QString::fromStdString(*res.message());
            }
            visDecoded = res.visibleDecoded();
            thrDecoded = res.thermalDecoded();
            if (res.visibleDimensions().has_value()) {
                visWidth = res.visibleDimensions()->width();
                visHeight = res.visibleDimensions()->height();
            }
            if (res.thermalDimensions().has_value()) {
                thrWidth = res.thermalDimensions()->width();
                thrHeight = res.thermalDimensions()->height();
            }
        }

        QString tagText = getStatusTagText(status);
        QString colorHex = getStatusColorHex(status);

        ui->lblPairStatus->setText(QString("Trạng thái: %1").arg(tagText));
        ui->lblPairStatus->setStyleSheet(QString("color: %1; font-weight: bold;").arg(colorHex));

        if (!messageText.isEmpty()) {
            ui->lblPairStatusMessage->setText(QString("Chi tiết: %1 | %2").arg(messageText, descText));
        } else {
            ui->lblPairStatusMessage->setText(QString("Chi tiết: %1").arg(descText));
        }

        // Resolve Full Paths
        std::filesystem::path visFullPath = datasetRoot_ / pair.visibleRelativePath();
        std::filesystem::path thrFullPath = datasetRoot_ / pair.thermalRelativePath();

        // Render Visible Image (A1)
        renderImageToLabel(ui->lblVisibleImage, QString::fromStdString(visFullPath.string()));
        QString visDimStr = (visWidth.has_value() && visHeight.has_value())
                                ? QString("%1 x %2 px").arg(*visWidth).arg(*visHeight)
                                : "--";
        ui->lblVisibleInfo->setText(QString("File: %1\nKích thước: %2 | OpenCV Decode: %3")
                                        .arg(QString::fromStdString(pair.visibleRelativePath()))
                                        .arg(visDimStr)
                                        .arg(visDecoded ? "OK" : (entry.validationResult.has_value() ? "Lỗi" : "Chờ")));

        // Render Thermal Image (A2)
        renderImageToLabel(ui->lblThermalImage, QString::fromStdString(thrFullPath.string()));
        QString thrDimStr = (thrWidth.has_value() && thrHeight.has_value())
                                ? QString("%1 x %2 px").arg(*thrWidth).arg(*thrHeight)
                                : "--";
        ui->lblThermalInfo->setText(QString("File: %1\nKích thước: %2 | OpenCV Decode: %3")
                                        .arg(QString::fromStdString(pair.thermalRelativePath()))
                                        .arg(thrDimStr)
                                        .arg(thrDecoded ? "OK" : (entry.validationResult.has_value() ? "Lỗi" : "Chờ")));
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi hiển thị cặp ảnh: %1").arg(ex.what()));
    }
}

void MainWindow::renderImageToLabel(QLabel *label, const QString &imagePath)
{
    try {
        if (!QFile::exists(imagePath)) {
            label->setText("[!] Không tìm thấy file ảnh trên đĩa");
            label->setPixmap(QPixmap());
            return;
        }

        QPixmap pixmap(imagePath);
        if (pixmap.isNull()) {
            label->setText("[!] Không thể giải mã dữ liệu ảnh");
            label->setPixmap(QPixmap());
            return;
        }

        // Use actual label size so pixmap fits completely inside label without clipping
        QSize targetSize = label->size();
        if (targetSize.width() < 100 || targetSize.height() < 100) {
            QWidget *container = label->parentWidget();
            if (container && container->contentsRect().width() > 100 && container->contentsRect().height() > 100) {
                targetSize = container->contentsRect().size();
                targetSize.setWidth(std::max(100, targetSize.width() - 20));
                targetSize.setHeight(std::max(100, targetSize.height() - 60));
            } else {
                targetSize = QSize(400, 300);
            }
        }

        int targetW = std::max(50, targetSize.width() - 10);
        int targetH = std::max(50, targetSize.height() - 10);

        QPixmap scaledPixmap = pixmap.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        label->setPixmap(scaledPixmap);
    } catch (const std::exception &ex) {
        label->setText(QString("[!] Lỗi hiển thị: %1").arg(ex.what()));
        label->setPixmap(QPixmap());
    }
}

void MainWindow::onSelectSingleImageClicked()
{
    try {
        const QString imageFile = QFileDialog::getOpenFileName(
            this,
            "Chọn 1 tập tin ảnh (Hệ thống sẽ tự động tìm ảnh cặp tương ứng)",
            ui->txtDatasetDir->text().isEmpty() ? QDir::homePath() : ui->txtDatasetDir->text(),
            "Images (*.jpg *.jpeg *.png *.bmp)"
        );

        if (imageFile.isEmpty()) return;

        std::filesystem::path filePath(imageFile.toStdString());
        if (!std::filesystem::is_regular_file(filePath)) {
            QMessageBox::warning(this, "Tập tin không hợp lệ", "Đường dẫn được chọn không phải là tập tin ảnh hợp lệ.");
            return;
        }

        std::filesystem::path resolvedRoot = resolveDatasetRoot(filePath);
        if (!std::filesystem::exists(resolvedRoot / "raw")) {
            QMessageBox::warning(this, "Không tìm thấy Dataset", "Tập tin ảnh không nằm trong cấu trúc thư mục bộ dữ liệu chuẩn (QART_Datasets/raw/...).");
            return;
        }

        // If dataset root changed, load dataset
        if (datasetRoot_ != resolvedRoot) {
            loadDatasetFromDirectory(QString::fromStdString(resolvedRoot.string()));
        }

        // Find canonical pair by source stem or relative path
        std::string stem = filePath.stem().string();
        std::filesystem::path relPath = std::filesystem::relative(filePath, resolvedRoot);
        std::string relPathStr = relPath.generic_string();

        int targetIndex = -1;
        for (std::size_t i = 0; i < allDiscoveredPairs_.size(); ++i) {
            const auto &p = allDiscoveredPairs_[i];
            if (p.sourceStem() == stem || p.visibleRelativePath() == relPathStr || p.thermalRelativePath() == relPathStr) {
                targetIndex = static_cast<int>(i);
                break;
            }
        }

        if (targetIndex >= 0) {
            const auto &foundPair = allDiscoveredPairs_[targetIndex];

            // Switch partition filter if needed
            isUpdatingControls_ = true;
            int partIdx = ui->cbPartitionSelect->findData(QString::fromStdString(foundPair.partitionId().value()));
            if (partIdx >= 0) {
                ui->cbPartitionSelect->setCurrentIndex(partIdx);
            }
            isUpdatingControls_ = false;

            applyFiltersAndModes();

            // Locate in list widget
            for (int i = 0; i < ui->lstImagePairs->count(); ++i) {
                auto *item = ui->lstImagePairs->item(i);
                if (item->text().contains(QString::fromStdString(foundPair.sourceStem()))) {
                    ui->lstImagePairs->setCurrentRow(i);
                    onPairSelected(item);
                    ui->lblSystemStatus->setText(QString("Đã tự động bắt cặp chính xác cho ảnh stem '%1'.").arg(QString::fromStdString(stem)));
                    return;
                }
            }
        } else {
            QMessageBox::information(this, "Không tìm thấy cặp ảnh",
                                     QString("Không tìm thấy cặp ảnh tương ứng cho tập tin '%1' trong bộ dữ liệu.").arg(QString::fromStdString(filePath.filename().string())));
        }
    } catch (const std::exception &ex) {
        QMessageBox::critical(this, "Lỗi Bắt Cặp", QString("Đã xảy ra lỗi khi tìm cặp ảnh:\n%1").arg(ex.what()));
    }
}

void MainWindow::onFullValidationClicked()
{
    try {
        if (allDiscoveredPairs_.empty()) {
            QMessageBox::information(this, "Thông báo", "Chưa có tập dữ liệu nào được mở để kiểm tra toàn bộ.");
            return;
        }

        ui->prgLoading->setVisible(true);
        ui->prgLoading->setValue(0);
        ui->lblSystemStatus->setText("Đang thực hiện kiểm định và giải mã toàn bộ cặp ảnh...");

        std::size_t total = allDiscoveredPairs_.size();
        for (std::size_t i = 0; i < total; ++i) {
            (void)getOrValidatePair(allDiscoveredPairs_[i]);

            if (i % 50 == 0 || i == total - 1) {
                int percent = static_cast<int>((i + 1) * 100 / total);
                ui->prgLoading->setValue(percent);
                QCoreApplication::processEvents();
            }
        }

        ui->prgLoading->setVisible(false);
        ui->lblSystemStatus->setText(QString("Đã hoàn thành kiểm định toàn bộ %1 cặp ảnh!").arg(total));

        // Refresh view with validated tags
        applyFiltersAndModes();
    } catch (const std::exception &ex) {
        ui->prgLoading->setVisible(false);
        QMessageBox::critical(this, "Lỗi Kiểm Định Toàn Bộ", QString("Đã xảy ra lỗi:\n%1").arg(ex.what()));
    }
}

void MainWindow::onAddToQueueClicked()
{
    try {
        if (!ui->lstImagePairs->currentItem()) {
            QMessageBox::information(this, "Thông báo", "Vui lòng chọn 1 cặp ảnh trong danh sách trước khi thêm vào Queue.");
            return;
        }

        int idx = ui->lstImagePairs->currentItem()->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= static_cast<int>(displayedItems_.size())) return;

        const auto &pair = displayedItems_[idx].pair;

        // Check if already in queue
        bool exists = false;
        for (const auto &p : manualSelectionQueue_) {
            if (p.pairId() == pair.pairId()) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            manualSelectionQueue_.push_back(pair);
            ui->lblSystemStatus->setText(QString("Đã thêm cặp '%1' vào Selection Queue (Tổng: %2 cặp).")
                                             .arg(QString::fromStdString(pair.pairId()))
                                             .arg(manualSelectionQueue_.size()));

            if (ui->cbViewMode->currentData().toString() == "queue") {
                applyFiltersAndModes();
            }
        } else {
            ui->lblSystemStatus->setText(QString("Cặp '%1' đã có sẵn trong Queue.").arg(QString::fromStdString(pair.pairId())));
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi thêm vào Queue: %1").arg(ex.what()));
    }
}

void MainWindow::onClearQueueClicked()
{
    try {
        manualSelectionQueue_.clear();
        ui->lblSystemStatus->setText("Đã làm trống Selection Queue.");

        if (ui->cbViewMode->currentData().toString() == "queue") {
            applyFiltersAndModes();
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi xóa Queue: %1").arg(ex.what()));
    }
}

PairValidationStatus MainWindow::safeParseStatus(const std::string &statusStr) const
{
    try {
        return qart::core::domain::validation::pairValidationStatusFromString(statusStr);
    } catch (...) {
        // Explicitly return a non-valid error status when string is unknown
        return PairValidationStatus::VisibleDecodeFailed;
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
            return "FORMAT VISIBLE K.HỖ TRỢ";
        case PairValidationStatus::UnsupportedThermalFormat:
            return "FORMAT THERMAL K.HỖ TRỢ";
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
            return "LỖI KHÔNG XÁC ĐỊNH";
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
            return "Cặp ảnh hợp lệ, đúng quy chuẩn độ phân giải dataset và giải mã thành công.";
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
            return "CẢNH BÁO: Kích thước pixel ảnh Màu và ảnh Nhiệt không khớp nhau.";
        case PairValidationStatus::UnexpectedVisibleDimensions:
            return "Kích thước ảnh màu không đúng với độ phân giải tiêu chuẩn của bộ dữ liệu.";
        case PairValidationStatus::UnexpectedThermalDimensions:
            return "Kích thước ảnh nhiệt không đúng với độ phân giải tiêu chuẩn của bộ dữ liệu.";
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
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi đổi dataset: %1").arg(ex.what()));
    }
}

void MainWindow::onPartitionChanged(int index)
{
    Q_UNUSED(index);
    if (isUpdatingControls_) return;

    try {
        applyFiltersAndModes();
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi đổi partition: %1").arg(ex.what()));
    }
}

void MainWindow::onViewModeChanged(int index)
{
    Q_UNUSED(index);
    if (isUpdatingControls_) return;

    try {
        QString mode = ui->cbViewMode->currentData().toString();
        bool isSampleOrQueue = (mode == "sample" || mode == "queue");
        ui->lblSampleCountLabel->setVisible(isSampleOrQueue);
        ui->spnSampleCount->setVisible(isSampleOrQueue);

        applyFiltersAndModes();
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi đổi chế độ xem: %1").arg(ex.what()));
    }
}

void MainWindow::onSampleCountChanged(int value)
{
    Q_UNUSED(value);
    if (isUpdatingControls_) return;

    try {
        QString mode = ui->cbViewMode->currentData().toString();
        if (mode == "sample" || mode == "queue") {
            applyFiltersAndModes();
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi thay đổi số lượng mẫu: %1").arg(ex.what()));
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
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi tìm kiếm: %1").arg(ex.what()));
    }
}

void MainWindow::onExportCsvClicked()
{
    try {
        if (displayedItems_.empty()) {
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

        // Ensure all displayed items are validated before export
        std::vector<ValidationManifestRow> exportRows;
        exportRows.reserve(displayedItems_.size());

        for (const auto &item : displayedItems_) {
            PairValidationResult res = item.validationResult.has_value()
                                           ? *item.validationResult
                                           : getOrValidatePair(item.pair);
            exportRows.push_back(ValidationManifestRow::fromResult(res));
        }

        ValidationManifest manifest("2.0", exportRows);
        CsvValidationManifestWriter writer;

        const auto writtenPath = writer.write(manifest, saveFile.toStdString(), true);
        (void)writtenPath;
        QMessageBox::information(this, "Thành công", QString("Đã xuất báo cáo CSV thành công (%1 dòng) tại:\n%2")
                                                         .arg(exportRows.size())
                                                         .arg(saveFile));
        ui->lblSystemStatus->setText(QString("Đã xuất báo cáo CSV (%1 cặp): %2").arg(exportRows.size()).arg(saveFile));
    } catch (const std::exception &ex) {
        QMessageBox::critical(this, "Lỗi xuất CSV", QString("Không thể xuất file CSV: %1").arg(ex.what()));
    }
}
