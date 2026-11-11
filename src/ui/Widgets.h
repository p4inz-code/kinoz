#pragma once
#include "ui/Icons.h"
#include <QAbstractButton>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QWidget>
#include <functional>

// ---- IconButton: flat, custom-painted, animated hover ----
class IconButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit IconButton(Icon ic, QWidget* parent = nullptr, int size = 32);
    void setIcon(Icon ic) { m_icon = ic; update(); }
    void setOverlayStyle(bool o) { m_overlay = o; update(); }
    void setActive(bool a) { m_active = a; update(); }
    QSize sizeHint() const override { return {m_size, m_size}; }
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    Icon m_icon;
    int m_size;
    bool m_overlay = false, m_active = false;
    qreal m_hover = 0;
    QVariantAnimation m_anim;
};

// ---- TextButton: label (+ optional icon), primary or quiet ----
class TextButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit TextButton(const QString& text, QWidget* parent = nullptr, bool primary = false, bool hasIcon = false, Icon ic = Icon::Plus);
    QSize sizeHint() const override;
    void setDanger(bool d) { m_danger = d; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override { m_hot = true; update(); }
    void leaveEvent(QEvent*) override { m_hot = false; update(); }
private:
    bool m_primary, m_hasIcon, m_hot = false, m_danger = false;
    Icon m_icon;
};

// ---- Slider: thin track, accent fill, round knob (panels and dialogs) ----
class Slider : public QWidget {
    Q_OBJECT
public:
    explicit Slider(QWidget* parent = nullptr);
    void setRange(double lo, double hi) { m_min = lo; m_max = hi; update(); }
    void setValue(double v, bool emitSignal = false);
    void setDefault(double d) { m_def = d; }
    double value() const { return m_val; }
    QSize sizeHint() const override { return {160, 22}; }
signals:
    void valueChanged(double v);   // user-driven only
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void enterEvent(QEnterEvent*) override { m_hot = true; update(); }
    void leaveEvent(QEvent*) override { m_hot = false; update(); }
private:
    void setFromX(qreal x);
    double m_min = 0, m_max = 1, m_val = 0, m_def = 0;
    bool m_hot = false, m_down = false;
};

// ---- Switch ----
class Switch : public QAbstractButton {
    Q_OBJECT
public:
    explicit Switch(QWidget* parent = nullptr);
    QSize sizeHint() const override { return {38, 22}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QVariantAnimation m_anim;
};

// ---- Segmented control (chips): exclusive choice ----
class Segmented : public QWidget {
    Q_OBJECT
public:
    explicit Segmented(const QStringList& labels, QWidget* parent = nullptr);
    void setCurrent(int i) { m_cur = i; update(); }
    int current() const { return m_cur; }
    QSize sizeHint() const override;
signals:
    void currentChanged(int i);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
private:
    QStringList m_labels;
    int m_cur = 0;
};

// ---- Thin, themed scroll bar ----
class ScrollBar : public QScrollBar {
    Q_OBJECT
public:
    explicit ScrollBar(Qt::Orientation o, QWidget* parent = nullptr);
    QSize sizeHint() const override { return orientation() == Qt::Vertical ? QSize(10, 40) : QSize(40, 10); }
protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override { m_hot = true; update(); }
    void leaveEvent(QEvent*) override { m_hot = false; update(); }
private:
    bool m_hot = false;
};

// ---- Custom popup menu (replaces the OS menu everywhere) ----
class PopupMenu : public QWidget {
    Q_OBJECT
public:
    explicit PopupMenu(QWidget* parent = nullptr);
    void addItem(const QString& id, const QString& text, bool checked = false, const QString& hint = {}, bool enabled = true);
    void addSeparator();
    void addHeader(const QString& text);
    void popup(const QPoint& globalPos);
    void popupAbove(const QRect& globalAnchor);
signals:
    void triggered(const QString& id);
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override { m_hover = -1; update(); }
    void keyPressEvent(QKeyEvent*) override;
private:
    struct Item { QString id, text, hint; bool checked = false, enabled = true, sep = false, header = false; };
    int itemAt(const QPoint& p) const;
    int itemY(int i) const;
    int itemH(const Item& it) const;
    QVector<Item> m_items;
    int m_hover = -1;
    int m_w = 200;
    static constexpr int kPad = 14;   // shadow margin
};

// ---- FadeCover: full-window veil used for the open / close animation ----
class FadeCover : public QWidget {
    Q_OBJECT
public:
    explicit FadeCover(QWidget* parent);
    void run(qreal from, qreal to, int ms, std::function<void()> done = {});
    qreal level() const { return m_op; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    qreal m_op = 1.0;
    QVariantAnimation m_anim;
    std::function<void()> m_done;
};

// ---- Toast: transient one-line message ----
class Toast : public QWidget {
    Q_OBJECT
public:
    explicit Toast(QWidget* parent);
    void show(const QString& text, int ms = 2200, bool error = false);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    bool m_error = false;
    QString m_text;
    QTimer* m_timer;
    qreal m_op = 0;
    QVariantAnimation m_anim;
};
