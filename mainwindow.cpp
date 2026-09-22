#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include <QColor>
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QResizeEvent>
#include <algorithm>
#include <filesystem>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::validation::PairValidationResult;
using qart::core::domain::validation::PairValidationStatus;

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
    ui->cbViewMode->addItem("Chế độ B: Lấy mẫu tự động (Deterministic Auto Sample)", "sample");
    ui->cbViewMode->addItem("Chế độ C: Hàng đợi chọn mẫu (Queue: Auto + Manual)", "queue");
    ui->cbViewMode->addItem("Chế độ D: Khám phá ngẫu nhiên (Exploratory Random Preview)", "random");

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

        ui->lblSystemStatus->setText(
            "Đang quét cấu trúc tập dữ liệu..."
        );
        QCoreApplication::processEvents();

        const QString requestedDataset =
            ui->cbDatasetSelect->currentData().toString();

        const bool loaded = appController_.loadDataset(
            std::filesystem::path(dirPath.toStdString()),
            requestedDataset.toStdString()
        );

        if (!loaded) {
            ui->lblSystemStatus->setText(
                "Không thể nhận diện tập dữ liệu hợp lệ."
            );
            ui->lblPairCount->setText("Tổng số: 0 cặp ảnh");
            ui->lblCurrentPairId->setText("Cặp ảnh: Chưa chọn");
            ui->lblPairStatus->setText("Trạng thái: --");
            ui->lblPairStatusMessage->setText("");

            QMessageBox::warning(
                this,
                "Không nhận diện được Dataset",
                QString(
                    "Không thể nhận diện tập dữ liệu chuẩn "
                    "(LLVIP, MSRS, RoadScene) tại đường dẫn:\n%1\n\n"
                    "Vui lòng kiểm tra lại cấu trúc thư mục chứa "
                    "các tập dữ liệu chuẩn!"
                ).arg(dirPath)
            );
            return;
        }

        const QString currentDatasetId =
            QString::fromStdString(
                appController_.currentDatasetId()
            );

        isUpdatingControls_ = true;

        if (requestedDataset.isEmpty()) {
            const int index =
                ui->cbDatasetSelect->findData(currentDatasetId);

            if (index >= 0) {
                ui->cbDatasetSelect->setCurrentIndex(index);
            }
        }

        const QString previousPartition =
            ui->cbPartitionSelect->currentData().toString();

        ui->cbPartitionSelect->clear();

        if (
            currentDatasetId == "llvip"
            || currentDatasetId == "msrs"
        ) {
            ui->cbPartitionSelect->addItem(
                "Tất cả Partition", ""
            );
            ui->cbPartitionSelect->addItem(
                "train", "train"
            );
            ui->cbPartitionSelect->addItem(
                "test", "test"
            );
        } else if (currentDatasetId == "roadscene") {
            ui->cbPartitionSelect->addItem(
                "all (native partition)", "all"
            );
        }

        const int partitionIndex =
            ui->cbPartitionSelect->findData(
                previousPartition
            );

        if (partitionIndex >= 0) {
            ui->cbPartitionSelect->setCurrentIndex(
                partitionIndex
            );
        } else {
            ui->cbPartitionSelect->setCurrentIndex(0);
        }

        isUpdatingControls_ = false;

        applyFiltersAndModes();

    } catch (const std::exception &ex) {
        QMessageBox::critical(
            this,
            "Lỗi Nạp Dataset",
            QString(
                "Đã xảy ra lỗi khi nạp dữ liệu:\n%1"
            ).arg(ex.what())
        );
    }
}

void MainWindow::applyFiltersAndModes()
{
    try {
        ui->lstImagePairs->clear();
        displayedItems_.clear();

        if (appController_.discoveredPairs().empty()) {
            ui->lblPairCount->setText("Tổng số: 0 cặp ảnh");
            ui->lblSystemStatus->setText("Chưa có dữ liệu cặp ảnh.");
            return;
        }

        // 1. Filter by Partition
        QString selectedPartition = ui->cbPartitionSelect->currentData().toString();
        std::vector<DatasetPair> filteredPairs;

        if (selectedPartition.isEmpty()) {
            filteredPairs = appController_.discoveredPairs();
        } else {
            for (const auto &pair : appController_.discoveredPairs()) {
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
            // Mode B: Core PreviewPairSelector (Deterministic Canonical Sampling)
            const int countPerPartition = std::max(2,ui->spnSampleCount->value());

            const auto selection =
                appController_.selectAutomaticPreview(
                    filteredPairs,
                    countPerPartition
                );

            for (
                const auto& result :
                selection.automaticResults()
            ) {
                displayedItems_.push_back(
                    PairItemEntry{
                        result.pair(),
                        result,
                        "[AUTO]"
                    }
                );
            }
        }
        else if (viewMode == "queue") {
            // Mode C: Selection Queue (Auto Sample + Manual Add)
            const int countPerPartition = std::max(2,ui->spnSampleCount->value());

            const auto selection =
                appController_.selectQueuedPreview(
                    filteredPairs,
                    countPerPartition,
                    selectedPartition.toStdString()
                );

            for (
                const auto& result :
                selection.automaticResults()
            ) {
                displayedItems_.push_back(
                    PairItemEntry{
                        result.pair(),
                        result,
                        "[AUTO]"
                    }
                );
            }

            for (
                const auto& result :
                selection.explicitResults()
            ) {
                displayedItems_.push_back(
                    PairItemEntry{
                        result.pair(),
                        result,
                        "[MANUAL]"
                    }
                );
            }
        }
        else if (viewMode == "random") {
            // Mode D: Exploratory Random Preview (Deterministic via fixed seed per count)
            int countPerPartition = std::max(2, ui->spnSampleCount->value());

            std::map<std::string, std::vector<DatasetPair>> partitionGroups;
            for (const auto &pair : filteredPairs) {
                partitionGroups[pair.partitionId().value()].push_back(pair);
            }

            std::mt19937 g(42 + countPerPartition);

            for (auto &[partId, groupPairs] : partitionGroups) {
                if (groupPairs.empty()) continue;

                std::vector<DatasetPair> shuffled = groupPairs;
                std::shuffle(shuffled.begin(), shuffled.end(), g);

                int takeCount = std::min(countPerPartition, static_cast<int>(shuffled.size()));
                for (int i = 0; i < takeCount; ++i) {
                    displayedItems_.push_back(PairItemEntry{shuffled[i], std::nullopt, "[EXPLORE]"});
                }
            }
        }

        // 3. Populate List Widget
        for (std::size_t i = 0; i < displayedItems_.size(); ++i) {
            const auto &itemEntry = displayedItems_[i];
            const auto &pair = itemEntry.pair;

            QString sourcePrefix = itemEntry.sourceTag.isEmpty() ? "" : (itemEntry.sourceTag + " ");
            QString inQueueSuffix = appController_.isManuallyQueued(pair.pairId()) ? " [IN QUEUE]" : "";

            QString itemText = QString("%1%2 (%3)%4")
                                   .arg(sourcePrefix, QString::fromStdString(pair.sourceStem()), QString::fromStdString(pair.pairId()), inQueueSuffix);

            auto *listItem = new QListWidgetItem(itemText, ui->lstImagePairs);
            listItem->setData(Qt::UserRole, static_cast<int>(i));
            listItem->setForeground(QBrush(QColor(0x1565C0))); // Soft blue default
        }

        QString modeName = "Duyệt toàn bộ";
        if (viewMode == "sample") modeName = "Lấy mẫu tự động";
        if (viewMode == "queue") modeName = "Hàng đợi chọn mẫu (Queue)";
        if (viewMode == "random") modeName = "Khám phá ngẫu nhiên";

        ui->lblPairCount->setText(QString("Tổng số: %1 cặp ảnh (%2)")
                                      .arg(displayedItems_.size())
                                      .arg(modeName));

        ui->lblSystemStatus->setText(QString("Đã tải %1 cặp ảnh (%2 - %3). Chọn một cặp để xem.")
                                         .arg(displayedItems_.size())
                                         .arg(QString::fromStdString(
                                                appController_.currentDatasetId()
                                            ).toUpper(), modeName));

        // Select first item if available
        if (ui->lstImagePairs->count() > 0) {
            ui->lstImagePairs->setCurrentRow(0);
            onPairSelected(ui->lstImagePairs->item(0));
        }
    } catch (const std::exception &ex) {
        const QString failedMode = ui->cbViewMode->currentData().toString();
        if (failedMode == "sample" || failedMode == "queue") {
            ui->lblPairCount->setText("Mode B/C không khả dụng");
        }
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
            entry.validationResult = appController_.getOrValidatePair(entry.pair);
        }

        // Update list item text and color with actual validated status
        PairValidationStatus status = entry.validationResult->status();
        QString tagText = getStatusTagText(status);
        QString colorHex = getStatusColorHex(status);

        // Check if in manual queue
        const bool isManualQueued =
            appController_.isManuallyQueued(
                entry.pair.pairId()
            );
        QString inQueueSuffix = isManualQueued ? " [IN QUEUE]" : "";

        QString sourcePrefix = entry.sourceTag.isEmpty() ? "" : (entry.sourceTag + " ");
        item->setText(QString("[%1] %2%3 (%4)%5")
                          .arg(tagText, sourcePrefix, QString::fromStdString(entry.pair.sourceStem()), QString::fromStdString(entry.pair.pairId()), inQueueSuffix));
        item->setForeground(QBrush(QColor(colorHex)));

        displayPair(entry);
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi chọn cặp ảnh: %1").arg(ex.what()));
    }
}

void MainWindow::displayPair(const PairItemEntry &entry) const {
    try {
        const auto &pair = entry.pair;
        ui->lblCurrentPairId->setText(QString("Cặp ảnh: %1 (Stem: %2 | Partition: %3)")
                                          .arg(QString::fromStdString(pair.pairId()), QString::fromStdString(pair.sourceStem()), QString::fromStdString(pair.partitionId().value())));

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
        ui->lblPairStatus->setStyleSheet(QString("background-color: %1; color: #FFFFFF; padding: 4px 10px; border-radius: 4px; font-weight: bold;")
                                             .arg(colorHex));

        if (!messageText.isEmpty()) {
            ui->lblPairStatusMessage->setText(QString("Chi tiết: %1 | %2").arg(messageText, descText));
        } else {
            ui->lblPairStatusMessage->setText(QString("Chi tiết: %1").arg(descText));
        }

        // Resolve Full Paths
        std::filesystem::path visFullPath = appController_.datasetRoot() / pair.visibleRelativePath();
        std::filesystem::path thrFullPath = appController_.datasetRoot() / pair.thermalRelativePath();

        // Render Visible Image (A1)
        renderImageToLabel(ui->lblVisibleImage, QString::fromStdString(visFullPath.string()));
        QString visDimStr = (visWidth.has_value() && visHeight.has_value())
                                ? QString("%1 x %2 px").arg(*visWidth).arg(*visHeight)
                                : "--";
        ui->lblVisibleInfo->setText(QString("File: %1\nKích thước: %2 | OpenCV Decode: %3")
                                        .arg(QString::fromStdString(pair.visibleRelativePath()), visDimStr)
                                        .arg(visDecoded ? "OK" : (entry.validationResult.has_value() ? "Lỗi" : "Chờ")));

        // Render Thermal Image (A2)
        renderImageToLabel(ui->lblThermalImage, QString::fromStdString(thrFullPath.string()));
        QString thrDimStr = (thrWidth.has_value() && thrHeight.has_value())
                                ? QString("%1 x %2 px").arg(*thrWidth).arg(*thrHeight)
                                : "--";
        ui->lblThermalInfo->setText(QString("File: %1\nKích thước: %2 | OpenCV Decode: %3")
                                        .arg(QString::fromStdString(pair.thermalRelativePath()), thrDimStr)
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
            label->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
            label->setPixmap(QPixmap());
            return;
        }

        QPixmap pixmap(imagePath);
        if (pixmap.isNull()) {
            label->setText("[!] Không thể giải mã dữ liệu ảnh");
            label->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
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
        label->setStyleSheet("background-color: #1e1e1e; color: #888888; border: 1px solid #333333; border-radius: 6px;");
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
            ui->txtDatasetDir->text().isEmpty()
                ? QDir::homePath()
                : ui->txtDatasetDir->text(),
            "Images (*.jpg *.jpeg *.png *.bmp)"
        );

        if (imageFile.isEmpty()) {
            return;
        }

        const std::filesystem::path filePath(
            imageFile.toStdString()
        );

        if (!std::filesystem::is_regular_file(filePath)) {
            QMessageBox::warning(
                this,
                "Tập tin không hợp lệ",
                "Đường dẫn được chọn không phải là tập tin ảnh hợp lệ."
            );
            return;
        }

        const auto lookup =
            appController_.lookupCanonicalImage(
                filePath
            );

        if (
            lookup.status
            == qart::application::
                SingleImageLookupStatus::UnsupportedDataset
        ) {
            QMessageBox::warning(
                this,
                "Không nhận diện được Dataset",
                QString(
                    "Tập tin ảnh '%1' không thuộc các bộ dữ liệu "
                    "được hỗ trợ (LLVIP, MSRS, RoadScene)."
                ).arg(
                    QString::fromStdString(
                        filePath.filename().string()
                    )
                )
            );
            return;
        }

        if (
            lookup.status
            == qart::application::
                SingleImageLookupStatus::DatasetLoadFailed
        ) {
            QMessageBox::warning(
                this,
                "Không thể tải Dataset",
                QString(
                    "Không thể tải dataset '%1' từ đường dẫn:\n%2"
                )
                    .arg(QString::fromStdString(
                            lookup.datasetId
                        ).toUpper(), QString::fromStdString(
                            lookup.datasetRoot.string()
                        ))
            );
            return;
        }

        if (
            lookup.status
            == qart::application::
                SingleImageLookupStatus::NonCanonicalInput
        ) {
            QMessageBox::information(
                this,
                "Ảnh không phải canonical input",
                QString(
                    "Tập tin '%1' không nằm trong modality input "
                    "canonical của %2. Ảnh auxiliary sẽ không được "
                    "ghép chỉ dựa trên stem."
                )
                    .arg(QString::fromStdString(
                            filePath.filename().string()
                        ), QString::fromStdString(
                            lookup.datasetId
                        ).toUpper())
            );
            return;
        }

        if (!lookup.pair.has_value()) {
            throw std::runtime_error(
                "Resolved single-image lookup returned no DatasetPair."
            );
        }

        if (lookup.datasetSessionChanged) {
            ui->txtDatasetDir->setText(
                QString::fromStdString(
                    lookup.datasetRoot.string()
                )
            );

            isUpdatingControls_ = true;

            const QString datasetId =
                QString::fromStdString(
                    lookup.datasetId
                );

            const int datasetIndex =
                ui->cbDatasetSelect->findData(
                    datasetId
                );

            if (datasetIndex >= 0) {
                ui->cbDatasetSelect->setCurrentIndex(
                    datasetIndex
                );
            }

            ui->cbPartitionSelect->clear();

            if (
                datasetId == "llvip"
                || datasetId == "msrs"
            ) {
                ui->cbPartitionSelect->addItem(
                    "Tất cả Partition",
                    ""
                );
                ui->cbPartitionSelect->addItem(
                    "train",
                    "train"
                );
                ui->cbPartitionSelect->addItem(
                    "test",
                    "test"
                );
            } else {
                ui->cbPartitionSelect->addItem(
                    "all (native partition)",
                    "all"
                );
            }

            isUpdatingControls_ = false;

            displayedItems_.clear();

            applyFiltersAndModes();
        }

        const DatasetPair& foundPair =
            *lookup.pair;

        const std::string stem =
            filePath.stem().string();

        const QString targetPartition =
            QString::fromStdString(
                foundPair.partitionId().value()
            );

        if (
            !ui->cbPartitionSelect
                 ->currentData()
                 .toString()
                 .isEmpty()
            &&
            ui->cbPartitionSelect
                    ->currentData()
                    .toString()
                != targetPartition
        ) {
            isUpdatingControls_ = true;

            const int partitionIndex =
                ui->cbPartitionSelect->findData(
                    targetPartition
                );

            if (partitionIndex >= 0) {
                ui->cbPartitionSelect->setCurrentIndex(
                    partitionIndex
                );
            } else {
                ui->cbPartitionSelect->setCurrentIndex(0);
            }

            isUpdatingControls_ = false;

            applyFiltersAndModes();
        }

        bool foundInList = false;

        for (
            int i = 0;
            i < ui->lstImagePairs->count();
            ++i
        ) {
            auto* item =
                ui->lstImagePairs->item(i);

            const int index =
                item->data(Qt::UserRole).toInt();

            if (
                index >= 0
                && index
                    < static_cast<int>(
                        displayedItems_.size()
                    )
                && displayedItems_[index]
                       .pair
                       .pairId()
                    == foundPair.pairId()
            ) {
                ui->lstImagePairs->setCurrentRow(i);

                onPairSelected(item);

                ui->lblSystemStatus->setText(
                    QString(
                        "Đã tự động bắt cặp chính xác cho ảnh stem "
                        "'%1' (%2/%3)."
                    )
                        .arg(QString::fromStdString(
                                stem
                            ), QString::fromStdString(
                                foundPair.datasetId()
                            ), QString::fromStdString(
                                foundPair
                                    .partitionId()
                                    .value()
                            ))
                );

                foundInList = true;
                break;
            }
        }

        if (!foundInList) {
            isUpdatingControls_ = true;

            const int fullModeIndex =
                ui->cbViewMode->findData(
                    "full"
                );

            if (fullModeIndex >= 0) {
                ui->cbViewMode->setCurrentIndex(
                    fullModeIndex
                );
            }

            isUpdatingControls_ = false;

            applyFiltersAndModes();

            for (
                int i = 0;
                i < ui->lstImagePairs->count();
                ++i
            ) {
                auto* item =
                    ui->lstImagePairs->item(i);

                const int index =
                    item->data(Qt::UserRole).toInt();

                if (
                    index >= 0
                    && index
                        < static_cast<int>(
                            displayedItems_.size()
                        )
                    && displayedItems_[index]
                           .pair
                           .pairId()
                        == foundPair.pairId()
                ) {
                    ui->lstImagePairs->setCurrentRow(i);

                    onPairSelected(item);

                    ui->lblSystemStatus->setText(
                        QString(
                            "Đã tự động bắt cặp chính xác cho ảnh stem "
                            "'%1' (%2/%3)."
                        )
                            .arg(QString::fromStdString(
                                    stem
                                ), QString::fromStdString(
                                    foundPair.datasetId()
                                ), QString::fromStdString(
                                    foundPair
                                        .partitionId()
                                        .value()
                                ))
                    );

                    break;
                }
            }
        }
    } catch (const std::exception &ex) {
        QMessageBox::critical(
            this,
            "Lỗi Bắt Cặp",
            QString(
                "Đã xảy ra lỗi khi tìm cặp ảnh:\n%1"
            ).arg(ex.what())
        );
    }
}

void MainWindow::onFullValidationClicked()
{
    try {
        if (appController_.discoveredPairs().empty()) {
            QMessageBox::information(
                this,
                "Thông báo",
                "Chưa có tập dữ liệu nào được mở để kiểm tra toàn bộ."
            );
            return;
        }

        ui->prgLoading->setVisible(true);
        ui->prgLoading->setValue(0);

        ui->lblSystemStatus->setText(
            "Đang thực hiện kiểm định và giải mã toàn bộ cặp ảnh..."
        );

        const std::size_t total =
            appController_.validateAllPairs(
                [this](
                    const std::size_t completed,
                    const std::size_t pairCount
                ) {
                    if (
                        completed % 50 == 1
                        || completed == pairCount
                    ) {
                        const int percent =
                            static_cast<int>(
                                completed * 100
                                / pairCount
                            );

                        ui->prgLoading->setValue(
                            percent
                        );

                        QCoreApplication::processEvents();
                    }
                }
            );

        ui->prgLoading->setVisible(false);

        ui->lblSystemStatus->setText(
            QString(
                "Đã hoàn thành kiểm định toàn bộ %1 cặp ảnh!"
            ).arg(total)
        );

        applyFiltersAndModes();
    } catch (const std::exception &ex) {
        ui->prgLoading->setVisible(false);

        QMessageBox::critical(
            this,
            "Lỗi Kiểm Định Toàn Bộ",
            QString(
                "Đã xảy ra lỗi:\n%1"
            ).arg(ex.what())
        );
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

        const auto& pair = displayedItems_[idx].pair;

        const bool added =
            appController_.addManualSelection(
                pair
            );

        if (added) {
            ui->lblSystemStatus->setText(
                QString(
                    "Đã thêm cặp '%1' vào Selection Queue "
                    "(Tổng: %2 cặp)."
                )
                    .arg(
                        QString::fromStdString(
                            pair.pairId()
                        )
                    )
                    .arg(
                        appController_
                            .manualSelectionCount()
                    )
            );

            applyFiltersAndModes();
        } else {
            ui->lblSystemStatus->setText(
                QString(
                    "Cặp '%1' đã có sẵn trong Queue."
                ).arg(
                    QString::fromStdString(
                        pair.pairId()
                    )
                )
            );
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi thêm vào Queue: %1").arg(ex.what()));
    }
}

void MainWindow::onClearQueueClicked()
{
    try {
        appController_.clearManualSelectionQueue();
        ui->lblSystemStatus->setText("Đã làm trống Selection Queue.");

        applyFiltersAndModes();
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi xóa Queue: %1").arg(ex.what()));
    }
}

QString MainWindow::getStatusTagText(PairValidationStatus status) {
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

QString MainWindow::getStatusColorHex(PairValidationStatus status) {
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

QString MainWindow::getStatusDescription(PairValidationStatus status) {
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
        bool isSampleOrQueue = (mode == "sample" || mode == "queue" || mode == "random");
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
        if (mode == "sample" || mode == "queue" || mode == "random") {
            applyFiltersAndModes();
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi thay đổi số lượng mẫu: %1").arg(ex.what()));
    }
}

void MainWindow::onSearchTextChanged(const QString &query) const {
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
            QMessageBox::information(
                this,
                "Thông báo",
                "Chưa có dữ liệu cặp ảnh để xuất báo cáo."
            );
            return;
        }

        const QString defaultPath =
            QString::fromStdString(
                appController_
                    .defaultValidationReportPath()
                    .string()
            );

        const QString saveFile =
            QFileDialog::getSaveFileName(
                this,
                "Xuất báo cáo Manifest CSV",
                defaultPath,
                "CSV Files (*.csv)"
            );

        if (saveFile.isEmpty()) {
            return;
        }

        std::vector<
            qart::application::ValidationExportItem
        > exportItems;

        exportItems.reserve(
            displayedItems_.size()
        );

        for (const auto& item : displayedItems_) {
            exportItems.push_back(
                qart::application::ValidationExportItem{
                    item.pair,
                    item.validationResult
                }
            );
        }

        const auto result =
            appController_.exportValidationReport(
                exportItems,
                std::filesystem::path(
                    saveFile.toStdString()
                )
            );

        QMessageBox::information(
            this,
            "Thành công",
            QString(
                "Đã xuất báo cáo CSV thành công "
                "(%1 dòng) tại:\n%2"
            )
                .arg(result.rowCount)
                .arg(
                    QString::fromStdString(
                        result.writtenPath.string()
                    )
                )
        );

        ui->lblSystemStatus->setText(
            QString(
                "Đã xuất báo cáo CSV (%1 cặp): %2"
            )
                .arg(result.rowCount)
                .arg(
                    QString::fromStdString(
                        result.writtenPath.string()
                    )
                )
        );
    } catch (const std::exception &ex) {
        QMessageBox::critical(
            this,
            "Lỗi xuất CSV",
            QString(
                "Không thể xuất file CSV: %1"
            ).arg(ex.what())
        );
    }
}
