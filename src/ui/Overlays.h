// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include "ui/Icons.h"
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QLineEdit>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

class PlayerController;
class MpvPlayer;
class JsonStore;
class LibraryIndex;
class Slider;
class Switch;
class Segmented;
class IconButton;
class TextButton;

// ---------------- RowList: painted, keyboard/wheel scrollable list ----------------
class RowList : public QWidget {
    Q_OBJECT
public:
    struct Row { Icon icon = Icon::File; QString title, subtitle, hint, data; bool header = false; };
    explicit RowList(QWidget* parent = nullptr);
    void setRows(const QVector<Row>& r, bool keepScroll = false);
    int scrollPos() const { return m_scroll; }
    const QVector<Row>& rows() const { return m_rows; }
    int current() const { return m_cur; }
    void setCurrent(int i);
    void move(int dir);
    int rowHeight() const { return m_rowH; }
    void setRowHeight(int h) { m_rowH = h; update(); }
    void setShowIcons(bool s) { m_icons = s; update(); }
    void setEmptyText(const QString& t) { m_empty = t; update(); }
    int contentHeight() const;
signals:
    void activated(int row);
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override { m_hover = -1; update(); }
private:
    int rowAt(const QPoint& p) const;
    void ensureVisible(int i);
    QVector<Row> m_rows;
    int m_cur = -1, m_hover = -1, m_rowH = 46, m_scroll = 0;
    bool m_icons = true;
    QString m_empty;
};

// ---------------- Sheet: centred modal card over a scrim ----------------
class Sheet : public QWidget {
    Q_OBJECT
public:
    explicit Sheet(QWidget* parent);
    void setContent(QWidget* w, const QSize& size);
    QWidget* content() const { return m_content; }
    void openSheet();
    void closeSheet();
signals:
    void closed();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    QRectF cardRect() const;
    QWidget* m_content = nullptr;
    QSize m_size;
    qreal m_op = 0;
    QVariantAnimation m_anim;
    bool m_closing = false;
};

// ---------------- ConfirmBox: small yes/no card shown inside a Sheet (Enter = confirm, Esc = cancel) ----------------
class ConfirmBox : public QWidget {
    Q_OBJECT
public:
    ConfirmBox(const QString& title, const QString& body, const QString& okText, bool danger, QWidget* parent = nullptr, const QString& cancelText = {});
signals:
    void accepted();
    void cancelled();
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    QString m_title, m_body;
    TextButton* m_ok;
    TextButton* m_cancel;
};

// ---------------- InputBox: one-line text prompt inside a Sheet (Enter = OK, Esc = cancel) ----------------
class InputBox : public QWidget {
    Q_OBJECT
public:
    InputBox(const QString& title, const QString& label, const QString& text, const QString& okText, QWidget* parent = nullptr);
    void focusEdit();
signals:
    void accepted(const QString& text);
    void cancelled();
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    QString m_title, m_label;
    QLineEdit* m_edit;
    TextButton* m_ok;
    TextButton* m_cancel;
};

// ---------------- FolderPicker (our own, replaces the OS dialog) ----------------
class FolderPicker : public QWidget {
    Q_OBJECT
public:
    enum Mode { PickFolder, PickFile };
    FolderPicker(Mode m, const QString& startDir, QWidget* parent = nullptr);
signals:
    void chosen(const QString& path);
    void cancelled();
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    void go(const QString& dir);
    Mode m_mode;
    QString m_dir;
    RowList* m_list;
    IconButton* m_up;
    TextButton *m_ok, *m_cancel;
    QString m_err;
};

// ---------------- Command palette (Ctrl+K) ----------------
class CommandPalette : public QWidget {
    Q_OBJECT
public:
    CommandPalette(PlayerController* ctl, LibraryIndex* idx, QWidget* parent);
    void openPalette();
    void closePalette();
    bool isOpen() const { return isVisible(); }
signals:
    void command(const QString& id);
    void playFile(const QString& path);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
    void resizeEvent(QResizeEvent*) override { layoutCard(); }
private:
    void refresh();
    void layoutCard();
    void activate(int row);
    PlayerController* m_ctl;
    LibraryIndex* m_idx;
    QLineEdit* m_input;
    RowList* m_list;
    QRect m_card;
};

// ---------------- Side panel: Adjust / Equalizer ----------------
class SidePanel : public QWidget {
    Q_OBJECT
public:
    SidePanel(PlayerController* ctl, JsonStore* set, QWidget* parent);
    void showPage(const QString& page);       // "adjust" | "eq"
    QString page() const { return m_page; }
    void slideOut();
    bool isOpen() const { return m_open; }
    static constexpr int kWidth = 320;
signals:
    void closedByUser();
    void toast(const QString&);
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    void build();
    void layoutPage();
    void applyEqSoon();
    void loadEqUi();
    void showDevices();
    PlayerController* m_ctl;
    JsonStore* m_set;
    QString m_page;
    bool m_open = false;
    IconButton* m_close;
    QVector<Slider*> m_colorSliders;
    QVector<QWidget*> m_adjustWidgets, m_eqWidgets;
    QVector<Slider*> m_bands;
    Slider* m_pre = nullptr;
    Switch* m_eqOn = nullptr;
    Switch* m_norm = nullptr;
    TextButton *m_reset = nullptr, *m_preset = nullptr, *m_device = nullptr;
    QTimer* m_eqTimer;
    QVariantAnimation m_anim;
    qreal m_x = 1.0;
};

// ---------------- Seek preview bubble ----------------
class PreviewTip : public QWidget {
    Q_OBJECT
public:
    explicit PreviewTip(QWidget* parent);
    void showFrame(const QImage& img, const QString& timeText, const QPoint& anchorBottomCenter);
    void showTimeOnly(const QString& timeText, const QPoint& anchorBottomCenter);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QImage m_img;
    QString m_time;
};
