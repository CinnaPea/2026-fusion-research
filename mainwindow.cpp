#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include <QColor>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTime>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using qart::core::domain::datasets::DatasetPair;
using qart::core::domain::validation::PairValidationResult;
using qart::core::domain::validation::PairValidationStatus;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("QART-Fusion");

    setupUiControls();

    // Splitter layout settings: give maximum space to image viewer
    ui->splitterMain->setStretchFactor(0, 0);
    ui->splitterMain->setStretchFactor(1, 1);
    ui->splitterMain->setSizes({280, 940});

    // Tooltips on 5 tabs with Phase details
    ui->tabMainWorkspaces->setTabToolTip(0, "Phase 1 - Khám phá, kiểm định và bắt cặp tập dữ liệu");
    ui->tabMainWorkspaces->setTabToolTip(1, "Phase 2 - Đánh giá chất lượng và tăng cường ảnh nhiệt (N5 / BYPASS)");
    ui->tabMainWorkspaces->setTabToolTip(2, "Phase 3 - Căn chỉnh đồng bộ không gian Visual-Thermal (Chưa triển khai)");
    ui->tabMainWorkspaces->setTabToolTip(3, "Phase 4 - Hợp nhất đa phổ đa cảm biến (Chưa triển khai)");
    ui->tabMainWorkspaces->setTabToolTip(4, "Phase 5/6 - Đánh giá định lượng và xuất báo cáo kết quả (Chưa triển khai)");

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
    connect(ui->btnToggleCollapse, &QPushButton::clicked, this, &MainWindow::onToggleCollapseClicked);

    // Duyệt qua cặp ảnh nào (chuột hoặc phím mũi tên) -> lập tức preview ngay trên UI
    connect(ui->lstImagePairs, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current, QListWidgetItem *previous) {
        Q_UNUSED(previous);
        if (current) {
            onPairSelected(current);
        }
    });
    connect(ui->lstImagePairs, &QListWidget::itemClicked, this, &MainWindow::onPairSelected);
    connect(ui->txtSearchPair, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);

    // Export Context Menu
    auto *exportMenu = new QMenu(this);
    auto *actManifest = exportMenu->addAction("Manifest dữ liệu (CSV) — Phase 1");
    connect(actManifest, &QAction::triggered, this, &MainWindow::onExportCsvClicked);
    auto *actPhase2 = exportMenu->addAction("Báo cáo kết quả Tăng cường (CSV) — Phase 2");
    connect(actPhase2, &QAction::triggered, this, &MainWindow::onExportPhase2Clicked);
    ui->btnExportCsv->setMenu(exportMenu);

    connect(ui->cbDatasetSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDatasetChanged);
    connect(ui->cbPartitionSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPartitionChanged);
    connect(ui->cbViewMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onViewModeChanged);
    connect(ui->spnSampleCount, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSampleCountChanged);

    // Tab and N5 Connections
    connect(ui->tabMainWorkspaces, &QTabWidget::currentChanged, this, &MainWindow::onWorkspaceTabChanged);
    connect(ui->btnApplyN5, &QPushButton::clicked, this, &MainWindow::onApplyN5Clicked);
    connect(ui->btnEnhanceAll, &QPushButton::clicked, this, &MainWindow::onEnhanceAllClicked);
    connect(ui->btnResetN5, &QPushButton::clicked, this, &MainWindow::onResetN5Clicked);

    // Synchronized Viewport connections between Thermal original and Enhanced viewer
    connect(ui->viewerN5Original, &InteractiveImageViewer::viewportChanged, this, [this](double zoom, QPointF offset) {
        if (ui->chkSyncView->isChecked()) {
            ui->viewerN5Enhanced->setSyncViewport(zoom, offset);
        }
    });
    connect(ui->viewerN5Enhanced, &InteractiveImageViewer::viewportChanged, this, [this](double zoom, QPointF offset) {
        if (ui->chkSyncView->isChecked()) {
            ui->viewerN5Original->setSyncViewport(zoom, offset);
        }
    });

    // Modality locked: radN5Thermal only
    connect(ui->radN5Thermal, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && ui->lstImagePairs->currentItem()) {
            int idx = ui->lstImagePairs->currentItem()->data(Qt::UserRole).toInt();
            if (idx >= 0 && idx < static_cast<int>(displayedItems_.size())) {
                updateQualityN5Tab(displayedItems_[idx]);
            }
        }
    });

    connect(ui->radModeN5, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && n5State_ == N5UiState::NotRun) {
            setN5UiState(N5UiState::NotRun);
        }
    });
    connect(ui->radModeBypass, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && n5State_ == N5UiState::NotRun) {
            setN5UiState(N5UiState::NotRun);
        }
    });

    setN5UiState(N5UiState::NotRun);
    appendLog("Hệ thống khởi động thành công. Sẵn sàng.");
}

MainWindow::~MainWindow()
{
    if (singleWorker_) {
        if (singleWorker_->isRunning()) {
            singleWorker_->requestInterruption();
            singleWorker_->wait(1000);
        }
        delete singleWorker_;
        singleWorker_ = nullptr;
    }
    if (batchWorker_) {
        if (batchWorker_->isRunning()) {
            batchWorker_->requestCancel();
            batchWorker_->wait(2000);
        }
        delete batchWorker_;
        batchWorker_ = nullptr;
    }
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
    ui->cbDatasetSelect->addItem("M3FD Dataset", "m3fd");
    ui->cbDatasetSelect->addItem("TNO Dataset", "tno");
    ui->cbDatasetSelect->addItem("DVTOD Dataset", "dvtod");

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
        if (!ui->tabMainWorkspaces) return;

        if (ui->tabMainWorkspaces->currentIndex() == 0) {
            if (ui->lstImagePairs->currentItem()) {
                int idx = ui->lstImagePairs->currentItem()->data(Qt::UserRole).toInt();
                if (idx >= 0 && idx < static_cast<int>(displayedItems_.size())) {
                    displayPair(displayedItems_[idx]);
                }
            }
        } else if (ui->tabMainWorkspaces->currentIndex() == 1) {
            // Resize in Tab 2 ONLY redraws from cache - zero recomputation!
            renderCachedN5Images();
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
            "Chọn thư mục bộ dữ liệu (LLVIP, MSRS, RoadScene, M3FD, TNO, DVTOD...)",
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
                    "(LLVIP, MSRS, RoadScene, M3FD, TNO, DVTOD) tại đường dẫn:\n%1\n\n"
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
        } else if (currentDatasetId == "dvtod") {
            ui->cbPartitionSelect->addItem(
                "Tất cả Partition", ""
            );
            ui->cbPartitionSelect->addItem(
                "train", "train"
            );
            ui->cbPartitionSelect->addItem(
                "val", "val"
            );
        } else if (
            currentDatasetId == "roadscene"
            || currentDatasetId == "m3fd"
            || currentDatasetId == "tno"
        ) {
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

        if (
            appController_.currentDatasetId() == "dvtod"
            && (
                viewMode == "sample"
                || viewMode == "queue"
            )
        ) {
            throw std::runtime_error(
                "DVTOD raw pairs are source-valid but not same-geometry VALID. "
                "Standard Mode B/C is intentionally unavailable before "
                "Phase 3 registration."
            );
        }

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

        // Update Tab 2 and discard stale N5 cache only when switching to a different pair
        const QString newPairId = QString::fromStdString(entry.pair.pairId());
        if (newPairId != activeN5PairId_ || cachedN5OriginalPixmap_.isNull()) {
            updateQualityN5Tab(entry);
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi chọn cặp ảnh: %1").arg(ex.what()));
    }
}

void MainWindow::displayPair(const PairItemEntry &entry) {
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
        renderImageToLabel(ui->lblVisibleImage, QString::fromStdString(visFullPath.string()), "VIS");
        QString visDimStr = (visWidth.has_value() && visHeight.has_value())
                                ? QString("%1 x %2 px").arg(*visWidth).arg(*visHeight)
                                : "--";
        ui->lblVisibleInfo->setText(QString("File: %1\nKích thước: %2 | OpenCV Decode: %3")
                                        .arg(QString::fromStdString(pair.visibleRelativePath()), visDimStr)
                                        .arg(visDecoded ? "OK" : (entry.validationResult.has_value() ? "Lỗi" : "Chờ")));

        // Render Thermal Image (A2)
        renderImageToLabel(ui->lblThermalImage, QString::fromStdString(thrFullPath.string()), "IR");
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

void MainWindow::renderImageToLabel(QLabel *label, const QString &imagePath, const QString &modality)
{
    try {
        if (!QFile::exists(imagePath)) {
            label->setText("[!] Không tìm thấy file ảnh trên đĩa");
            label->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
            label->setPixmap(QPixmap());
            appendLog(QString("Failed to load %1 image: %2").arg(modality, QFileInfo(imagePath).fileName()));
            return;
        }

        QPixmap pixmap(imagePath);
        if (pixmap.isNull()) {
            label->setText("[!] Không thể giải mã dữ liệu ảnh");
            label->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
            label->setPixmap(QPixmap());
            appendLog(QString("Rejected %1 file: %2").arg(modality, QFileInfo(imagePath).fileName()));
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
        appendLog(QString("Failed to render %1 image: %2").arg(modality, ex.what()));
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
                    "được hỗ trợ (LLVIP, MSRS, RoadScene, M3FD, TNO, DVTOD)."
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
            } else if (datasetId == "dvtod") {
                ui->cbPartitionSelect->addItem(
                    "Tất cả Partition",
                    ""
                );
                ui->cbPartitionSelect->addItem(
                    "train",
                    "train"
                );
                ui->cbPartitionSelect->addItem(
                    "val",
                    "val"
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
            return "CẢNH BÁO: Cả hai ảnh nguồn giải mã được nhưng không cùng hình học pixel. Cặp nguồn chưa sẵn sàng cho xử lý yêu cầu cùng kích thước; cần căn chỉnh/đăng ký nếu pipeline yêu cầu.";
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

std::optional<Phase2SourceSelection>
MainWindow::selectedPhase2Source() const
{
    const QListWidgetItem* item =
        ui->lstImagePairs->currentItem();

    if (!item || item->isHidden()) {
        return std::nullopt;
    }

    bool validIndex = false;

    const int index =
        item->data(Qt::UserRole).toInt(&validIndex);

    if (
        !validIndex
        || index < 0
        || static_cast<std::size_t>(index)
            >= displayedItems_.size()
    ) {
        return std::nullopt;
    }

    const PairItemEntry& entry =
        displayedItems_[static_cast<std::size_t>(index)];

    if (
        !entry.validationResult.has_value()
        || entry.validationResult->status()
            != PairValidationStatus::Valid
        || !entry.validationResult->thermalDecoded()
    ) {
        return std::nullopt;
    }

    const auto& root = appController_.datasetRoot();
    const auto& datasetId = appController_.currentDatasetId();

    if (root.empty() || datasetId.empty()) {
        return std::nullopt;
    }

    return Phase2SourceSelection{
        QString::fromStdString(datasetId),
        QString::fromStdString(entry.pair.pairId()),
        root / entry.pair.thermalRelativePath()
    };
}

void MainWindow::onWorkspaceTabChanged(int index)
{
    try {
        if (index == 0) {
            // Tab 1: Dữ liệu (Original Pair)
            if (ui->lstImagePairs->currentItem()) {
                int idx = ui->lstImagePairs->currentItem()->data(Qt::UserRole).toInt();
                if (idx >= 0 && idx < static_cast<int>(displayedItems_.size())) {
                    displayPair(displayedItems_[idx]);
                }
            }
        } else if (index == 1) {
            // Tab 2: Quality & N5 - Vẽ lại từ bộ đệm (cache) mà không chạy lại thuật toán N5!
            renderCachedN5Images();
        }
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi đổi tab: %1").arg(ex.what()));
    }
}

void MainWindow::onToggleCollapseClicked()
{
    if (!ui->widgetFilterBar) return;
    const bool willBeVisible = !ui->widgetFilterBar->isVisible();
    ui->widgetFilterBar->setVisible(willBeVisible);
    ui->btnToggleCollapse->setText(willBeVisible ? "▲ Thu gọn" : "▼ Mở rộng");
}

void MainWindow::appendLog(const QString &message)
{
    const QString timeStr = QTime::currentTime().toString("HH:mm:ss");
    const QString formatted = QString("[%1] %2").arg(timeStr, message);
    if (ui->txtLogConsole) {
        ui->txtLogConsole->appendPlainText(formatted);
        auto *bar = ui->txtLogConsole->verticalScrollBar();
        if (bar) {
            bar->setValue(bar->maximum());
        }
    }
}

void MainWindow::onApplyN5Clicked()
{
    try {
        if (activeN5PairId_.isEmpty()) {
            QMessageBox::information(this, "Thông báo", "Vui lòng chọn một cặp ảnh trước khi chạy.");
            return;
        }

        if (ui->lstImagePairs->currentItem() == nullptr) {
            return;
        }

        int idx = ui->lstImagePairs->currentItem()->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= static_cast<int>(displayedItems_.size())) {
            return;
        }

        const auto &entry = displayedItems_[idx];
        const auto &pair = entry.pair;
        std::filesystem::path fullPath = appController_.datasetRoot() / pair.thermalRelativePath();

        const auto mode = ui->radModeBypass->isChecked()
                              ? qart::application::ThermalProcessingMode::Bypass
                              : qart::application::ThermalProcessingMode::N5PostHalf;
        const QString modeStr = (mode == qart::application::ThermalProcessingMode::Bypass) ? "BYPASS" : "N5";

        // 1. Chuyển UI sang trạng thái Processing và khóa nút Apply chống spam click
        setN5UiState(N5UiState::Processing);
        ui->viewerN5Enhanced->clearImage(QString("Đang xử lý worker thread (%1)...").arg(modeStr));
        appendLog(QString("Started enhancement for pair %1 (Method: %2)").arg(activeN5PairId_, modeStr));

        // 2. Chạy tác vụ xử lý trên worker thread thật (không chạy trên GUI thread)
        if (singleWorker_) {
            if (singleWorker_->isRunning()) {
                singleWorker_->requestInterruption();
                singleWorker_->wait(500);
            }
            delete singleWorker_;
            singleWorker_ = nullptr;
        }

        EnhanceJobRequest req;
        req.pairId = activeN5PairId_;
        req.thermalFullPath = fullPath;
        req.mode = mode;

        singleWorker_ = new SingleEnhanceWorkerThread(req, this);
        connect(singleWorker_, &SingleEnhanceWorkerThread::jobFinished,
                this, &MainWindow::onSingleWorkerFinished);
        singleWorker_->start();
    } catch (const std::exception &ex) {
        setN5UiState(N5UiState::Error, 0, ex.what());
        QMessageBox::critical(this, "Lỗi xử lý", QString("Đã xảy ra lỗi khi kích hoạt xử lý:\n%1").arg(ex.what()));
    }
}

void MainWindow::onSingleWorkerFinished(const QString &pairId,
                                        const QImage &resultImage,
                                        qint64 elapsedMs,
                                        bool success,
                                        const QString &errorMessage,
                                        int mode)
{
    // STALE INVALIDATION: Nếu người dùng đã chọn cặp ảnh khác trong lúc worker đang chạy,
    // kết quả cũ sẽ bị loại bỏ hoàn toàn, không đè lên cặp mới!
    if (pairId != activeN5PairId_) {
        appendLog(QString("Dropped stale result for pair %1 (active pair is %2).").arg(pairId, activeN5PairId_));
        return;
    }

    const QString modeStr = (mode == static_cast<int>(qart::application::ThermalProcessingMode::Bypass)) ? "BYPASS" : "N5";

    if (success && !resultImage.isNull()) {
        cachedN5EnhancedPixmap_ = QPixmap::fromImage(resultImage);
        ui->viewerN5Enhanced->setPixmap(cachedN5EnhancedPixmap_);
        ui->viewerN5Enhanced->fitToView();

        if (ui->chkSyncView->isChecked()) {
            ui->viewerN5Enhanced->setSyncViewport(ui->viewerN5Original->zoomFactor(), ui->viewerN5Original->panOffset());
        }

        setN5UiState(N5UiState::Completed, elapsedMs);
        appendLog(QString("Finished enhancement for pair %1 in %2 ms.").arg(pairId).arg(elapsedMs));

        // Lưu kết quả thực vào danh sách Phase 2
        Phase2Record rec;
        rec.pairId = pairId;
        rec.datasetId = QString::fromStdString(appController_.currentDatasetId());
        rec.processingMethod = modeStr;
        rec.processingVersion = (modeStr == "BYPASS") ? "BYPASS-v1.0" : "N5PostHalf-v1.0";
        rec.status = "SUCCESS";
        rec.elapsedMs = elapsedMs;
        phase2Results_.push_back(rec);
    } else {
        cachedN5EnhancedPixmap_ = QPixmap();
        ui->viewerN5Enhanced->clearImage(QString("Lỗi: %1").arg(errorMessage));
        setN5UiState(N5UiState::Error, elapsedMs, errorMessage);
        appendLog(QString("Failed to process %1 for pair %2: %3").arg(modeStr, pairId, errorMessage));

        Phase2Record rec;
        rec.pairId = pairId;
        rec.datasetId = QString::fromStdString(appController_.currentDatasetId());
        rec.processingMethod = modeStr;
        rec.processingVersion = (modeStr == "BYPASS") ? "BYPASS-v1.0" : "N5PostHalf-v1.0";
        rec.status = "ERROR";
        rec.elapsedMs = elapsedMs > 0 ? std::optional<qint64>(elapsedMs) : std::nullopt;
        rec.errorMessage = errorMessage;
        phase2Results_.push_back(rec);
    }
}

void MainWindow::onEnhanceAllClicked()
{
    try {
        if (displayedItems_.empty()) {
            QMessageBox::information(this, "Thông báo", "Chưa có danh sách ảnh nào để xử lý toàn bộ.");
            return;
        }

        // Nếu đang trong tiến trình xử lý hàng loạt -> Bấm lại để HỦY / DỪNG
        if (isBatchEnhancing_) {
            if (batchWorker_) {
                batchWorker_->requestCancel();
            }
            ui->lblN5Status->setText("Đang yêu cầu dừng xử lý...");
            appendLog("Batch processing cancelled by user.");
            return;
        }

        const auto mode = ui->radModeBypass->isChecked()
                              ? qart::application::ThermalProcessingMode::Bypass
                              : qart::application::ThermalProcessingMode::N5PostHalf;
        const QString modeStr = (mode == qart::application::ThermalProcessingMode::Bypass) ? "BYPASS" : "N5";

        std::vector<EnhanceJobRequest> requests;
        requests.reserve(displayedItems_.size());
        for (const auto &item : displayedItems_) {
            EnhanceJobRequest req;
            req.pairId = QString::fromStdString(item.pair.pairId());
            req.thermalFullPath = appController_.datasetRoot() / item.pair.thermalRelativePath();
            req.mode = mode;
            requests.push_back(req);
        }

        const int totalCount = static_cast<int>(requests.size());
        isBatchEnhancing_ = true;
        ui->btnEnhanceAll->setText(tr("Dừng tăng cường"));
        ui->btnEnhanceAll->setStyleSheet("background-color: #7B1113; color: #FFFFFF; font-weight: bold; border-radius: 4px; padding: 6px;");
        ui->btnApplyN5->setEnabled(false);
        ui->btnResetN5->setEnabled(false);

        ui->prgN5->setVisible(true);
        ui->prgN5->setRange(0, totalCount);
        ui->prgN5->setValue(0);

        setN5UiState(N5UiState::Processing);
        appendLog(QString("Started batch processing for %1 pairs (%2).").arg(totalCount).arg(modeStr));

        if (batchWorker_) {
            if (batchWorker_->isRunning()) {
                batchWorker_->requestCancel();
                batchWorker_->wait(500);
            }
            delete batchWorker_;
            batchWorker_ = nullptr;
        }

        batchWorker_ = new BatchEnhanceWorkerThread(std::move(requests), this);
        connect(batchWorker_, &BatchEnhanceWorkerThread::jobStepFinished, this, &MainWindow::onBatchStepFinished);
        connect(batchWorker_, &BatchEnhanceWorkerThread::batchFinished, this, &MainWindow::onBatchWorkerFinished);
        batchWorker_->start();
    } catch (const std::exception &ex) {
        isBatchEnhancing_ = false;
        ui->btnEnhanceAll->setText(tr("Tăng cường toàn bộ dataset"));
        ui->btnEnhanceAll->setStyleSheet("");
        setN5UiState(N5UiState::Error, 0, ex.what());
        QMessageBox::critical(this, "Lỗi xử lý hàng loạt", QString("Đã xảy ra lỗi:\n%1").arg(ex.what()));
    }
}

void MainWindow::onBatchStepFinished(int index,
                                     const QString &pairId,
                                     const QImage &resultImage,
                                     qint64 elapsedMs,
                                     bool success,
                                     const QString &errorMessage,
                                     int mode,
                                     int successCount,
                                     int errorCount,
                                     int cancelledCount)
{
    Q_UNUSED(cancelledCount);
    // 1. Chuyển chọn dòng và cuộn danh sách hiển thị
    if (index >= 0 && index < ui->lstImagePairs->count()) {
        ui->lstImagePairs->setCurrentRow(index);
        if (auto *item = ui->lstImagePairs->item(index)) {
            ui->lstImagePairs->scrollToItem(item);
        }
    }

    activeN5PairId_ = pairId;
    const QString modeStr = (mode == static_cast<int>(qart::application::ThermalProcessingMode::Bypass)) ? "BYPASS" : "N5";

    if (success && !resultImage.isNull()) {
        cachedN5EnhancedPixmap_ = QPixmap::fromImage(resultImage);
        ui->viewerN5Enhanced->setPixmap(cachedN5EnhancedPixmap_);
        ui->viewerN5Enhanced->fitToView();
        ui->lblN5EnhancedInfo->setText(QString("Cặp: %1 | %2 OK (%3 ms)").arg(pairId, modeStr).arg(elapsedMs));

        Phase2Record rec;
        rec.pairId = pairId;
        rec.datasetId = QString::fromStdString(appController_.currentDatasetId());
        rec.processingMethod = modeStr;
        rec.processingVersion = (modeStr == "BYPASS") ? "BYPASS-v1.0" : "N5PostHalf-v1.0";
        rec.status = "SUCCESS";
        rec.elapsedMs = elapsedMs;
        phase2Results_.push_back(rec);
    } else {
        cachedN5EnhancedPixmap_ = QPixmap();
        ui->viewerN5Enhanced->clearImage(QString("Lỗi: %1").arg(errorMessage));
        ui->lblN5EnhancedInfo->setText(QString("Cặp: %1 | Lỗi: %2").arg(pairId, errorMessage));
        appendLog(QString("Batch item error on pair %1: %2").arg(pairId, errorMessage));

        Phase2Record rec;
        rec.pairId = pairId;
        rec.datasetId = QString::fromStdString(appController_.currentDatasetId());
        rec.processingMethod = modeStr;
        rec.processingVersion = (modeStr == "BYPASS") ? "BYPASS-v1.0" : "N5PostHalf-v1.0";
        rec.status = "ERROR";
        rec.elapsedMs = elapsedMs > 0 ? std::optional<qint64>(elapsedMs) : std::nullopt;
        rec.errorMessage = errorMessage;
        phase2Results_.push_back(rec);
    }

    ui->prgN5->setValue(index + 1);
    ui->lblN5Status->setText(QString("Đang xử lý: %1/%2 (Thành công: %3 | Lỗi: %4)")
                                 .arg(index + 1)
                                 .arg(ui->prgN5->maximum())
                                 .arg(successCount)
                                 .arg(errorCount));
}

void MainWindow::onBatchWorkerFinished(int successCount, int errorCount, int cancelledCount)
{
    const int total = successCount + errorCount + cancelledCount;
    const bool stoppedEarly = (cancelledCount > 0);
    isBatchEnhancing_ = false;

    ui->btnEnhanceAll->setText(tr("Tăng cường toàn bộ dataset"));
    ui->btnEnhanceAll->setStyleSheet("");
    ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty());
    ui->btnResetN5->setEnabled(!cachedN5EnhancedPixmap_.isNull());
    ui->prgN5->setVisible(false);

    if (stoppedEarly) {
        setN5UiState(N5UiState::Cancelled);
        ui->lblN5Status->setText(QString("Đã hủy: %1 thành công, %2 lỗi, %3 đã hủy (Tổng số %4).")
                                     .arg(successCount).arg(errorCount).arg(cancelledCount).arg(total));
        appendLog(QString("Batch cancelled: %1 succeeded, %2 failed, %3 cancelled (Total %4).")
                      .arg(successCount).arg(errorCount).arg(cancelledCount).arg(total));
    } else {
        setN5UiState(N5UiState::Completed);
        ui->lblN5Status->setText(QString("Hoàn tất: %1 thành công, %2 lỗi (Tổng số %3).")
                                     .arg(successCount).arg(errorCount).arg(total));
        appendLog(QString("Batch completed: %1 succeeded, %2 failed (Total %3).")
                      .arg(successCount).arg(errorCount).arg(total));
    }
}

void MainWindow::onResetN5Clicked()
{
    try {
        if (isBatchEnhancing_) {
            if (batchWorker_) {
                batchWorker_->requestCancel();
            }
            isBatchEnhancing_ = false;
        }
        cachedN5EnhancedPixmap_ = QPixmap();
        ui->viewerN5Enhanced->clearImage("Đã khôi phục ảnh gốc. Bấm 'Áp dụng xử lý' để chạy.");
        ui->viewerN5Original->fitToView();
        ui->viewerN5Enhanced->fitToView();
        setN5UiState(N5UiState::NotRun);
        appendLog(QString("Cleared IR image cache for pair %1").arg(activeN5PairId_));
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi reset: %1").arg(ex.what()));
    }
}

void MainWindow::onExportPhase2Clicked()
{
    if (phase2Results_.empty()) {
        appendLog("Save blocked: no result available.");
        QMessageBox::warning(this, "Xuất kết quả Phase 2",
                             "Chưa có kết quả xử lý thực tế nào để xuất báo cáo.\n(Save blocked: no result available.)");
        return;
    }

    int successCount = 0;
    int errorCount = 0;
    for (const auto &r : phase2Results_) {
        if (r.status == "SUCCESS") successCount++;
        else errorCount++;
    }

    QMessageBox::information(this, "Phạm vi xuất kết quả Phase 2",
                             QString("Phạm vi dữ liệu xuất:\n- Tổng số bản ghi: %1\n- Thành công: %2\n- Lỗi: %3\n\nLưu ý: Schema báo cáo Phase 2 đang được trình anh hướng dẫn duyệt trước khi ghi file.")
                                 .arg(phase2Results_.size())
                                 .arg(successCount)
                                 .arg(errorCount));
}

void MainWindow::updateQualityN5Tab(const PairItemEntry &entry)
{
    try {
        const auto &pair = entry.pair;
        activeN5PairId_ = QString::fromStdString(pair.pairId());

        ui->lblN5PairInfo->setText(QString("Cặp ảnh: %1 (Stem: %2 | Partition: %3)")
                                       .arg(QString::fromStdString(pair.pairId()),
                                            QString::fromStdString(pair.sourceStem()),
                                            QString::fromStdString(pair.partitionId().value())));

        // KHOÁ CỨNG MODALITY: Luôn nạp ảnh nhiệt (Thermal / IR - A2). Tuyệt đối không nạp ảnh màu Visible.
        std::filesystem::path targetRelPath = pair.thermalRelativePath();
        std::filesystem::path fullPath = appController_.datasetRoot() / targetRelPath;

        if (QFile::exists(QString::fromStdString(fullPath.string()))) {
            cachedN5OriginalPixmap_ = QPixmap(QString::fromStdString(fullPath.string()));
            if (!cachedN5OriginalPixmap_.isNull()) {
                ui->lblN5OriginalInfo->setText(QString("File: %1\nKích thước: %2 x %3 px | Modality: Thermal (A2)")
                                                   .arg(QString::fromStdString(targetRelPath.string()))
                                                   .arg(cachedN5OriginalPixmap_.width())
                                                   .arg(cachedN5OriginalPixmap_.height()));
                ui->viewerN5Original->setPixmap(cachedN5OriginalPixmap_);
            } else {
                cachedN5OriginalPixmap_ = QPixmap();
                ui->lblN5OriginalInfo->setText("Không thể giải mã dữ liệu ảnh nhiệt qua QPixmap.");
                ui->viewerN5Original->clearImage("Lỗi giải mã ảnh nhiệt");
                appendLog(QString("Rejected IR file: %1").arg(QString::fromStdString(targetRelPath.filename().string())));
            }
        } else {
            cachedN5OriginalPixmap_ = QPixmap();
            ui->lblN5OriginalInfo->setText("Không tìm thấy file ảnh nhiệt trên đĩa.");
            ui->viewerN5Original->clearImage("Không tìm thấy file ảnh nhiệt");
            appendLog(QString("Failed to load IR image: %1").arg(QString::fromStdString(targetRelPath.filename().string())));
        }

        // STALE INVALIDATION: Xóa ngay trạng thái và kết quả cũ trên khung xem khi đổi cặp ảnh!
        cachedN5EnhancedPixmap_ = QPixmap();
        ui->viewerN5Enhanced->clearImage(tr("Bấm 'Áp dụng xử lý' để chạy N5 hoặc BYPASS"));
        setN5UiState(N5UiState::NotRun);

        ui->viewerN5Original->fitToView();
        ui->viewerN5Enhanced->fitToView();
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi cập nhật Tab N5: %1").arg(ex.what()));
    }
}

void MainWindow::setN5UiState(N5UiState state, qint64 elapsedMs, const QString &detailMsg)
{
    n5State_ = state;
    const bool isBypass = (ui->radModeBypass && ui->radModeBypass->isChecked());
    const QString methodStr = isBypass ? "BYPASS" : "N5";

    switch (state) {
    case N5UiState::NotRun:
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty() && !isBatchEnhancing_);
        ui->btnEnhanceAll->setEnabled(!displayedItems_.empty() && !isBatchEnhancing_);
        ui->btnResetN5->setEnabled(!cachedN5EnhancedPixmap_.isNull() && !isBatchEnhancing_);
        ui->prgN5->setVisible(false);
        ui->lblN5Status->setText("Trạng thái: Chưa chạy");
        ui->lblN5Status->setStyleSheet("background-color: #1a2a3a; color: #4B8DFF; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        ui->lblN5EnhancedInfo->setText("Thời gian: — | Trạng thái: Chưa chạy");
        break;

    case N5UiState::Processing:
        ui->btnApplyN5->setEnabled(false); // Khóa nút Apply chống spam click!
        ui->btnEnhanceAll->setEnabled(false);
        ui->btnResetN5->setEnabled(false);
        ui->prgN5->setVisible(true);
        ui->prgN5->setRange(0, 0); // Marquee animation
        ui->lblN5Status->setText(QString("Trạng thái: Đang xử lý (%1)...").arg(methodStr));
        ui->lblN5Status->setStyleSheet("background-color: #3b2d18; color: #E5A93C; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        ui->lblN5EnhancedInfo->setText(QString("Thời gian: — | Trạng thái: Đang xử lý (%1)...").arg(methodStr));
        break;

    case N5UiState::Completed:
        ui->btnApplyN5->setEnabled(!isBatchEnhancing_);
        ui->btnEnhanceAll->setEnabled(!displayedItems_.empty() && !isBatchEnhancing_);
        ui->btnResetN5->setEnabled(true);
        ui->prgN5->setVisible(false);
        if (elapsedMs > 0) {
            ui->lblN5Status->setText(QString("Trạng thái: Hoàn tất (%1 ms | %2)").arg(elapsedMs).arg(methodStr));
            ui->lblN5EnhancedInfo->setText(QString("Thời gian: %1 ms | Trạng thái: Hoàn tất | %2").arg(elapsedMs).arg(methodStr));
        } else {
            ui->lblN5Status->setText(QString("Trạng thái: Hoàn tất | %1").arg(methodStr));
            ui->lblN5EnhancedInfo->setText(QString("Thời gian: — | Trạng thái: Hoàn tất | %1").arg(methodStr));
        }
        ui->lblN5Status->setStyleSheet("background-color: #183329; color: #39BDA7; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        break;

    case N5UiState::Error:
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty() && !isBatchEnhancing_);
        ui->btnEnhanceAll->setEnabled(!displayedItems_.empty() && !isBatchEnhancing_);
        ui->btnResetN5->setEnabled(false);
        ui->prgN5->setVisible(false);
        ui->lblN5Status->setText("Trạng thái: Lỗi");
        ui->lblN5Status->setStyleSheet("background-color: #3d1a1a; color: #FF6B6B; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        ui->lblN5EnhancedInfo->setText(QString("Thời gian: — | Trạng thái: Lỗi | %1").arg(detailMsg));
        break;

    case N5UiState::Cancelled:
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty());
        ui->btnEnhanceAll->setEnabled(!displayedItems_.empty());
        ui->btnResetN5->setEnabled(!cachedN5EnhancedPixmap_.isNull());
        ui->prgN5->setVisible(false);
        ui->lblN5Status->setText("Trạng thái: Đã hủy");
        ui->lblN5Status->setStyleSheet("background-color: #2b2b2b; color: #E0E0E0; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        ui->lblN5EnhancedInfo->setText("Thời gian: — | Trạng thái: Đã hủy");
        break;
    }
}

void MainWindow::renderCachedN5Images()
{
    if (ui->viewerN5Original) {
        if (!cachedN5OriginalPixmap_.isNull()) {
            ui->viewerN5Original->setPixmap(cachedN5OriginalPixmap_);
        } else {
            ui->viewerN5Original->clearImage("Chưa có ảnh nhiệt");
        }
    }
    if (ui->viewerN5Enhanced) {
        if (!cachedN5EnhancedPixmap_.isNull()) {
            ui->viewerN5Enhanced->setPixmap(cachedN5EnhancedPixmap_);
        } else {
            ui->viewerN5Enhanced->clearImage("Bấm 'Áp dụng xử lý' để chạy N5 hoặc BYPASS");
        }
    }
}
