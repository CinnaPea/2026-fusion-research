#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "core/domain/datasets/dataset_pair.h"
#include "core/domain/validation/pair_validation_result.h"
#include "core/domain/validation/pair_validation_status.h"

#include <QDateTime>
#include <QSaveFile>
#include <QStringList>
#include <QCloseEvent>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTime>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
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
    if (ui->txtLogConsole) {
        QFont logFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        logFont.setPointSize(11);
        ui->txtLogConsole->setFont(logFont);
    }

    setupUiControls();

    // Enforce balanced 50/50 side-by-side image columns on Tab 1 & Tab 2
    ui->dualImageDisplayLayout->setStretch(0, 1);
    ui->dualImageDisplayLayout->setStretch(1, 1);
    ui->n5BodyLayout->setStretch(0, 1);
    ui->n5BodyLayout->setStretch(1, 0);
    ui->n5ImagesLayout->setStretch(0, 1);
    ui->n5ImagesLayout->setStretch(1, 1);

    // Splitter layout settings: give maximum space to image viewer
    ui->splitterMain->setStretchFactor(0, 0);
    ui->splitterMain->setStretchFactor(1, 1);
    ui->splitterMain->setSizes({255, 1005});

    // Vertical splitter for Tab 2 right panel (Controls on top, Log console below)
    ui->splitterN5Right->setCollapsible(0, false);
    ui->splitterN5Right->setCollapsible(1, false);
    ui->splitterN5Right->setStretchFactor(0, 1);
    ui->splitterN5Right->setStretchFactor(1, 0);
    ui->splitterN5Right->setSizes({440, 170});

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

    connect(ui->radModeN5, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && !isBatchEnhancing_ && n5State_ == N5UiState::NotRun) {
            setN5UiState(N5UiState::NotRun);
        }
    });
    connect(ui->radModeBypass, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && !isBatchEnhancing_ && n5State_ == N5UiState::NotRun) {
            setN5UiState(N5UiState::NotRun);
        }
    });
    connect(ui->chkShowDiffMap, &QCheckBox::toggled, this, [this](bool) {
        renderCachedN5Images();
        if (!cachedN5EnhancedPixmap_.isNull() && !lastMethodStr_.isEmpty()) {
            const bool showingDiff = ui->chkShowDiffMap->isChecked() && !cachedN5DiffPixmap_.isNull();
            ui->lblN5EnhancedInfo->setText(formatEnhancementMetricsSummary(
                lastMethodStr_,
                lastElapsedMs_,
                lastAppliedParams_,
                lastThermalMetrics_,
                showingDiff));
        }
    });
    connect(ui->chkSyncView, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && !cachedN5OriginalPixmap_.isNull() && !cachedN5EnhancedPixmap_.isNull()) {
            ui->viewerN5Enhanced->setSyncViewport(
                ui->viewerN5Original->zoomFactor(),
                ui->viewerN5Original->panOffset());
        }
    });

    applyThemeAwareStyles();

    // Algorithm parameters two-way slider/spinbox synchronization
    connect(ui->sldParamRadius, &QSlider::valueChanged, this, [this](int val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->spnParamRadius->setValue(val);
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });
    connect(ui->spnParamRadius, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->sldParamRadius->setValue(val);
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });

    connect(ui->sldParamEps, &QSlider::valueChanged, this, [this](int val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->spnParamEps->setValue(val / 1000.0);
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });
    connect(ui->spnParamEps, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->sldParamEps->setValue(qRound(val * 1000.0));
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });

    connect(ui->sldParamH, &QSlider::valueChanged, this, [this](int val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->spnParamH->setValue(val / 10.0);
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });
    connect(ui->spnParamH, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->sldParamH->setValue(qRound(val * 10.0));
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });

    connect(ui->sldParamGamma, &QSlider::valueChanged, this, [this](int val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->spnParamGamma->setValue(val / 100.0);
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });
    connect(ui->spnParamGamma, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->sldParamGamma->setValue(qRound(val * 100.0));
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });

    connect(ui->sldParamAlpha, &QSlider::valueChanged, this, [this](int val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->spnParamAlpha->setValue(val / 100.0);
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });
    connect(ui->spnParamAlpha, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (isSyncingParams_) return;
        isSyncingParams_ = true;
        ui->sldParamAlpha->setValue(qRound(val * 100.0));
        isSyncingParams_ = false;
        onAlgorithmParamChanged();
    });

    connect(ui->cmbParamMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        onAlgorithmParamChanged();
    });
    connect(ui->cmbParamDetailNorm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        onAlgorithmParamChanged();
    });
    connect(ui->chkParamAdaptiveNlm, &QCheckBox::toggled, this, [this](bool) {
        onAlgorithmParamChanged();
    });

    connect(ui->btnParamDefaults, &QPushButton::clicked, this, &MainWindow::onResetAlgorithmParamsClicked);

    updateBatchButtonText();
    setN5UiState(N5UiState::NotRun);
    appendLog("Hệ thống khởi động thành công. Sẵn sàng.");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if ((batchWorker_ && batchWorker_->isRunning())
        || (singleWorker_ && singleWorker_->isRunning())
        || (previewWorker_ && previewWorker_->isRunning())) {
        appendLog("Window closing: waiting for active worker threads to finish safely...");
    }
    currentSessionId_++;
    currentRequestId_++;
    currentBatchId_++;
    currentPreviewRequestId_++;
    isBatchEnhancing_ = false;
    isBatchCancelling_ = false;
    stopAllWorkersAndWait();
    event->accept();
}

MainWindow::~MainWindow()
{
    currentSessionId_++;
    currentRequestId_++;
    currentBatchId_++;
    currentPreviewRequestId_++;
    stopAllWorkersAndWait();
    delete ui;
}

void MainWindow::pruneFinishedRetiredWorkers()
{
    pendingRetiredWorkers_.erase(
        std::remove_if(
            pendingRetiredWorkers_.begin(),
            pendingRetiredWorkers_.end(),
            [](const QPointer<QThread> &worker) {
                return worker.isNull();
            }),
        pendingRetiredWorkers_.end());
}

void MainWindow::retireSingleWorker()
{
    pruneFinishedRetiredWorkers();
    if (!singleWorker_) {
        singleWorker_.clear();
        return;
    }

    disconnect(singleWorker_.data(), nullptr, this, nullptr);
    singleWorker_->requestInterruption();
    if (singleWorker_->isRunning()) {
        pendingRetiredWorkers_.emplace_back(singleWorker_.data());
    } else {
        singleWorker_->deleteLater();
    }
    singleWorker_.clear();
}

void MainWindow::retireBatchWorker()
{
    pruneFinishedRetiredWorkers();
    if (!batchWorker_) {
        batchWorker_.clear();
        return;
    }

    disconnect(batchWorker_.data(), nullptr, this, nullptr);
    batchWorker_->requestCancel();
    batchWorker_->requestInterruption();
    if (batchWorker_->isRunning()) {
        pendingRetiredWorkers_.emplace_back(batchWorker_.data());
    } else {
        batchWorker_->deleteLater();
    }
    batchWorker_.clear();
}

void MainWindow::retirePreviewWorker()
{
    pruneFinishedRetiredWorkers();
    if (!previewWorker_) {
        previewWorker_.clear();
        return;
    }

    disconnect(previewWorker_.data(), nullptr, this, nullptr);
    previewWorker_->requestInterruption();
    if (previewWorker_->isRunning()) {
        pendingRetiredWorkers_.emplace_back(previewWorker_.data());
    } else {
        previewWorker_->deleteLater();
    }
    previewWorker_.clear();
}

void MainWindow::stopAllWorkersAndWait()
{
    if (batchWorker_) {
        disconnect(batchWorker_.data(), nullptr, this, nullptr);
        batchWorker_->requestCancel();
        batchWorker_->requestInterruption();
    }
    if (singleWorker_) {
        disconnect(singleWorker_.data(), nullptr, this, nullptr);
        singleWorker_->requestInterruption();
    }
    if (previewWorker_) {
        disconnect(previewWorker_.data(), nullptr, this, nullptr);
        previewWorker_->requestInterruption();
    }
    for (const auto &retired : pendingRetiredWorkers_) {
        if (retired) {
            if (auto *batch = qobject_cast<BatchEnhanceWorkerThread *>(retired.data())) {
                batch->requestCancel();
            }
            retired->requestInterruption();
        }
    }

    // Never destroy a running QThread after a timeout: wait until execution finishes.
    if (batchWorker_) {
        if (batchWorker_->isRunning()) {
            batchWorker_->wait();
        }
        delete batchWorker_.data();
        batchWorker_.clear();
    }
    if (singleWorker_) {
        if (singleWorker_->isRunning()) {
            singleWorker_->wait();
        }
        delete singleWorker_.data();
        singleWorker_.clear();
    }
    if (previewWorker_) {
        if (previewWorker_->isRunning()) {
            previewWorker_->wait();
        }
        delete previewWorker_.data();
        previewWorker_.clear();
    }
    for (auto &retired : pendingRetiredWorkers_) {
        if (retired) {
            if (retired->isRunning()) {
                retired->wait();
            }
            delete retired.data();
            retired.clear();
        }
    }
    pendingRetiredWorkers_.clear();
}

void MainWindow::invalidateActiveSessionAndWorkers(bool clearOriginalViewer)
{
    currentSessionId_++;
    currentRequestId_++;
    currentBatchId_++;
    currentPreviewRequestId_++;

    retireBatchWorker();
    retireSingleWorker();
    retirePreviewWorker();

    isBatchEnhancing_ = false;
    isBatchCancelling_ = false;
    batchTotalCount_ = 0;

    activePreviewIndex_ = -1;
    cachedPreviewIndex_ = -1;
    cachedVisibleImage_ = QImage();
    cachedThermalImage_ = QImage();

    if (clearOriginalViewer) {
        clearTab1PreviewUi();
        cachedN5OriginalPixmap_ = QPixmap();
        activeN5OriginalBaseInfo_.clear();
        activeN5OriginalStats_ = ThermalPlaneStats{};
        activeN5PairId_.clear();
        activeThermalValid_ = false;
        if (ui->viewerN5Original) {
            ui->viewerN5Original->clearImage("Chưa có ảnh nhiệt");
        }
        if (ui->lblN5PairInfo) {
            ui->lblN5PairInfo->setText("Cặp ảnh: Chưa chọn");
        }
        if (ui->lblN5OriginalInfo) {
            ui->lblN5OriginalInfo->setText("Chưa nạp ảnh nhiệt");
        }
    }

    clearN5EnhancedCache();
    if (ui->viewerN5Enhanced) {
        ui->viewerN5Enhanced->clearImage(tr("Bấm 'Áp dụng xử lý' để chạy N5 hoặc BYPASS"));
    }

    ui->prgN5->setVisible(false);
    updateBatchButtonText();
    setN5UiState(N5UiState::NotRun);
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

    ui->grpAlgorithmParams->setEnabled(false);
    ui->grpAlgorithmParams->setToolTip("Thử nghiệm — chưa khả dụng trong bản hiện tại");

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

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event && (event->type() == QEvent::PaletteChange
                  || event->type() == QEvent::ApplicationPaletteChange
                  || event->type() == QEvent::ThemeChange)) {
        applyThemeAwareStyles();
    }
}

void MainWindow::applyThemeAwareStyles()
{
    if (!ui || !ui->grpAlgorithmParams) {
        return;
    }

    const bool isDark = palette().color(QPalette::Window).lightness() < 128;

    ui->scrollAreaN5Controls->setStyleSheet("QScrollArea#scrollAreaN5Controls { background: transparent; border: none; }");
    if (ui->scrollAreaN5Controls->viewport()) {
        ui->scrollAreaN5Controls->viewport()->setAutoFillBackground(false);
    }
    ui->scrollContentN5Controls->setStyleSheet("QWidget#scrollContentN5Controls { background: transparent; }");

    const char *activeTextColor = isDark ? "#e5e7eb" : "#1f2937";
    const char *metaTextColor = isDark ? "#cbd5e1" : "#374151";
    const QString activeCtrlStyle = QString("color: %1;").arg(activeTextColor);
    ui->lblN5ModeLabel->setStyleSheet(activeCtrlStyle);
    ui->radModeN5->setStyleSheet(activeCtrlStyle);
    ui->radModeBypass->setStyleSheet(activeCtrlStyle);
    ui->chkSyncView->setStyleSheet(activeCtrlStyle);
    if (ui->chkShowDiffMap) {
        ui->chkShowDiffMap->setStyleSheet(activeCtrlStyle);
    }

    const QString infoLabelStyle = QString("font-size: 11px; color: %1;").arg(metaTextColor);
    ui->lblVisibleInfo->setStyleSheet(infoLabelStyle);
    ui->lblThermalInfo->setStyleSheet(infoLabelStyle);
    ui->lblN5OriginalInfo->setStyleSheet(infoLabelStyle);
    ui->lblN5EnhancedInfo->setStyleSheet(infoLabelStyle);

    const char *splitterHandleBg = isDark ? "#2b2e38" : "#d0d5dd";
    const char *splitterHandleHover = isDark ? "#528bff" : "#2563eb";
    ui->splitterN5Right->setStyleSheet(
        QString("QSplitter::handle:vertical {"
                "    background-color: %1;"
                "    height: 6px;"
                "    margin: 2px 0px;"
                "    border-radius: 2px;"
                "}"
                "QSplitter::handle:vertical:hover {"
                "    background-color: %2;"
                "}")
            .arg(splitterHandleBg, splitterHandleHover));

    const char *cardBg = isDark ? "rgba(255, 255, 255, 0.03)" : "#f6f7f9";
    const char *cardBorder = isDark ? "#3e4451" : "#d0d5dd";
    const char *titleColor = isDark ? "#abb2bf" : "#667085";
    const char *labelColor = isDark ? "#b8bec8" : "#475467";
    const char *noticeColor = isDark ? "#fbbf24" : "#b45309";
    const char *fieldBg = isDark ? "#1c2026" : "#eaecf0";
    const char *fieldColor = isDark ? "#d1d5db" : "#475467";
    const char *grooveBg = isDark ? "#3a3f4b" : "#d0d5dd";
    const char *subPageBg = isDark ? "#528bff" : "#98a2b3";
    const char *handleBg = isDark ? "#9ca3af" : "#98a2b3";
    const char *handleBorder = isDark ? "#282c34" : "#667085";

    ui->grpAlgorithmParams->setStyleSheet(
        QString("QGroupBox#grpAlgorithmParams {"
                "    background-color: %1;"
                "    font-weight: bold;"
                "    border: 1px solid %2;"
                "    border-radius: 6px;"
                "    margin-top: 8px;"
                "    padding-top: 12px;"
                "}"
                "QGroupBox#grpAlgorithmParams::title {"
                "    subcontrol-origin: margin;"
                "    subcontrol-position: top left;"
                "    padding: 0 4px;"
                "    color: %3;"
                "}"
                "QGroupBox#grpAlgorithmParams QLabel {"
                "    font-size: 11px;"
                "    color: %4;"
                "}"
                "QGroupBox#grpAlgorithmParams QLabel#lblParamNotice {"
                "    font-size: 11px;"
                "    font-style: italic;"
                "    color: %5;"
                "    padding: 2px;"
                "}"
                "QGroupBox#grpAlgorithmParams QSpinBox, "
                "QGroupBox#grpAlgorithmParams QDoubleSpinBox, "
                "QGroupBox#grpAlgorithmParams QComboBox {"
                "    background-color: %6;"
                "    color: %7;"
                "    border: 1px solid %2;"
                "    border-radius: 3px;"
                "    padding: 1px 4px;"
                "    font-size: 11px;"
                "}"
                "QGroupBox#grpAlgorithmParams QSlider::groove:horizontal {"
                "    height: 4px;"
                "    background: %8;"
                "    border-radius: 2px;"
                "}"
                "QGroupBox#grpAlgorithmParams QSlider::sub-page:horizontal {"
                "    background: %9;"
                "    border-radius: 2px;"
                "}"
                "QGroupBox#grpAlgorithmParams QSlider::handle:horizontal {"
                "    background: %10;"
                "    border: 1px solid %11;"
                "    width: 12px;"
                "    margin: -4px 0px;"
                "    border-radius: 6px;"
                "}"
                "QGroupBox#grpAlgorithmParams QCheckBox {"
                "    font-size: 11px;"
                "    color: %4;"
                "    spacing: 5px;"
                "}")
            .arg(cardBg, cardBorder, titleColor, labelColor, noticeColor,
                 fieldBg, fieldColor, grooveBg, subPageBg, handleBg, handleBorder));

    const char *defBg = isDark ? "#333842" : "#eaecf0";
    const char *defColor = isDark ? "#9ca3af" : "#667085";
    const char *defBorder = isDark ? "#4b5263" : "#d0d5dd";
    ui->btnParamDefaults->setStyleSheet(
        QString("QPushButton#btnParamDefaults {"
                "    background-color: %1;"
                "    color: %2;"
                "    border: 1px solid %3;"
                "    border-radius: 4px;"
                "    padding: 5px;"
                "    font-weight: bold;"
                "}")
            .arg(defBg, defColor, defBorder));

    const char *applyBg = isDark ? "#3b597c" : "#2563eb";
    const char *applyHover = isDark ? "#4a6f9c" : "#1d4ed8";
    const char *applyBorder = isDark ? "#528bff" : "#1d4ed8";
    const char *applyDisBg = isDark ? "#2b2e38" : "#e5e7eb";
    const char *applyDisColor = isDark ? "#6b7280" : "#6b7280";
    const char *applyDisBorder = isDark ? "#3a3e4b" : "#d1d5db";
    ui->btnApplyN5->setStyleSheet(
        QString("QPushButton#btnApplyN5 {"
                "    background-color: %1;"
                "    color: #ffffff;"
                "    border: 1px solid %3;"
                "    border-radius: 4px;"
                "    padding: 6px 10px;"
                "    font-weight: bold;"
                "}"
                "QPushButton#btnApplyN5:hover {"
                "    background-color: %2;"
                "}"
                "QPushButton#btnApplyN5:disabled {"
                "    background-color: %4;"
                "    color: %5;"
                "    border: 1px solid %6;"
                "}")
            .arg(applyBg, applyHover, applyBorder,
                 applyDisBg, applyDisColor, applyDisBorder));
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
        clearAllPairEnhancementCache();
        invalidateActiveSessionAndWorkers(true);

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

void MainWindow::clearTab1PreviewUi()
{
    ui->lblCurrentPairId->setText("Cặp ảnh: Chưa chọn");
    ui->lblPairStatus->setText("Trạng thái: --");
    ui->lblPairStatus->setStyleSheet("");
    ui->lblPairStatusMessage->setText("");
    ui->lblVisibleImage->setPixmap(QPixmap());
    ui->lblVisibleImage->setText("Chưa nạp ảnh Visible");
    ui->lblVisibleImage->setStyleSheet("background-color: #1e1e1e; color: #888888; border: 1px solid #333333; border-radius: 6px;");
    ui->lblVisibleInfo->setText("");
    ui->lblThermalImage->setPixmap(QPixmap());
    ui->lblThermalImage->setText("Chưa nạp ảnh Thermal");
    ui->lblThermalImage->setStyleSheet("background-color: #1e1e1e; color: #888888; border: 1px solid #333333; border-radius: 6px;");
    ui->lblThermalInfo->setText("");
}

void MainWindow::updateListItemDisplay(QListWidgetItem *listItem, const PairItemEntry &entry) const
{
    if (!listItem) {
        return;
    }

    const QString pairIdQStr = QString::fromStdString(entry.pair.pairId());
    const QString sourcePrefix = entry.sourceTag.isEmpty() ? "" : (entry.sourceTag + " ");
    const QString inQueueSuffix = appController_.isManuallyQueued(entry.pair.pairId()) ? " [IN QUEUE]" : "";
    const auto *cachedEnh = findPairEnhancement(pairIdQStr);

    if (entry.validationResult.has_value()) {
        const PairValidationStatus status = entry.validationResult->status();
        const QString baseTag = getStatusTagText(status);
        const QString tagText = cachedEnh ? QString("%1 · %2").arg(baseTag, cachedEnh->methodStr) : baseTag;
        const QString colorHex = getStatusColorHex(status);
        listItem->setText(QString("[%1] %2%3 (%4)%5")
                              .arg(tagText,
                                   sourcePrefix,
                                   QString::fromStdString(entry.pair.sourceStem()),
                                   pairIdQStr,
                                   inQueueSuffix));
        listItem->setForeground(QBrush(QColor(colorHex)));
    } else {
        const QString enhPrefix = cachedEnh ? QString("[%1] ").arg(cachedEnh->methodStr) : QString();
        listItem->setText(QString("%1%2%3 (%4)%5")
                              .arg(enhPrefix,
                                   sourcePrefix,
                                   QString::fromStdString(entry.pair.sourceStem()),
                                   pairIdQStr,
                                   inQueueSuffix));
        listItem->setForeground(QBrush(QColor(0x1565C0))); // Soft blue default
    }
}

void MainWindow::applyFiltersAndModes()
{
    try {
        retirePreviewWorker();
        currentPreviewRequestId_++;
        activePreviewIndex_ = -1;
        cachedPreviewIndex_ = -1;
        cachedVisibleImage_ = QImage();
        cachedThermalImage_ = QImage();

        ui->lstImagePairs->clear();
        displayedItems_.clear();

        if (appController_.discoveredPairs().empty()) {
            clearTab1PreviewUi();
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
            isUpdatingControls_ = true;
            ui->cbViewMode->setCurrentIndex(0); // Chế độ A: Duyệt toàn bộ
            isUpdatingControls_ = false;
            viewMode = "full";
            appendLog("DVTOD: Mode B/C không khả dụng trước căn chỉnh Phase 3 vì các cặp ảnh chưa đồng kích thước.");
            QMessageBox::information(
                this,
                "Chế độ xem không khả dụng",
                "Các cặp ảnh DVTOD là nguồn hợp lệ nhưng chưa đồng kích thước pixel (1920x1080 vs 640x512).\n"
                "Chế độ lấy mẫu tự động (Mode B/C) yêu cầu các ảnh cùng hình học pixel.\n"
                "Hệ thống tự động chuyển về Chế độ A (Duyệt toàn bộ) để nghiên cứu căn chỉnh."
            );
        }

        if (viewMode == "full") {
            // Mode A: Full Dataset View
            displayedItems_.reserve(filteredPairs.size());
            for (const auto &pair : filteredPairs) {
                displayedItems_.push_back(PairItemEntry{
                    pair,
                    appController_.findCachedValidationResult(pair.pairId()),
                    ""
                });
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
                    displayedItems_.push_back(PairItemEntry{
                        shuffled[i],
                        appController_.findCachedValidationResult(shuffled[i].pairId()),
                        "[EXPLORE]"
                    });
                }
            }
        }

        // 3. Populate List Widget
        for (std::size_t i = 0; i < displayedItems_.size(); ++i) {
            auto *listItem = new QListWidgetItem(ui->lstImagePairs);
            listItem->setData(Qt::UserRole, static_cast<int>(i));
            updateListItemDisplay(listItem, displayedItems_[i]);
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
        } else {
            clearTab1PreviewUi();
        }

        updateBatchButtonText();
    } catch (const std::exception &ex) {
        const QString failedMode = ui->cbViewMode->currentData().toString();
        if (failedMode == "sample" || failedMode == "queue") {
            ui->lblPairCount->setText("Mode B/C không khả dụng");
        }
        ui->lblSystemStatus->setText(QString("Lỗi áp dụng bộ lọc: %1").arg(ex.what()));
        updateBatchButtonText();
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

        PairItemEntry &entry = displayedItems_[index];
        const QString pairIdQStr = QString::fromStdString(entry.pair.pairId());
        const CachedPairEnhancement *cachedEnh =
            !isBatchEnhancing_ ? findPairEnhancement(pairIdQStr) : nullptr;

        // Avoid redundant cancellation/reloads when itemClicked fires right after currentItemChanged
        const bool matchesTab1 =
            (cachedPreviewIndex_ == index)
            && (!cachedVisibleImage_.isNull() || !cachedThermalImage_.isNull() || entry.validationResult.has_value());
        const bool tab2MatchesCache =
            (cachedEnh == nullptr && cachedN5EnhancedPixmap_.isNull())
            || (cachedEnh != nullptr && !cachedN5EnhancedPixmap_.isNull() && lastMethodStr_ == cachedEnh->methodStr);
        const bool matchesTab2 =
            isBatchEnhancing_
            || (activeN5PairId_ == pairIdQStr
                && (!cachedN5OriginalPixmap_.isNull() || !activeThermalValid_)
                && tab2MatchesCache);
        const bool currentlyLoadingSame =
            (activePreviewIndex_ == index && previewWorker_ && previewWorker_->isRunning());
        if ((matchesTab1 && matchesTab2) || currentlyLoadingSame) {
            return;
        }

        updateListItemDisplay(item, entry);

        // Immediate lightweight UI metadata update
        ui->lblCurrentPairId->setText(QString("Cặp ảnh: %1 (Stem: %2 | Partition: %3)")
                                          .arg(pairIdQStr,
                                               QString::fromStdString(entry.pair.sourceStem()),
                                               QString::fromStdString(entry.pair.partitionId().value())));

        // Clear in-memory decoded image cache for previous pair
        activePreviewIndex_ = index;
        cachedPreviewIndex_ = -1;
        cachedVisibleImage_ = QImage();
        cachedThermalImage_ = QImage();

        ui->lblPairStatus->setText("Trạng thái: Đang nạp ảnh...");
        ui->lblPairStatus->setStyleSheet("background-color: #3b2d18; color: #E5A93C; padding: 4px 10px; border-radius: 4px; font-weight: bold;");
        ui->lblPairStatusMessage->setText("Đang đọc và kiểm định ảnh trong luồng nền...");

        ui->lblVisibleImage->setText("Đang nạp ảnh Visible...");
        ui->lblVisibleImage->setStyleSheet("background-color: #1e1e1e; color: #888888; border: 1px solid #333333; border-radius: 6px;");
        ui->lblVisibleImage->setPixmap(QPixmap());
        ui->lblThermalImage->setText("Đang nạp ảnh Thermal...");
        ui->lblThermalImage->setStyleSheet("background-color: #1e1e1e; color: #888888; border: 1px solid #333333; border-radius: 6px;");
        ui->lblThermalImage->setPixmap(QPixmap());

        std::optional<qart::application::ThermalProcessingMode> rehydrateMode;

        // Update Tab 2 outside active batch runs
        if (!isBatchEnhancing_) {
            const bool samePairAlreadyRendered =
                (activeN5PairId_ == pairIdQStr) && !cachedN5EnhancedPixmap_.isNull();
            const bool hasCachedPixels =
                (cachedEnh != nullptr && !cachedEnh->resultImage.isNull());

            activeN5PairId_ = pairIdQStr;
            cachedN5OriginalPixmap_ = QPixmap();
            activeN5OriginalBaseInfo_.clear();
            activeN5OriginalStats_ = ThermalPlaneStats{};
            ui->lblN5PairInfo->setText(QString("Cặp ảnh: %1 (Stem: %2 | Partition: %3)")
                                           .arg(pairIdQStr,
                                                QString::fromStdString(entry.pair.sourceStem()),
                                                QString::fromStdString(entry.pair.partitionId().value())));
            if (ui->viewerN5Original) {
                ui->viewerN5Original->clearImage("Đang nạp ảnh nhiệt...");
            }
            ui->lblN5OriginalInfo->setText("Đang nạp ảnh nhiệt...");

            if (hasCachedPixels) {
                displayCachedPairEnhancement(*cachedEnh);
                setN5UiState(N5UiState::Completed, cachedEnh->elapsedMs, QString(), cachedEnh->methodStr);
                ui->btnApplyN5->setEnabled(entry.validationResult.has_value() && entry.validationResult->thermalDecoded());
            } else if (!samePairAlreadyRendered) {
                clearN5EnhancedCache();
                if (cachedEnh != nullptr) {
                    rehydrateMode = cachedEnh->mode;
                    if (ui->viewerN5Enhanced) {
                        ui->viewerN5Enhanced->clearImage(QString("Đang nạp kết quả (%1)...").arg(cachedEnh->methodStr));
                    }
                    ui->lblN5Status->setText(QString("Trạng thái: Đang nạp kết quả (%1)...").arg(cachedEnh->methodStr));
                    ui->lblN5Status->setStyleSheet("background-color: #3b2d18; color: #E5A93C; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
                    ui->lblN5EnhancedInfo->setText(QString("Thời gian: — | Đang nạp kết quả %1...").arg(cachedEnh->methodStr));
                } else {
                    if (ui->viewerN5Enhanced) {
                        ui->viewerN5Enhanced->clearImage(tr("Bấm 'Áp dụng xử lý' để chạy N5 hoặc BYPASS"));
                    }
                    ui->lblN5Status->setText("Trạng thái: Đang nạp...");
                    ui->lblN5Status->setStyleSheet("background-color: #3b2d18; color: #E5A93C; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
                    ui->lblN5EnhancedInfo->setText("Thời gian: — | Trạng thái: Đang nạp ảnh...");
                }
                ui->btnApplyN5->setEnabled(false);
            }
        }

        // Spawn async PreviewWorkerThread off GUI thread
        retirePreviewWorker();
        currentPreviewRequestId_++;

        PreviewJobRequest req;
        req.sessionId = currentSessionId_;
        req.requestId = currentPreviewRequestId_;
        req.enhancementRequestId = currentRequestId_;
        req.itemIndex = index;
        req.pair = entry.pair;
        req.validationResult = entry.validationResult;
        req.datasetRoot = appController_.datasetRoot();
        req.rehydrateMode = rehydrateMode;

        previewWorker_ = new PreviewWorkerThread(std::move(req), this);
        connect(previewWorker_.data(), &PreviewWorkerThread::previewFinished,
                this, &MainWindow::onPreviewFinished);
        connect(previewWorker_.data(), &QThread::finished,
                previewWorker_.data(), &QObject::deleteLater);
        previewWorker_->start();

    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi chọn cặp ảnh: %1").arg(ex.what()));
    }
}

void MainWindow::onPreviewFinished(const PreviewJobResult &result)
{
    if (result.sessionId != currentSessionId_ || result.requestId != currentPreviewRequestId_) {
        // Drop stale preview results (e.g. user scrolled past quickly)
        return;
    }

    if (result.itemIndex < 0 || result.itemIndex >= static_cast<int>(displayedItems_.size())) {
        return;
    }

    auto &entry = displayedItems_[result.itemIndex];

    // Reject results that no longer match the item at this index.
    if (QString::fromStdString(entry.pair.pairId()) != result.pairId) {
        return;
    }

    // A worker failure is not evidence that a source file is missing.
    if (!result.errorMessage.isEmpty()
        || !result.validationResult.has_value()) {
        cachedPreviewIndex_ = -1;
        const QString error =
            !result.errorMessage.isEmpty()
                ? result.errorMessage
                : tr("Worker không trả về kết quả kiểm định.");

        appendLog(
            QString("Preview error [%1]: %2")
                .arg(result.pairId, error)
        );

        ui->lblPairStatus->setText(tr("Trạng thái: Lỗi nạp/kiểm định"));
        ui->lblPairStatus->setStyleSheet(
            "background-color: #3d1a1a; color: #FF6B6B;"
            "padding: 4px 10px; border-radius: 4px;"
            "font-weight: bold;"
        );
        ui->lblPairStatusMessage->setText(error);

        ui->lblVisibleImage->setPixmap(QPixmap());
        ui->lblVisibleImage->setText(tr("Không thể hoàn tất nạp ảnh"));
        ui->lblThermalImage->setPixmap(QPixmap());
        ui->lblThermalImage->setText(tr("Không thể hoàn tất nạp ảnh"));

        ui->lblVisibleInfo->setText(
            tr("Kiểm định chưa hoàn tất — chưa xác định trạng thái giải mã.")
        );
        ui->lblThermalInfo->setText(
            tr("Kiểm định chưa hoàn tất — chưa xác định trạng thái giải mã.")
        );

        if (!isBatchEnhancing_ && result.enhancementRequestId == currentRequestId_) {
            activeN5PairId_ = result.pairId;
            activeThermalValid_ = false;

            cachedN5OriginalPixmap_ = QPixmap();
            activeN5OriginalBaseInfo_.clear();
            activeN5OriginalStats_ = ThermalPlaneStats{};
            clearN5EnhancedCache();

            ui->lblN5PairInfo->setText(
                QString("Cặp ảnh: %1").arg(result.pairId)
            );
            ui->viewerN5Original->clearImage(tr("Lỗi nạp/kiểm định"));
            ui->viewerN5Enhanced->clearImage(
                tr("Chưa có kết quả xử lý")
            );
            ui->lblN5OriginalInfo->setText(error);

            setN5UiState(N5UiState::Error, 0, error);
        }

        // Preserve existing validation data and list status.
        return;
    }

    entry.validationResult = result.validationResult;
    appController_.cacheValidationResult(*result.validationResult);
    cachedPreviewIndex_ = result.itemIndex;

    // Update list widget item status text & color
    if (auto *listItem = ui->lstImagePairs->item(result.itemIndex)) {
        updateListItemDisplay(listItem, entry);
    }

    // Cache the decoded QImages in memory
    cachedVisibleImage_ = result.visibleImage;
    cachedThermalImage_ = result.thermalImage;

    // Status badge & description on Tab 1
    PairValidationStatus status = result.validationResult->status();
    QString descText = getStatusDescription(status);
    QString messageText = result.validationResult->message().has_value()
                              ? QString::fromStdString(*result.validationResult->message())
                              : "";

    std::optional<int> visWidth, visHeight, thrWidth, thrHeight;
    if (result.validationResult->visibleDimensions().has_value()) {
        visWidth = result.validationResult->visibleDimensions()->width();
        visHeight = result.validationResult->visibleDimensions()->height();
    }
    if (result.validationResult->thermalDimensions().has_value()) {
        thrWidth = result.validationResult->thermalDimensions()->width();
        thrHeight = result.validationResult->thermalDimensions()->height();
    }

    QString tagText = getStatusTagText(status);
    QString colorHex = getStatusColorHex(status);

    if (status == PairValidationStatus::DimensionMismatch) {
        tagText = "LỆCH KÍCH THƯỚC (CHỜ CĂN CHỈNH)";
        colorHex = "#D97706";
        ui->lblPairStatusMessage->setText(
            QString("Cặp nguồn hợp lệ để nghiên cứu căn chỉnh (Phase 3). "
                    "Chưa đủ điều kiện xử lý chung theo pixel "
                    "(Visible: %1x%2 px, Thermal: %3x%4 px). "
                    "Hệ thống không tự resize để né kiểm định.")
                .arg(visWidth.value_or(0))
                .arg(visHeight.value_or(0))
                .arg(thrWidth.value_or(0))
                .arg(thrHeight.value_or(0)));
    } else if (!messageText.isEmpty()) {
        ui->lblPairStatusMessage->setText(QString("Chi tiết: %1 | %2").arg(messageText, descText));
    } else {
        ui->lblPairStatusMessage->setText(QString("Chi tiết: %1").arg(descText));
    }

    ui->lblPairStatus->setText(QString("Trạng thái: %1").arg(tagText));
    ui->lblPairStatus->setStyleSheet(QString("background-color: %1; color: #FFFFFF; padding: 4px 10px; border-radius: 4px; font-weight: bold;")
                                         .arg(colorHex));

    // Render Visible Image from in-memory QImage
    if (result.visDecodeSuccess && !cachedVisibleImage_.isNull()) {
        renderImageFromMemory(ui->lblVisibleImage, cachedVisibleImage_);
    } else if (!result.visFileExists) {
        ui->lblVisibleImage->setText("[!] Không tìm thấy file ảnh trên đĩa");
        ui->lblVisibleImage->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
        ui->lblVisibleImage->setPixmap(QPixmap());
        appendLog(QString("Failed to load VIS image: %1").arg(QString::fromStdString(entry.pair.visibleRelativePath())));
    } else {
        ui->lblVisibleImage->setText("[!] Không thể giải mã dữ liệu ảnh");
        ui->lblVisibleImage->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
        ui->lblVisibleImage->setPixmap(QPixmap());
        appendLog(QString("Rejected VIS file: %1").arg(QString::fromStdString(entry.pair.visibleRelativePath())));
    }

    const bool visOk = result.validationResult->visibleDecoded();
    QString visDimStr = (visWidth.has_value() && visHeight.has_value())
                            ? QString("%1 x %2 px").arg(*visWidth).arg(*visHeight)
                            : "--";
    ui->lblVisibleInfo->setText(QString("File: %1\nKích thước: %2 | OpenCV Decode: %3")
                                    .arg(QString::fromStdString(entry.pair.visibleRelativePath()), visDimStr)
                                    .arg(visOk ? "OK" : "Lỗi"));

    // Render Thermal Image from in-memory QImage
    if (result.thrDecodeSuccess && !cachedThermalImage_.isNull()) {
        renderImageFromMemory(ui->lblThermalImage, cachedThermalImage_);
    } else if (!result.thrFileExists) {
        ui->lblThermalImage->setText("[!] Không tìm thấy file ảnh trên đĩa");
        ui->lblThermalImage->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
        ui->lblThermalImage->setPixmap(QPixmap());
        appendLog(QString("Failed to load IR image: %1").arg(QString::fromStdString(entry.pair.thermalRelativePath())));
    } else {
        ui->lblThermalImage->setText("[!] Không thể giải mã dữ liệu ảnh");
        ui->lblThermalImage->setStyleSheet("background-color: #2b1d1d; color: #ff6b6b; border: 1px solid #d32f2f; border-radius: 6px; font-weight: bold;");
        ui->lblThermalImage->setPixmap(QPixmap());
        appendLog(QString("Rejected IR file: %1").arg(QString::fromStdString(entry.pair.thermalRelativePath())));
    }

    const bool thrOk = result.validationResult->thermalDecoded();
    QString thrDimStr = (thrWidth.has_value() && thrHeight.has_value())
                            ? QString("%1 x %2 px").arg(*thrWidth).arg(*thrHeight)
                            : "--";
    ui->lblThermalInfo->setText(QString("File: %1\nKích thước: %2 | OpenCV Decode: %3")
                                    .arg(QString::fromStdString(entry.pair.thermalRelativePath()), thrDimStr)
                                    .arg(thrOk ? "OK" : "Lỗi"));

    // Update Tab 2 (N5 Viewer) with in-memory decoded image without disk I/O
    if (!isBatchEnhancing_) {
        const bool preserveExistingEnhanced =
            (activeN5PairId_ == result.pairId)
            && thrOk
            && result.thrDecodeSuccess
            && !cachedThermalImage_.isNull()
            && !cachedN5EnhancedPixmap_.isNull();

        activeN5PairId_ = result.pairId;
        activeThermalValid_ = thrOk;

        if (result.thrDecodeSuccess && !cachedThermalImage_.isNull()) {
            cachedN5OriginalPixmap_ = QPixmap::fromImage(cachedThermalImage_);
            activeN5OriginalBaseInfo_ = QString("File: %1\n%2")
                                            .arg(QString::fromStdString(entry.pair.thermalRelativePath()), result.channelDesc);
            activeN5OriginalStats_ = result.sourceStats;
            ui->lblN5OriginalInfo->setText(
                formatOriginalMetricsSummary(activeN5OriginalBaseInfo_, activeN5OriginalStats_));
            ui->viewerN5Original->setPixmap(cachedN5OriginalPixmap_, false);
            ui->viewerN5Original->fitToView();
        } else if (!result.thrFileExists) {
            cachedN5OriginalPixmap_ = QPixmap();
            activeN5OriginalBaseInfo_.clear();
            activeN5OriginalStats_ = ThermalPlaneStats{};
            ui->lblN5OriginalInfo->setText("Không tìm thấy file ảnh nhiệt trên đĩa.");
            ui->viewerN5Original->clearImage("Không tìm thấy file ảnh nhiệt");
        } else {
            cachedN5OriginalPixmap_ = QPixmap();
            activeN5OriginalBaseInfo_.clear();
            activeN5OriginalStats_ = ThermalPlaneStats{};
            ui->lblN5OriginalInfo->setText("Không thể giải mã dữ liệu ảnh nhiệt.");
            ui->viewerN5Original->clearImage("Lỗi giải mã ảnh nhiệt");
        }

        // Reset, explicit Apply or a new batch supersedes this restoration.
        // Source preview above may still update; enhancement state must not.
        if (result.enhancementRequestId != currentRequestId_) {
            return;
        }

        if (!result.rehydrationError.isEmpty()) {
            clearN5EnhancedCache();
            ui->viewerN5Enhanced->clearImage(
                tr("Không thể tái tạo kết quả đã xử lý")
            );

            appendLog(
                QString("Restore failed [%1]: %2")
                    .arg(result.pairId, result.rehydrationError)
            );

            // Thermal validity remains independent of restoration success.
            // Existing Error-state logic permits explicit Apply to retry.
            setN5UiState(
                N5UiState::Error,
                0,
                result.rehydrationError
            );
            return;
        }

        if (result.rehydratedEnhancement
                && thrOk
                && !result.rehydratedEnhancedImage.isNull()) {
            const auto *cached = findPairEnhancement(result.pairId);

            // The original result must still exist and match this mode.
            if (cached == nullptr
                || cached->mode != result.rehydratedMode) {
                return;
                }

            // Keep the original execution time, not restoration overhead.
            const qint64 originalElapsedMs = cached->elapsedMs;
            const QString modeStr = cached->methodStr;

            EnhanceJobRequest synthReq;
            synthReq.pairId = result.pairId;
            synthReq.mode = result.rehydratedMode;
            synthReq.metrics = result.rehydratedMetrics;
            synthReq.diffHeatmap = result.rehydratedDiffHeatmap;
            synthReq.appliedParamsSummary =
                result.rehydratedAppliedParams;

            storePairEnhancementResult(
                synthReq,
                result.rehydratedEnhancedImage,
                originalElapsedMs,
                modeStr
            );
            updateEnhancedResultDisplay(
                synthReq,
                result.rehydratedEnhancedImage,
                originalElapsedMs,
                modeStr
            );

            appendLog(
                QString(
                    "Restored %1 [%2] by background recomputation "
                    "in %3 ms; original execution time: %4 ms.")
                    .arg(result.pairId, modeStr)
                    .arg(result.rehydratedElapsedMs)
                    .arg(originalElapsedMs)
            );

            ui->btnApplyN5->setToolTip(
                tr("Áp dụng xử lý thuật toán cho ảnh nhiệt.")
            );
            setN5UiState(
                N5UiState::Completed,
                originalElapsedMs,
                QString(),
                modeStr
            );
        } else if (!preserveExistingEnhanced) {
            // STALE INVALIDATION: Xóa kết quả N5 cũ khi nạp cặp chưa xử lý
            clearN5EnhancedCache();
            ui->viewerN5Enhanced->clearImage(tr("Bấm 'Áp dụng xử lý' để chạy N5 hoặc BYPASS"));

            if (!activeThermalValid_) {
                ui->btnApplyN5->setEnabled(false);
                ui->btnApplyN5->setToolTip(tr("Ảnh nhiệt không đạt điều kiện kiểm định (thiếu file hoặc lỗi giải mã)."));
                ui->lblN5Status->setText(tr("Trạng thái: Không thể xử lý (Ảnh nhiệt không hợp lệ)"));
                ui->lblN5Status->setStyleSheet("background-color: #3d1a1a; color: #FF6B6B; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
                ui->lblN5EnhancedInfo->setText(tr("Thời gian: — | Trạng thái: Không thể xử lý"));
            } else {
                ui->btnApplyN5->setToolTip(tr("Áp dụng xử lý thuật toán cho ảnh nhiệt."));
                setN5UiState(N5UiState::NotRun);
            }

            ui->viewerN5Enhanced->fitToView();
        }
    }
}

void MainWindow::renderImageFromMemory(QLabel *label, const QImage &image)
{
    if (image.isNull()) {
        label->setPixmap(QPixmap());
        return;
    }

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

    QPixmap pixmap = QPixmap::fromImage(image);
    QPixmap scaledPixmap = pixmap.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    label->setPixmap(scaledPixmap);
    label->setStyleSheet("background-color: #1e1e1e; color: #888888; border: 1px solid #333333; border-radius: 6px;");
}

void MainWindow::displayPair(const PairItemEntry &entry) {
    Q_UNUSED(entry);
    if (!cachedVisibleImage_.isNull()) {
        renderImageFromMemory(ui->lblVisibleImage, cachedVisibleImage_);
    }
    if (!cachedThermalImage_.isNull()) {
        renderImageFromMemory(ui->lblThermalImage, cachedThermalImage_);
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
            clearAllPairEnhancementCache();
            invalidateActiveSessionAndWorkers(true);

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
        invalidateActiveSessionAndWorkers(true);
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
        invalidateActiveSessionAndWorkers(true);

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
        invalidateActiveSessionAndWorkers(true);
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
        invalidateActiveSessionAndWorkers(true);

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
            invalidateActiveSessionAndWorkers(true);
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
            if (auto *currentItem = ui->lstImagePairs->currentItem()) {
                int idx = currentItem->data(Qt::UserRole).toInt();
                if (idx >= 0 && idx < static_cast<int>(displayedItems_.size())) {
                    if (cachedPreviewIndex_ != idx) {
                        onPairSelected(currentItem);
                    } else {
                        displayPair(displayedItems_[idx]);
                    }
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

void MainWindow::updateBatchButtonText()
{
    if (isBatchEnhancing_) {
        ui->btnEnhanceAll->setEnabled(true);
        ui->btnEnhanceAll->setText(isBatchCancelling_ ? tr("Đang dừng tăng cường...") : tr("Dừng tăng cường"));
        ui->btnEnhanceAll->setStyleSheet("background-color: #7B1113; color: #FFFFFF; font-weight: bold; border-radius: 4px; padding: 6px;");
        return;
    }
    ui->btnEnhanceAll->setStyleSheet("");
    const int count = static_cast<int>(displayedItems_.size());
    if (count > 0) {
        ui->btnEnhanceAll->setText(QString("Tăng cường danh sách hiển thị (%1 cặp)").arg(count));
        ui->btnEnhanceAll->setEnabled(n5State_ != N5UiState::Processing);
    } else {
        ui->btnEnhanceAll->setText(tr("Tăng cường danh sách hiển thị"));
        ui->btnEnhanceAll->setEnabled(false);
    }
}

void MainWindow::onApplyN5Clicked()
{
    try {
        if (isBatchEnhancing_) {
            QMessageBox::information(this, "Thông báo", "Đang chạy xử lý hàng loạt. Vui lòng dừng batch trước khi chạy đơn ảnh.");
            return;
        }

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

        auto &entry = displayedItems_[idx];
        if (!entry.validationResult.has_value()) {
            entry.validationResult = appController_.getOrValidatePair(entry.pair);
        }

        // Tách biệt điều kiện kiểm định: Chỉ cần kênh nhiệt hợp lệ (thermalDecoded == true),
        // không yêu cầu 2 ảnh phải cùng kích thước.
        if (!entry.validationResult->thermalDecoded()) {
            activeThermalValid_ = false;
            appendLog(QString("Enhancement blocked: pair %1 thermal image failed validation.").arg(activeN5PairId_));
            QMessageBox::warning(this, "Không đủ điều kiện xử lý",
                                 "Ảnh nhiệt của cặp này không đạt điều kiện kiểm định (thiếu file hoặc lỗi giải mã).");
            return;
        }
        activeThermalValid_ = true;

        const auto &pair = entry.pair;
        std::filesystem::path fullPath = appController_.datasetRoot() / pair.thermalRelativePath();

        const auto mode = ui->radModeBypass->isChecked()
                              ? qart::application::ThermalProcessingMode::Bypass
                              : qart::application::ThermalProcessingMode::N5PostHalf;
        const QString modeStr = (mode == qart::application::ThermalProcessingMode::Bypass) ? "BYPASS" : "N5";

        // 1. Chuyển UI sang trạng thái Processing và khóa nút Apply chống spam click
        setN5UiState(N5UiState::Processing, 0, QString(), modeStr);
        ui->viewerN5Enhanced->clearImage(QString("Đang xử lý worker thread (%1)...").arg(modeStr));
        appendLog(QString("Started enhancement for pair %1 (Method: %2)").arg(activeN5PairId_, modeStr));

        // 2. Retire an toàn worker trước đó (không xóa thread khi còn đang chạy)
        retireSingleWorker();

        currentRequestId_++;
        EnhanceJobRequest req;
        req.sessionId = currentSessionId_;
        req.requestId = currentRequestId_;
        req.pairId = activeN5PairId_;
        req.datasetId = QString::fromStdString(appController_.currentDatasetId());
        req.datasetRoot = appController_.datasetRoot();
        req.thermalFullPath = fullPath;
        req.mode = mode;

        singleWorker_ = new SingleEnhanceWorkerThread(req, this);
        connect(singleWorker_.data(), &SingleEnhanceWorkerThread::jobFinished,
                this, &MainWindow::onSingleWorkerFinished);
        connect(singleWorker_.data(), &QThread::finished,
                singleWorker_.data(), &QObject::deleteLater);
        singleWorker_->start();
    } catch (const std::exception &ex) {
        setN5UiState(N5UiState::Error, 0, ex.what());
        QMessageBox::critical(this, "Lỗi xử lý", QString("Đã xảy ra lỗi khi kích hoạt xử lý:\n%1").arg(ex.what()));
    }
}

void MainWindow::onSingleWorkerFinished(const EnhanceJobRequest &request,
                                        const QImage &resultImage,
                                        qint64 elapsedMs,
                                        bool success,
                                        const QString &errorMessage)
{
    if (sender() == singleWorker_.data()) {
        singleWorker_.clear();
    }

    // STALE INVALIDATION: So khớp session ID, request ID và pair ID
    if (request.sessionId != currentSessionId_
        || request.requestId != currentRequestId_
        || request.pairId != activeN5PairId_
        || isBatchEnhancing_) {
        appendLog(QString("Dropped stale result for pair %1 (request #%2, active request is #%3).")
                      .arg(request.pairId)
                      .arg(request.requestId)
                      .arg(currentRequestId_));
        return;
    }

    const QString modeStr = (request.mode == qart::application::ThermalProcessingMode::Bypass) ? "BYPASS" : "N5";

    if (success && !resultImage.isNull()) {
        storePairEnhancementResult(request, resultImage, elapsedMs, modeStr);
        updateEnhancedResultDisplay(request, resultImage, elapsedMs, modeStr);
        setN5UiState(N5UiState::Completed, elapsedMs, QString(), modeStr);
        refreshPairListDisplayById(request.pairId, activePreviewIndex_);

        if (request.metrics.valid) {
            appendLog(QString("Finished enhancement for pair %1 in %2 ms (Method: %3 | Params: %4 | %5).")
                          .arg(request.pairId)
                          .arg(elapsedMs)
                          .arg(modeStr, request.appliedParamsSummary, formatEnhancementLogMetrics(request.metrics)));
        } else {
            appendLog(QString("Finished enhancement for pair %1 in %2 ms (Method: %3).")
                          .arg(request.pairId)
                          .arg(elapsedMs)
                          .arg(modeStr));
        }

        // Lưu kết quả thực vào danh sách Phase 2 với đúng datasetId và mode của request
        Phase2Record rec;
        rec.pairId = request.pairId;
        rec.datasetId = request.datasetId;
        rec.processingMethod = modeStr;
        rec.processingVersion = (modeStr == "BYPASS") ? "BYPASS-v1.0" : "N5PostHalf-v1.0";
        rec.status = "SUCCESS";
        rec.elapsedMs = elapsedMs;
        phase2Results_.push_back(rec);
    } else {
        removePairEnhancementCache(request.pairId);
        refreshPairListDisplayById(request.pairId, activePreviewIndex_);
        clearN5EnhancedCache();
        ui->viewerN5Enhanced->clearImage(QString("Lỗi: %1").arg(errorMessage));
        setN5UiState(N5UiState::Error, elapsedMs, errorMessage, modeStr);
        appendLog(QString("Failed to process %1 for pair %2: %3").arg(modeStr, request.pairId, errorMessage));

        Phase2Record rec;
        rec.pairId = request.pairId;
        rec.datasetId = request.datasetId;
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
        // Nếu đang trong tiến trình xử lý hàng loạt -> Bấm lại để HỦY / DỪNG
        if (isBatchEnhancing_) {
            if (!isBatchCancelling_) {
                isBatchCancelling_ = true;
                if (batchWorker_) {
                    batchWorker_->requestCancel();
                    batchWorker_->requestInterruption();
                }
                updateBatchButtonText();
                ui->lblN5Status->setText("Đang yêu cầu dừng xử lý...");
                appendLog("Batch processing cancelled by user.");
            }
            return;
        }

        if (displayedItems_.empty()) {
            QMessageBox::information(this, "Thông báo", "Chưa có danh sách ảnh nào để xử lý toàn bộ.");
            return;
        }

        if (appController_.datasetRoot().empty()) {
            QMessageBox::warning(this, "Lỗi Dataset", "Chưa có đường dẫn dataset root hợp lệ để xử lý hàng loạt.");
            return;
        }

        // Retire any lingering single-image worker cleanly before starting batch
        retireSingleWorker();
        currentRequestId_++;

        const auto mode = ui->radModeBypass->isChecked()
                              ? qart::application::ThermalProcessingMode::Bypass
                              : qart::application::ThermalProcessingMode::N5PostHalf;
        const QString modeStr = (mode == qart::application::ThermalProcessingMode::Bypass) ? "BYPASS" : "N5";

        std::vector<EnhanceJobRequest> requests;
        requests.reserve(displayedItems_.size());

        currentBatchId_++;
        for (const auto &item : displayedItems_) {
            EnhanceJobRequest req;
            req.sessionId = currentSessionId_;
            req.requestId = currentBatchId_;
            req.pairId = QString::fromStdString(item.pair.pairId());
            req.datasetId = QString::fromStdString(appController_.currentDatasetId());
            req.datasetRoot = appController_.datasetRoot();
            req.thermalFullPath = req.datasetRoot / item.pair.thermalRelativePath();
            req.mode = mode;
            req.pair = item.pair;
            req.validationResult = item.validationResult;

            if (item.validationResult.has_value() && !item.validationResult->thermalDecoded()) {
                req.isSkipped = true;
                req.skipReason = "Lỗi kiểm định ảnh nhiệt";
            }
            requests.push_back(req);
        }

        batchTotalCount_ = static_cast<int>(requests.size());
        isBatchEnhancing_ = true;
        isBatchCancelling_ = false;
        n5State_ = N5UiState::NotRun;

        ui->btnApplyN5->setEnabled(false);
        ui->btnResetN5->setEnabled(false);
        ui->prgN5->setRange(0, batchTotalCount_);
        ui->prgN5->setValue(0);
        ui->prgN5->setVisible(true);
        updateBatchButtonText();

        ui->lblN5Status->setText(QString("Trạng thái: Đang xử lý hàng loạt 0/%1 (%2)...")
                                     .arg(batchTotalCount_)
                                     .arg(modeStr));
        ui->lblN5Status->setStyleSheet("background-color: #3b2d18; color: #E5A93C; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        ui->lblN5EnhancedInfo->setText(QString("Thời gian: — | Đang chuẩn bị hàng loạt (%1)...").arg(modeStr));
        appendLog(QString("Started batch processing for %1 pairs (%2).").arg(batchTotalCount_).arg(modeStr));

        retireBatchWorker();

        batchWorker_ = new BatchEnhanceWorkerThread(currentSessionId_, currentBatchId_, std::move(requests), this);
        connect(batchWorker_.data(), &BatchEnhanceWorkerThread::jobStepFinished,
                this, &MainWindow::onBatchStepFinished);
        connect(batchWorker_.data(), &BatchEnhanceWorkerThread::batchFinished,
                this, &MainWindow::onBatchWorkerFinished);
        connect(batchWorker_.data(), &QThread::finished,
                batchWorker_.data(), &QObject::deleteLater);
        batchWorker_->start();
    } catch (const std::exception &ex) {
        isBatchEnhancing_ = false;
        isBatchCancelling_ = false;
        batchTotalCount_ = 0;
        ui->prgN5->setVisible(false);
        updateBatchButtonText();
        setN5UiState(N5UiState::Error, 0, ex.what());
        QMessageBox::critical(this, "Lỗi xử lý hàng loạt", QString("Đã xảy ra lỗi:\n%1").arg(ex.what()));
    }
}

void MainWindow::onBatchStepFinished(quint64 sessionId,
                                     quint64 batchId,
                                     int index,
                                     int totalCount,
                                     const EnhanceJobRequest &request,
                                     const QImage &resultImage,
                                     qint64 elapsedMs,
                                     bool success,
                                     const QString &errorMessage,
                                     int successCount,
                                     int errorCount,
                                     int skippedCount,
                                     int cancelledCount)
{
    Q_UNUSED(cancelledCount);

    if (sessionId != currentSessionId_ || batchId != currentBatchId_) {
        appendLog(QString("Dropped stale batch step (session %1/%2, batch %3/%4, pair %5).")
                      .arg(sessionId).arg(currentSessionId_)
                      .arg(batchId).arg(currentBatchId_)
                      .arg(request.pairId));
        return;
    }

    // Highlight current row without triggering onPairSelected -> updateQualityN5Tab reset!
    if (index >= 0 && index < ui->lstImagePairs->count()) {
        const QSignalBlocker blocker(ui->lstImagePairs);
        ui->lstImagePairs->setCurrentRow(index);
        if (auto *item = ui->lstImagePairs->item(index)) {
            ui->lstImagePairs->scrollToItem(item);
        }
    }

    activeN5PairId_ = request.pairId;
    activeThermalValid_ =
        request.validationResult.has_value()
        && request.validationResult->thermalDecoded();

    QString sourcePath =
        QString::fromStdString(request.thermalFullPath.string());

    ui->lblN5PairInfo->setText(
        QString("Cặp ảnh: %1").arg(request.pairId)
    );

    if (index >= 0
        && index < static_cast<int>(displayedItems_.size())
        && displayedItems_[index].pair.pairId()
            == request.pairId.toStdString()) {
        auto &entry = displayedItems_[index];

        if (request.validationResult.has_value()) {
            entry.validationResult = request.validationResult;
            appController_.cacheValidationResult(*request.validationResult);
            if (auto *item = ui->lstImagePairs->item(index)) {
                updateListItemDisplay(item, entry);
            }
        }

        activeThermalValid_ =
            entry.validationResult.has_value()
            && entry.validationResult->thermalDecoded();

        sourcePath =
            QString::fromStdString(entry.pair.thermalRelativePath());

        ui->lblN5PairInfo->setText(
            QString("Cặp ảnh: %1 (Stem: %2 | Partition: %3)")
                .arg(
                    request.pairId,
                    QString::fromStdString(entry.pair.sourceStem()),
                    QString::fromStdString(
                        entry.pair.partitionId().value()
                    )
                )
        );
    }

    // Display worker-owned image data; no file loading here.
    if (!request.sourcePreview.isNull()) {
        cachedN5OriginalPixmap_ =
            QPixmap::fromImage(request.sourcePreview);

        ui->viewerN5Original->setPixmap(
            cachedN5OriginalPixmap_,
            false
        );
        ui->viewerN5Original->fitToView();

        QString sourceInfo =
            QString("File: %1\nẢnh gốc: %2 × %3 px | %4 kênh%5")
                .arg(sourcePath)
                .arg(request.sourcePreview.width())
                .arg(request.sourcePreview.height())
                .arg(request.sourceChannels)
                .arg(request.sourceChannels > 1 ? QString(" (BGR → Gray8)") : QString(" (Gray8)"));

        if (request.sourceChannels > 1) {
            sourceInfo += tr("\n[Giữ nguyên biểu diễn, không tự giải mã bảng màu]");
        }

        activeN5OriginalBaseInfo_ = sourceInfo;
        activeN5OriginalStats_ = ThermalPlaneStats{};
        ui->lblN5OriginalInfo->setText(activeN5OriginalBaseInfo_);
    } else {
        cachedN5OriginalPixmap_ = QPixmap();
        activeN5OriginalBaseInfo_.clear();
        activeN5OriginalStats_ = ThermalPlaneStats{};

        ui->viewerN5Original->clearImage(
            tr("Không có preview nguồn cho mục này")
        );
        ui->lblN5OriginalInfo->setText(
            QString("File: %1\n%2")
                .arg(
                    sourcePath,
                    tr("Preview nguồn không khả dụng; xem trạng thái xử lý.")
                )
        );
    }

    const QString modeStr = (request.mode == qart::application::ThermalProcessingMode::Bypass) ? "BYPASS" : "N5";

    if (success && !resultImage.isNull()) {
        storePairEnhancementResult(request, resultImage, elapsedMs, modeStr);
        updateEnhancedResultDisplay(request, resultImage, elapsedMs, modeStr);
        refreshPairListDisplayById(request.pairId, index);

        Phase2Record rec;
        rec.pairId = request.pairId;
        rec.datasetId = request.datasetId;
        rec.processingMethod = modeStr;
        rec.processingVersion = (modeStr == "BYPASS") ? "BYPASS-v1.0" : "N5PostHalf-v1.0";
        rec.status = "SUCCESS";
        rec.elapsedMs = elapsedMs;
        phase2Results_.push_back(rec);
    } else {
        removePairEnhancementCache(request.pairId);
        refreshPairListDisplayById(request.pairId, index);
        clearN5EnhancedCache();
        ui->viewerN5Enhanced->clearImage(QString("Lỗi/Bỏ qua: %1").arg(errorMessage));
        ui->lblN5EnhancedInfo->setText(QString("Cặp: %1 | %2").arg(request.pairId, errorMessage));
        appendLog(QString("Batch item (%1): pair %2 - %3").arg(request.isSkipped ? "Skipped" : "Error", request.pairId, errorMessage));

        Phase2Record rec;
        rec.pairId = request.pairId;
        rec.datasetId = request.datasetId;
        rec.processingMethod = modeStr;
        rec.processingVersion = (modeStr == "BYPASS") ? "BYPASS-v1.0" : "N5PostHalf-v1.0";
        rec.status = request.isSkipped ? "SKIPPED" : "ERROR";
        rec.elapsedMs = elapsedMs > 0 ? std::optional<qint64>(elapsedMs) : std::nullopt;
        rec.errorMessage = errorMessage;
        phase2Results_.push_back(rec);
    }

    ui->prgN5->setRange(0, totalCount);
    ui->prgN5->setValue(index + 1);
    ui->prgN5->setVisible(true);
    ui->btnEnhanceAll->setEnabled(true);
    ui->lblN5Status->setText(QString("Đang xử lý: %1/%2 | Thành công: %3 | Lỗi: %4 | Bỏ qua: %5")
                                 .arg(index + 1)
                                 .arg(totalCount)
                                 .arg(successCount)
                                 .arg(errorCount)
                                 .arg(skippedCount));
}

void MainWindow::onBatchWorkerFinished(quint64 sessionId,
                                       quint64 batchId,
                                       int totalCount,
                                       int successCount,
                                       int errorCount,
                                       int skippedCount,
                                       int cancelledCount)
{
    if (sender() == batchWorker_.data()) {
        batchWorker_.clear();
    }

    if (sessionId != currentSessionId_ || batchId != currentBatchId_) {
        appendLog(QString("Dropped stale batch completion (session %1/%2, batch %3/%4).")
                      .arg(sessionId).arg(currentSessionId_)
                      .arg(batchId).arg(currentBatchId_));
        return;
    }

    const bool stoppedEarly = (cancelledCount > 0 || isBatchCancelling_);
    isBatchEnhancing_ = false;
    isBatchCancelling_ = false;
    batchTotalCount_ = 0;

    ui->prgN5->setVisible(false);
    updateBatchButtonText();

    if (stoppedEarly) {
        n5State_ = N5UiState::Cancelled;
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty() && activeThermalValid_);
        ui->btnResetN5->setEnabled(!cachedN5EnhancedPixmap_.isNull());
        ui->lblN5Status->setText(QString("Đã hủy: Tổng %1 | Thành công: %2 | Lỗi: %3 | Bỏ qua: %4 | Đã hủy: %5")
                                     .arg(totalCount).arg(successCount).arg(errorCount).arg(skippedCount).arg(cancelledCount));
        ui->lblN5Status->setStyleSheet("background-color: #2b2b2b; color: #E0E0E0; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        appendLog(QString("Batch cancelled: %1 succeeded, %2 failed, %3 skipped, %4 cancelled (Total %5).")
                      .arg(successCount).arg(errorCount).arg(skippedCount).arg(cancelledCount).arg(totalCount));
    } else {
        n5State_ = N5UiState::Completed;
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty() && activeThermalValid_);
        ui->btnResetN5->setEnabled(!cachedN5EnhancedPixmap_.isNull());
        ui->lblN5Status->setText(QString("Hoàn tất: Tổng %1 | Thành công: %2 | Lỗi: %3 | Bỏ qua: %4")
                                     .arg(totalCount).arg(successCount).arg(errorCount).arg(skippedCount));
        ui->lblN5Status->setStyleSheet("background-color: #183329; color: #39BDA7; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        appendLog(QString("Batch completed: %1 succeeded, %2 failed, %3 skipped (Total %4).")
                      .arg(successCount).arg(errorCount).arg(skippedCount).arg(totalCount));
    }
}

void MainWindow::onResetN5Clicked()
{
    try {
        currentRequestId_++;
        currentBatchId_++;
        retireBatchWorker();
        retireSingleWorker();

        isBatchEnhancing_ = false;
        isBatchCancelling_ = false;
        batchTotalCount_ = 0;

        if (!activeN5PairId_.isEmpty()) {
            removePairEnhancementCache(activeN5PairId_);
            refreshPairListDisplayById(activeN5PairId_, activePreviewIndex_);
        }
        clearN5EnhancedCache();
        ui->viewerN5Enhanced->clearImage("Đã khôi phục ảnh gốc. Bấm 'Áp dụng xử lý' để chạy.");
        ui->viewerN5Original->fitToView();
        ui->viewerN5Enhanced->fitToView();
        updateBatchButtonText();
        setN5UiState(N5UiState::NotRun);
        appendLog(QString("Cleared IR image cache for pair %1").arg(activeN5PairId_));
    } catch (const std::exception &ex) {
        ui->lblSystemStatus->setText(QString("Lỗi reset: %1").arg(ex.what()));
    }
}

void MainWindow::onAlgorithmParamChanged()
{
    if (isSyncingParams_) {
        return;
    }
    if (ui->chkAutoUpdate && ui->chkAutoUpdate->isChecked()) {
        if (ui->btnApplyN5 && ui->btnApplyN5->isEnabled()
            && n5State_ != N5UiState::Processing && !isBatchEnhancing_) {
            onApplyN5Clicked();
        }
    }
}

void MainWindow::onResetAlgorithmParamsClicked()
{
    isSyncingParams_ = true;
    ui->spnParamRadius->setValue(3);
    ui->sldParamRadius->setValue(3);

    ui->spnParamEps->setValue(0.101);
    ui->sldParamEps->setValue(101);

    ui->spnParamH->setValue(7.6);
    ui->sldParamH->setValue(76);

    ui->spnParamGamma->setValue(2.00);
    ui->sldParamGamma->setValue(200);

    ui->spnParamAlpha->setValue(0.46);
    ui->sldParamAlpha->setValue(46);

    ui->cmbParamMode->setCurrentIndex(0);
    ui->cmbParamDetailNorm->setCurrentIndex(0);

    ui->chkParamAdaptiveNlm->setChecked(false);
    ui->chkAutoUpdate->setChecked(false);
    isSyncingParams_ = false;

    appendLog(tr("Đã khôi phục thông số thuật toán về mặc định (Paper): r=3, eps=0.101, h=7.6, gamma=2.00, alpha=0.46."));

    if (ui->chkAutoUpdate->isChecked() && ui->btnApplyN5->isEnabled()) {
        onApplyN5Clicked();
    }
}

void MainWindow::onExportPhase2Clicked()
{
    if (phase2Results_.empty()) {
        appendLog("Save blocked: no result available.");
        QMessageBox::warning(
            this,
            tr("Xuất kết quả Phase 2"),
            tr("Chưa có bản ghi xử lý nào để xuất.")
        );
        return;
    }

    // Freeze the records before opening the modal file dialog.
    // Workers may finish while the dialog is open.
    const auto records = phase2Results_;
    const qlonglong rowCount =
        static_cast<qlonglong>(records.size());

    const QString suggestedName =
        QString("phase2_execution_%1.csv").arg(
            QDateTime::currentDateTime().toString(
                "yyyyMMdd_HHmmss"
            )
        );

    QString outputPath = QFileDialog::getSaveFileName(
        this,
        tr("Lưu báo cáo thực thi Phase 2"),
        suggestedName,
        tr("CSV (*.csv)")
    );

    if (outputPath.isEmpty()) {
        return;
    }

    if (!outputPath.endsWith(".csv", Qt::CaseInsensitive)) {
        outputPath += ".csv";

        // The file dialog did not check this appended filename.
        if (QFileInfo::exists(outputPath)) {
            const auto answer = QMessageBox::question(
                this,
                tr("Ghi đè báo cáo"),
                tr("Tệp đã tồn tại:\n%1\n\nGhi đè?")
                    .arg(outputPath),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );

            if (answer != QMessageBox::Yes) {
                return;
            }
        }
    }

    // Always quote fields; embedded quotes become doubled quotes.
    // This preserves commas, quotes and embedded line breaks.
    const auto csvField = [](QString value) -> QString {
        value.replace(
            QStringLiteral("\""),
            QStringLiteral("\"\"")
        );

        return QStringLiteral("\"")
            + value
            + QStringLiteral("\"");
    };

    QString csv = QStringLiteral(
        "pair_id,dataset_id,processing_method,"
        "processing_version,status,elapsed_ms,error_message\r\n"
    );

    for (const auto &record : records) {
        const QString elapsed =
            record.elapsedMs.has_value()
                ? QString::number(*record.elapsedMs)
                : QString();

        QStringList fields;
        fields << csvField(record.pairId)
               << csvField(record.datasetId)
               << csvField(record.processingMethod)
               << csvField(record.processingVersion)
               << csvField(record.status)
               << csvField(elapsed)
               << csvField(record.errorMessage);

        csv += fields.join(QLatin1Char(','));
        csv += QStringLiteral("\r\n");
    }

    QSaveFile file(outputPath);

    if (!file.open(QIODevice::WriteOnly)) {
        const QString error = file.errorString();

        appendLog(
            QString("Phase 2 CSV open failed: %1").arg(error)
        );
        QMessageBox::critical(
            this,
            tr("Không thể lưu báo cáo"),
            tr("Không thể mở tệp:\n%1\n\n%2")
                .arg(outputPath, error)
        );
        return;
    }

    const QByteArray bytes = csv.toUtf8();
    const qint64 expectedBytes =
        static_cast<qint64>(bytes.size());

    if (file.write(bytes) != expectedBytes) {
        const QString error = file.errorString();
        file.cancelWriting();

        appendLog(
            QString("Phase 2 CSV write failed: %1").arg(error)
        );
        QMessageBox::critical(
            this,
            tr("Không thể lưu báo cáo"),
            tr("Không ghi được đầy đủ dữ liệu:\n%1\n\n%2")
                .arg(outputPath, error)
        );
        return;
    }

    if (!file.commit()) {
        const QString error = file.errorString();

        appendLog(
            QString("Phase 2 CSV commit failed: %1").arg(error)
        );
        QMessageBox::critical(
            this,
            tr("Không thể hoàn tất lưu báo cáo"),
            tr("Không thể hoàn tất tệp:\n%1\n\n%2")
                .arg(outputPath, error)
        );
        return;
    }

    appendLog(
        QString("Exported %1 Phase 2 execution records: %2")
            .arg(rowCount)
            .arg(outputPath)
    );

    QMessageBox::information(
        this,
        tr("Đã lưu báo cáo Phase 2"),
        tr("Đã ghi %1 bản ghi vào:\n%2\n\n"
           "Báo cáo chứa các bản ghi có tại thời điểm bấm xuất; "
           "không bao gồm mục chưa có bản ghi xử lý.")
            .arg(rowCount)
            .arg(outputPath)
    );
}

void MainWindow::setN5UiState(N5UiState state, qint64 elapsedMs, const QString &detailMsg, const QString &methodOverride)
{
    n5State_ = state;
    const QString methodStr = !methodOverride.isEmpty()
                                  ? methodOverride
                                  : ((ui->radModeBypass && ui->radModeBypass->isChecked()) ? "BYPASS" : "N5");

    // Batch execution owns prgN5 & btnEnhanceAll while active so single-image state updates never interrupt it.
    if (isBatchEnhancing_) {
        ui->btnApplyN5->setEnabled(false);
        ui->btnResetN5->setEnabled(false);
        updateBatchButtonText();
        return;
    }

    switch (state) {
    case N5UiState::NotRun:
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty() && activeThermalValid_);
        updateBatchButtonText();
        ui->btnResetN5->setEnabled(!cachedN5EnhancedPixmap_.isNull());
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
        ui->prgN5->setRange(0, 0); // Marquee animation for single-image processing only
        ui->lblN5Status->setText(QString("Trạng thái: Đang xử lý (%1)...").arg(methodStr));
        ui->lblN5Status->setStyleSheet("background-color: #3b2d18; color: #E5A93C; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        ui->lblN5EnhancedInfo->setText(QString("Thời gian: — | Trạng thái: Đang xử lý (%1)...").arg(methodStr));
        break;

    case N5UiState::Completed:
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty() && activeThermalValid_);
        updateBatchButtonText();
        ui->btnResetN5->setEnabled(!cachedN5EnhancedPixmap_.isNull());
        ui->prgN5->setVisible(false);
        if (elapsedMs >= 0) {
            ui->lblN5Status->setText(QString("Trạng thái: Hoàn tất (%1 ms | %2)").arg(elapsedMs).arg(methodStr));
        } else {
            ui->lblN5Status->setText(QString("Trạng thái: Hoàn tất | %1").arg(methodStr));
        }
        if (lastThermalMetrics_.valid && !lastMethodStr_.isEmpty()) {
            const bool showingDiff = ui->chkShowDiffMap
                                     && ui->chkShowDiffMap->isChecked()
                                     && !cachedN5DiffPixmap_.isNull();
            ui->lblN5EnhancedInfo->setText(formatEnhancementMetricsSummary(
                lastMethodStr_,
                lastElapsedMs_,
                lastAppliedParams_,
                lastThermalMetrics_,
                showingDiff));
        } else if (elapsedMs >= 0) {
            ui->lblN5EnhancedInfo->setText(QString("Thời gian: %1 ms | Trạng thái: Hoàn tất | %2").arg(elapsedMs).arg(methodStr));
        } else {
            ui->lblN5EnhancedInfo->setText(QString("Thời gian: — | Trạng thái: Hoàn tất | %1").arg(methodStr));
        }
        ui->lblN5Status->setStyleSheet("background-color: #183329; color: #39BDA7; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        break;

    case N5UiState::Error:
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty() && activeThermalValid_);
        updateBatchButtonText();
        ui->btnResetN5->setEnabled(false);
        ui->prgN5->setVisible(false);
        ui->lblN5Status->setText("Trạng thái: Lỗi");
        ui->lblN5Status->setStyleSheet("background-color: #3d1a1a; color: #FF6B6B; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        ui->lblN5EnhancedInfo->setText(QString("Thời gian: — | Trạng thái: Lỗi | %1").arg(detailMsg));
        break;

    case N5UiState::Cancelled:
        ui->btnApplyN5->setEnabled(!activeN5PairId_.isEmpty() && activeThermalValid_);
        updateBatchButtonText();
        ui->btnResetN5->setEnabled(!cachedN5EnhancedPixmap_.isNull());
        ui->prgN5->setVisible(false);
        ui->lblN5Status->setText("Trạng thái: Đã hủy");
        ui->lblN5Status->setStyleSheet("background-color: #2b2b2b; color: #E0E0E0; padding: 4px 8px; border-radius: 4px; font-weight: bold;");
        ui->lblN5EnhancedInfo->setText("Thời gian: — | Trạng thái: Đã hủy");
        break;
    }
}

void MainWindow::clearN5EnhancedCache()
{
    cachedN5EnhancedPixmap_ = QPixmap();
    cachedN5DiffPixmap_     = QPixmap();
    lastThermalMetrics_     = ThermalEnhancementMetrics{};
    lastMethodStr_.clear();
    lastAppliedParams_.clear();
    lastElapsedMs_ = -1;
    if (ui->chkShowDiffMap) {
        QSignalBlocker b(ui->chkShowDiffMap);
        ui->chkShowDiffMap->setEnabled(false);
        ui->chkShowDiffMap->setChecked(false);
    }
}

// static
QString MainWindow::formatEnhancementLogMetrics(const ThermalEnhancementMetrics &m)
{
    if (!m.valid) return QString();
    return QString("Mean %1→%2 Std %3→%4 Entropy %5→%6 bit/px "
                   "Edge %7→%8 MAE=%9 MaxΔ=%10 Changed=%11%")
        .arg(m.inputMean,  0, 'f', 1).arg(m.outputMean,    0, 'f', 1)
        .arg(m.inputStd,   0, 'f', 1).arg(m.outputStd,     0, 'f', 1)
        .arg(m.inputEntropy,  0, 'f', 2).arg(m.outputEntropy,  0, 'f', 2)
        .arg(m.inputEdgeGrad, 0, 'f', 1).arg(m.outputEdgeGrad, 0, 'f', 1)
        .arg(m.mae,  0, 'f', 1)
        .arg(m.maxAbsDiff)
        .arg(m.changedPixelPercent, 0, 'f', 1);
}

// static
QString MainWindow::formatOriginalMetricsSummary(const QString &baseInfo,
                                                 const ThermalPlaneStats &stats)
{
    if (!stats.valid) {
        return baseInfo;
    }
    const QString statsText = QString(
        "Mean: %1 | Std: %2 | Dải: %3..%4\n"
        "Entropy: %5 bit/px | Sobel/4: %6")
        .arg(stats.mean, 0, 'f', 1)
        .arg(stats.stdDev, 0, 'f', 1)
        .arg(stats.minVal)
        .arg(stats.maxVal)
        .arg(stats.entropy, 0, 'f', 2)
        .arg(stats.edgeGradient, 0, 'f', 1);
    if (baseInfo.isEmpty()) {
        return statsText;
    }
    return baseInfo + QLatin1Char('\n') + statsText;
}

// static
QString MainWindow::formatEnhancementMetricsSummary(
    const QString &methodStr, qint64 elapsedMs, const QString &appliedParams,
    const ThermalEnhancementMetrics &m, bool showingDiffMap)
{
    if (!m.valid) {
        return QString("Thời gian: %1 ms | Trạng thái: Hoàn tất | %2")
            .arg(elapsedMs).arg(methodStr);
    }

    const auto formatSignedDelta = [](double delta, int precision) -> QString {
        const double zeroThreshold = 0.5 * std::pow(10.0, -precision);
        const double clean = (std::abs(delta) < zeroThreshold) ? 0.0 : delta;
        if (clean < 0.0) {
            return QString::number(clean, 'f', precision);
        }
        return QString("+%1").arg(clean, 0, 'f', precision);
    };

    const QString headerSuffix = showingDiffMap ? QString(
              " | %1 · [Diff Inferno: 0..%2 Gray8; "
              "thang màu riêng từng ảnh]")
              .arg(methodStr)
              .arg(m.maxAbsDiff) : QString(" | %1").arg(methodStr);

    return QString("Lần chạy gốc: %1 ms%2\n"
            "(Gồm đọc ảnh, xử lý, thống kê và diff)\n"
            "Params: %3\n"
            "Mean: %4 (%5) | Std: %6 (%7) | Dải: %8..%9\n"
            "Entropy: %10 (%11) bit/px | Sobel/4: %12 (%13)\n"
            "MAE so với đầu vào: %14 | MaxΔ: %15 | Thay đổi: %16%")
        .arg(elapsedMs)
        .arg(headerSuffix, appliedParams)
        .arg(m.outputMean, 0, 'f', 1)
        .arg(formatSignedDelta(m.outputMean - m.inputMean, 1))
        .arg(m.outputStd, 0, 'f', 1)
        .arg(formatSignedDelta(m.outputStd - m.inputStd, 1))
        .arg(m.outputMin)
        .arg(m.outputMax)
        .arg(m.outputEntropy, 0, 'f', 2)
        .arg(formatSignedDelta(m.outputEntropy - m.inputEntropy, 2))
        .arg(m.outputEdgeGrad, 0, 'f', 1)
        .arg(formatSignedDelta(m.outputEdgeGrad - m.inputEdgeGrad, 1))
        .arg(m.mae, 0, 'f', 1)
        .arg(m.maxAbsDiff)
        .arg(m.changedPixelPercent, 0, 'f', 1);
}

void MainWindow::updateEnhancedResultDisplay(
    const EnhanceJobRequest &request,
    const QImage             &resultImage,
    qint64                    elapsedMs,
    const QString            &methodStr)
{
    lastMethodStr_      = methodStr;
    lastElapsedMs_      = elapsedMs;
    lastAppliedParams_  = request.appliedParamsSummary;
    lastThermalMetrics_ = request.metrics;

    cachedN5EnhancedPixmap_ = QPixmap::fromImage(resultImage);
    cachedN5DiffPixmap_     = request.diffHeatmap.isNull()
                                  ? QPixmap()
                                  : QPixmap::fromImage(request.diffHeatmap);

    // Refresh left column with original-plane stats so left = original, right = enhanced.
    if (request.metrics.valid) {
        ThermalPlaneStats origStats;
        origStats.valid = true;
        origStats.mean = request.metrics.inputMean;
        origStats.stdDev = request.metrics.inputStd;
        origStats.entropy = request.metrics.inputEntropy;
        origStats.edgeGradient = request.metrics.inputEdgeGrad;
        origStats.minVal = request.metrics.inputMin;
        origStats.maxVal = request.metrics.inputMax;
        activeN5OriginalStats_ = origStats;
        if (!activeN5OriginalBaseInfo_.isEmpty() && ui->lblN5OriginalInfo) {
            ui->lblN5OriginalInfo->setText(
                formatOriginalMetricsSummary(activeN5OriginalBaseInfo_, activeN5OriginalStats_));
        }
    }

    // Enable diff-map checkbox only when a real diff exists
    const bool hasDiff = !cachedN5DiffPixmap_.isNull() && request.metrics.maxAbsDiff > 0;
    if (ui->chkShowDiffMap) {
        ui->chkShowDiffMap->setEnabled(hasDiff);
        if (!hasDiff) {
            QSignalBlocker b(ui->chkShowDiffMap);
            ui->chkShowDiffMap->setChecked(false);
        }
    }

    const bool showingDiff = hasDiff
                             && ui->chkShowDiffMap
                             && ui->chkShowDiffMap->isChecked();
    const QPixmap &displayPix = showingDiff ? cachedN5DiffPixmap_ : cachedN5EnhancedPixmap_;
    ui->viewerN5Enhanced->setPixmap(displayPix, false);
    ui->viewerN5Enhanced->fitToView();

    if (ui->chkSyncView && ui->chkSyncView->isChecked()) {
        ui->viewerN5Enhanced->setSyncViewport(
            ui->viewerN5Original->zoomFactor(),
            ui->viewerN5Original->panOffset());
    }

    ui->lblN5EnhancedInfo->setText(
        formatEnhancementMetricsSummary(methodStr, elapsedMs,
                                        request.appliedParamsSummary,
                                        request.metrics, showingDiff));
}

void MainWindow::storePairEnhancementResult(
    const EnhanceJobRequest &request,
    const QImage             &resultImage,
    qint64                    elapsedMs,
    const QString            &methodStr)
{
    if (request.pairId.isEmpty() || resultImage.isNull()) {
        return;
    }

    const std::string key = request.pairId.toStdString();
    auto &slot = pairEnhancementCache_[key];
    cachedEnhancementPixelBytes_ -= slot.imageBytes();

    slot.mode = request.mode;
    slot.methodStr = methodStr;
    slot.elapsedMs = elapsedMs;
    slot.appliedParamsSummary = request.appliedParamsSummary;
    slot.metrics = request.metrics;
    slot.resultImage = resultImage;
    slot.diffHeatmap = request.diffHeatmap;
    slot.lastUsedSeq = ++cacheAccessSeq_;

    cachedEnhancementPixelBytes_ += slot.imageBytes();
    trimPairEnhancementPixelCache(key);
}

void MainWindow::trimPairEnhancementPixelCache(const std::string &protectedPairId)
{
    while (cachedEnhancementPixelBytes_ > kMaxEnhancementCacheBytes) {
        auto oldestIt = pairEnhancementCache_.end();
        quint64 minSeq = std::numeric_limits<quint64>::max();

        for (auto it = pairEnhancementCache_.begin(); it != pairEnhancementCache_.end(); ++it) {
            if (it->first == protectedPairId || it->second.imageBytes() <= 0) {
                continue;
            }
            if (it->second.lastUsedSeq < minSeq) {
                minSeq = it->second.lastUsedSeq;
                oldestIt = it;
            }
        }

        if (oldestIt == pairEnhancementCache_.end()) {
            break;
        }

        // Keep lightweight metadata for rehydration off GUI thread; evict large pixel buffers.
        cachedEnhancementPixelBytes_ -= oldestIt->second.imageBytes();
        oldestIt->second.resultImage = QImage();
        oldestIt->second.diffHeatmap = QImage();
    }
}

void MainWindow::clearAllPairEnhancementCache()
{
    pairEnhancementCache_.clear();
    cachedEnhancementPixelBytes_ = 0;
    cacheAccessSeq_ = 0;
}

void MainWindow::removePairEnhancementCache(const QString &pairId)
{
    if (pairId.isEmpty()) {
        return;
    }
    const auto it = pairEnhancementCache_.find(pairId.toStdString());
    if (it == pairEnhancementCache_.end()) {
        return;
    }
    cachedEnhancementPixelBytes_ -= it->second.imageBytes();
    pairEnhancementCache_.erase(it);
}

void MainWindow::refreshPairListDisplayById(const QString &pairId, int hintIndex)
{
    const std::string targetId = pairId.toStdString();
    if (hintIndex >= 0
        && hintIndex < static_cast<int>(displayedItems_.size())
        && displayedItems_[static_cast<std::size_t>(hintIndex)].pair.pairId() == targetId) {
        if (auto *item = ui->lstImagePairs->item(hintIndex)) {
            updateListItemDisplay(item, displayedItems_[static_cast<std::size_t>(hintIndex)]);
            return;
        }
    }

    const int count = ui->lstImagePairs->count();
    for (int i = 0; i < count; ++i) {
        auto *item = ui->lstImagePairs->item(i);
        if (!item) continue;
        const int entryIdx = item->data(Qt::UserRole).toInt();
        if (entryIdx >= 0
            && entryIdx < static_cast<int>(displayedItems_.size())
            && displayedItems_[static_cast<std::size_t>(entryIdx)].pair.pairId() == targetId) {
            updateListItemDisplay(item, displayedItems_[static_cast<std::size_t>(entryIdx)]);
            break;
        }
    }
}

const CachedPairEnhancement *MainWindow::findPairEnhancement(const QString &pairId) const
{
    if (pairId.isEmpty()) {
        return nullptr;
    }
    const auto it = pairEnhancementCache_.find(pairId.toStdString());
    if (it == pairEnhancementCache_.end()) {
        return nullptr;
    }
    it->second.lastUsedSeq = ++cacheAccessSeq_;
    return &it->second;
}

void MainWindow::displayCachedPairEnhancement(const CachedPairEnhancement &cached)
{
    EnhanceJobRequest synthReq;
    synthReq.mode = cached.mode;
    synthReq.metrics = cached.metrics;
    synthReq.diffHeatmap = cached.diffHeatmap;
    synthReq.appliedParamsSummary = cached.appliedParamsSummary;
    updateEnhancedResultDisplay(synthReq, cached.resultImage, cached.elapsedMs, cached.methodStr);
}

void MainWindow::renderCachedN5Images()
{
    if (ui->viewerN5Original) {
        if (!cachedN5OriginalPixmap_.isNull()) {
            ui->viewerN5Original->setPixmap(cachedN5OriginalPixmap_, true);
        } else {
            ui->viewerN5Original->clearImage("Chưa có ảnh nhiệt");
        }
    }
    if (ui->viewerN5Enhanced) {
        const bool showDiff = ui->chkShowDiffMap
                              && ui->chkShowDiffMap->isChecked()
                              && !cachedN5DiffPixmap_.isNull();
        const QPixmap &pix = showDiff ? cachedN5DiffPixmap_ : cachedN5EnhancedPixmap_;
        if (!pix.isNull()) {
            ui->viewerN5Enhanced->setPixmap(pix, true);
        } else {
            ui->viewerN5Enhanced->clearImage("Bấm 'Áp dụng xử lý' để chạy N5 hoặc BYPASS");
        }
    }
}
