#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QVector>
#include <mpv/client.h>
#include <mpv/render.h>

class MpvPlayer;

// Renders mpv frames into a Qt-owned GL surface, then draws real "glass" (blurred backdrop + tint + liquid-metal
// sheen) for floating UI on top of the frame, so overlays composite cleanly.
class VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    struct Glass { QRectF rect; qreal radius = 16; qreal opacity = 1; QColor tint{14, 14, 18}; qreal tintStrength = 0.30; qreal metal = 1.0; };

    explicit VideoWidget(MpvPlayer* player, QWidget* parent = nullptr);
    ~VideoWidget() override;
    void setGlass(const QVector<Glass>& g);
    bool glassOk() const { return m_glassOk; }
    QString rendererInfo() const { return m_glInfo; }
    void setPointer(const QPointF& normalized) { m_pointer = normalized; }

signals:
    void clicked(const QPoint&);
    void doubleClicked();
    void rightClicked(const QPoint&);
    void wheel(int delta, const QPointF& pos, Qt::KeyboardModifiers m);
    void dragged(const QPoint& delta);
    void moved();
    void glassReady(bool ok);

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    void createContext();
    bool initGlass();
    void drawGlass();
    void ensureBlurTargets(int w, int h);
    GLuint compile(const char* vs, const char* fs);

    MpvPlayer* m_player;
    mpv_render_context* m_ctx = nullptr;
    bool m_glReady = false;
    QPoint m_last, m_press;
    bool m_down = false, m_dragging = false;

    // glass
    QVector<Glass> m_glass;
    bool m_glassOk = false;
    QString m_glInfo;
    GLuint m_vao = 0, m_blurProg = 0, m_compProg = 0;
    GLuint m_tex[2] = {0, 0}, m_fbo[2] = {0, 0};
    int m_texW = 0, m_texH = 0;
    QElapsedTimer m_clock;
    QPointF m_pointer{0.5, 0.5};
};
