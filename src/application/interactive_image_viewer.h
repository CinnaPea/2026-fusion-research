#ifndef INTERACTIVE_IMAGE_VIEWER_H
#define INTERACTIVE_IMAGE_VIEWER_H

#include <QWidget>
#include <QPixmap>
#include <QPointF>
#include <QPoint>
#include <QString>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

class InteractiveImageViewer : public QWidget
{
    Q_OBJECT

public:
    explicit InteractiveImageViewer(QWidget *parent = nullptr);
    ~InteractiveImageViewer() override = default;

    void setPixmap(const QPixmap &pixmap);
    void clearImage(const QString &placeholder = QString());
    [[nodiscard]] const QPixmap& pixmap() const { return pixmap_; }
    [[nodiscard]] bool hasImage() const { return !pixmap_.isNull(); }

    [[nodiscard]] double zoomFactor() const { return zoomFactor_; }
    [[nodiscard]] QPointF panOffset() const { return panOffset_; }
    [[nodiscard]] bool isFitToView() const { return isFitToView_; }

signals:
    void viewportChanged(double zoomFactor, QPointF panOffset);

public slots:
    void fitToView();
    void zoom100();
    void zoomIn();
    void zoomOut();
    void resetView();
    void setSyncViewport(double zoomFactor, QPointF panOffset);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateZoomLabel();
    void calculateFitParameters(double &outScale, QPointF &outOffset) const;

    QPixmap pixmap_;
    QString placeholderText_;

    double zoomFactor_ = 1.0;
    QPointF panOffset_ = {0, 0};
    bool isFitToView_ = true;

    bool isDragging_ = false;
    QPoint lastMousePos_;
    bool isSyncUpdating_ = false;

    // Overlay toolbar widgets
    QWidget *toolbarWidget_ = nullptr;
    QPushButton *btnFit_ = nullptr;
    QPushButton *btn100_ = nullptr;
    QPushButton *btnZoomIn_ = nullptr;
    QPushButton *btnZoomOut_ = nullptr;
    QPushButton *btnReset_ = nullptr;
    QLabel *lblZoomPercent_ = nullptr;
};

#endif // INTERACTIVE_IMAGE_VIEWER_H
