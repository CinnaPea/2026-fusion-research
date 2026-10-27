#include "interactive_image_viewer.h"

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <algorithm>
#include <cmath>

InteractiveImageViewer::InteractiveImageViewer(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(180, 160);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Overlay toolbar at bottom
    toolbarWidget_ = new QWidget(this);
    toolbarWidget_->setObjectName("viewerToolbar");
    toolbarWidget_->setStyleSheet(
        "#viewerToolbar { background-color: rgba(26, 26, 26, 220); border: 1px solid #444444; border-radius: 4px; padding: 2px; }"
        "QPushButton { background-color: #2b2b2b; color: #d0d0d0; border: 1px solid #555555; border-radius: 3px; padding: 2px 6px; font-size: 11px; font-weight: bold; min-width: 24px; min-height: 20px; }"
        "QPushButton:hover { background-color: #3d3d3d; color: #ffffff; border-color: #777777; }"
        "QPushButton:pressed { background-color: #1e1e1e; }"
        "QLabel { color: #a0a0a0; font-size: 11px; padding: 0 4px; }"
    );

    auto *toolbarLayout = new QHBoxLayout(toolbarWidget_);
    toolbarLayout->setContentsMargins(4, 2, 4, 2);
    toolbarLayout->setSpacing(4);

    btnFit_ = new QPushButton(tr("Vừa"), toolbarWidget_);
    btnFit_->setToolTip(tr("Vừa khung (Fit to view)"));
    connect(btnFit_, &QPushButton::clicked, this, &InteractiveImageViewer::fitToView);
    toolbarLayout->addWidget(btnFit_);

    btn100_ = new QPushButton(tr("100%"), toolbarWidget_);
    btn100_->setToolTip(tr("Xem kích thước thực (1:1 pixel)"));
    connect(btn100_, &QPushButton::clicked, this, &InteractiveImageViewer::zoom100);
    toolbarLayout->addWidget(btn100_);

    btnZoomIn_ = new QPushButton(tr("+"), toolbarWidget_);
    btnZoomIn_->setToolTip(tr("Phóng to (+15%)"));
    connect(btnZoomIn_, &QPushButton::clicked, this, &InteractiveImageViewer::zoomIn);
    toolbarLayout->addWidget(btnZoomIn_);

    btnZoomOut_ = new QPushButton(tr("−"), toolbarWidget_);
    btnZoomOut_->setToolTip(tr("Thu nhỏ (−15%)"));
    connect(btnZoomOut_, &QPushButton::clicked, this, &InteractiveImageViewer::zoomOut);
    toolbarLayout->addWidget(btnZoomOut_);

    btnReset_ = new QPushButton(tr("↺"), toolbarWidget_);
    btnReset_->setToolTip(tr("Khôi phục góc nhìn"));
    connect(btnReset_, &QPushButton::clicked, this, &InteractiveImageViewer::resetView);
    toolbarLayout->addWidget(btnReset_);

    lblZoomPercent_ = new QLabel(tr("Vừa khung"), toolbarWidget_);
    toolbarLayout->addWidget(lblZoomPercent_);

    toolbarWidget_->adjustSize();
    placeholderText_ = tr("Chưa có ảnh");
}

void InteractiveImageViewer::setPixmap(const QPixmap &pixmap)
{
    pixmap_ = pixmap;
    fitToView();
}

void InteractiveImageViewer::clearImage(const QString &placeholder)
{
    pixmap_ = QPixmap();
    placeholderText_ = placeholder.isEmpty() ? tr("Chưa có ảnh") : placeholder;
    isFitToView_ = true;
    zoomFactor_ = 1.0;
    panOffset_ = {0, 0};
    updateZoomLabel();
    update();
}

void InteractiveImageViewer::calculateFitParameters(double &outScale, QPointF &outOffset) const
{
    if (pixmap_.isNull() || width() <= 10 || height() <= 40) {
        outScale = 1.0;
        outOffset = {0, 0};
        return;
    }

    const double availW = std::max(10.0, static_cast<double>(width()) - 8.0);
    const double availH = std::max(10.0, static_cast<double>(height()) - 42.0);

    const double scaleX = availW / pixmap_.width();
    const double scaleY = availH / pixmap_.height();
    outScale = std::min(scaleX, scaleY);

    const double drawnW = pixmap_.width() * outScale;
    const double drawnH = pixmap_.height() * outScale;
    const double posX = (static_cast<double>(width()) - drawnW) / 2.0;
    const double posY = (availH - drawnH) / 2.0 + 4.0;
    outOffset = QPointF(posX, posY);
}

void InteractiveImageViewer::fitToView()
{
    isFitToView_ = true;
    double scale = 1.0;
    QPointF offset(0, 0);
    calculateFitParameters(scale, offset);
    zoomFactor_ = scale;
    panOffset_ = offset;

    updateZoomLabel();
    update();

    if (!isSyncUpdating_) {
        isSyncUpdating_ = true;
        emit viewportChanged(zoomFactor_, panOffset_);
        isSyncUpdating_ = false;
    }
}

void InteractiveImageViewer::zoom100()
{
    if (pixmap_.isNull()) return;

    isFitToView_ = false;
    zoomFactor_ = 1.0;
    const double availH = std::max(10.0, static_cast<double>(height()) - 42.0);
    panOffset_ = QPointF((static_cast<double>(width()) - pixmap_.width()) / 2.0,
                         (availH - pixmap_.height()) / 2.0 + 4.0);

    updateZoomLabel();
    update();

    if (!isSyncUpdating_) {
        isSyncUpdating_ = true;
        emit viewportChanged(zoomFactor_, panOffset_);
        isSyncUpdating_ = false;
    }
}

void InteractiveImageViewer::zoomIn()
{
    if (pixmap_.isNull()) return;

    const QPointF center(width() / 2.0, (height() - 36.0) / 2.0);
    const double newZoom = std::min(zoomFactor_ * 1.2, 20.0);
    const double factor = newZoom / zoomFactor_;

    panOffset_ = center - factor * (center - panOffset_);
    zoomFactor_ = newZoom;
    isFitToView_ = false;

    updateZoomLabel();
    update();

    if (!isSyncUpdating_) {
        isSyncUpdating_ = true;
        emit viewportChanged(zoomFactor_, panOffset_);
        isSyncUpdating_ = false;
    }
}

void InteractiveImageViewer::zoomOut()
{
    if (pixmap_.isNull()) return;

    const QPointF center(width() / 2.0, (height() - 36.0) / 2.0);
    const double newZoom = std::max(zoomFactor_ / 1.2, 0.05);
    const double factor = newZoom / zoomFactor_;

    panOffset_ = center - factor * (center - panOffset_);
    zoomFactor_ = newZoom;
    isFitToView_ = false;

    updateZoomLabel();
    update();

    if (!isSyncUpdating_) {
        isSyncUpdating_ = true;
        emit viewportChanged(zoomFactor_, panOffset_);
        isSyncUpdating_ = false;
    }
}

void InteractiveImageViewer::resetView()
{
    fitToView();
}

void InteractiveImageViewer::setSyncViewport(double zoomFactor, QPointF panOffset)
{
    if (isSyncUpdating_) return;

    isSyncUpdating_ = true;
    isFitToView_ = false;
    zoomFactor_ = zoomFactor;
    panOffset_ = panOffset;

    updateZoomLabel();
    update();
    isSyncUpdating_ = false;
}

void InteractiveImageViewer::updateZoomLabel()
{
    if (isFitToView_) {
        lblZoomPercent_->setText(tr("Vừa khung"));
    } else {
        const int pct = static_cast<int>(std::round(zoomFactor_ * 100.0));
        lblZoomPercent_->setText(QString("%1%").arg(pct));
    }
}

void InteractiveImageViewer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Dark sleek background
    painter.fillRect(rect(), QColor(24, 24, 24));
    painter.setPen(QPen(QColor(50, 50, 50), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    if (pixmap_.isNull()) {
        painter.setPen(QColor(140, 140, 140));
        QFont f = painter.font();
        f.setPointSize(11);
        painter.setFont(f);
        painter.drawText(rect().adjusted(10, 10, -10, -40), Qt::AlignCenter | Qt::TextWordWrap, placeholderText_);
        return;
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    double currentScale = zoomFactor_;
    QPointF currentOffset = panOffset_;

    if (isFitToView_) {
        calculateFitParameters(currentScale, currentOffset);
        zoomFactor_ = currentScale;
        panOffset_ = currentOffset;
    }

    const double drawnW = pixmap_.width() * currentScale;
    const double drawnH = pixmap_.height() * currentScale;

    QRectF targetRect(currentOffset.x(), currentOffset.y(), drawnW, drawnH);
    painter.drawPixmap(targetRect, pixmap_, QRectF(pixmap_.rect()));
}

void InteractiveImageViewer::wheelEvent(QWheelEvent *event)
{
    if (pixmap_.isNull()) {
        QWidget::wheelEvent(event);
        return;
    }

    const QPoint numDegrees = event->angleDelta();
    if (numDegrees.isNull()) return;

    const double numSteps = numDegrees.y() / 120.0;
    const double zoomMultiplier = std::pow(1.15, numSteps);
    const double newZoom = std::clamp(zoomFactor_ * zoomMultiplier, 0.05, 20.0);

    const QPointF mousePos = event->position();
    const double factor = newZoom / zoomFactor_;

    panOffset_ = mousePos - factor * (mousePos - panOffset_);
    zoomFactor_ = newZoom;
    isFitToView_ = false;

    updateZoomLabel();
    update();

    if (!isSyncUpdating_) {
        isSyncUpdating_ = true;
        emit viewportChanged(zoomFactor_, panOffset_);
        isSyncUpdating_ = false;
    }

    event->accept();
}

void InteractiveImageViewer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        isDragging_ = true;
        lastMousePos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void InteractiveImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging_ && !pixmap_.isNull()) {
        const QPoint delta = event->pos() - lastMousePos_;
        lastMousePos_ = event->pos();

        panOffset_ += QPointF(delta.x(), delta.y());
        isFitToView_ = false;

        updateZoomLabel();
        update();

        if (!isSyncUpdating_) {
            isSyncUpdating_ = true;
            emit viewportChanged(zoomFactor_, panOffset_);
            isSyncUpdating_ = false;
        }

        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void InteractiveImageViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (isDragging_ && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        isDragging_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void InteractiveImageViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (isFitToView_) {
            zoom100();
        } else {
            fitToView();
        }
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void InteractiveImageViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (toolbarWidget_) {
        toolbarWidget_->adjustSize();
        // Position toolbar at bottom right
        const int tbW = toolbarWidget_->width();
        const int tbH = toolbarWidget_->height();
        toolbarWidget_->move(std::max(4, width() - tbW - 6),
                             std::max(4, height() - tbH - 6));
        toolbarWidget_->raise();
    }
    if (isFitToView_) {
        fitToView();
    }
}
