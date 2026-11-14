// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QHash>
#include <QJsonObject>
#include <QVector>
#include <QWidget>

class JsonStore;
class Updater;
class Segmented;
class Stepper;
class Switch;
class IconButton;
class TextButton;
class RowList;
class QTextBrowser;

// Settings: General, Playback, Privacy, Shortcuts, Info, About, Legal. Every control applies live and is remembered.
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    SettingsPage(JsonStore* set, const QHash<QString, QString>& keys, Updater* updater, const QString& diagnostics, QWidget* parent = nullptr);
    // What has to be written to keymap.json: only keys that differ from the defaults. A default key you removed is saved as an empty entry.
    static QJsonObject keymapOverrides(const QHash<QString, QString>& current, const QHash<QString, QString>& defaults);
signals:
    void keymapChanged(const QHash<QString, QString>& keys);   // after an edit on the Shortcuts page: apply it right away
    void changed(const QString& key);
    void privacyAction(const QString& id);   // clearRecent | clearResume | clearMarks | clearThumbs | clearMusic | clearLogs | cleanIdentity | forgetFolders | openData
    void makeDefault();
    void closeRequested();
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    enum Page { General, Playback, Privacy, Shortcuts, Info, About, Legal, PageCount };
    struct Row { QString key, title, desc; QWidget* w = nullptr; };
    void addSwitch(Page p, const QString& key, bool def, const QString& title, const QString& desc);
    void addSeg(Page p, const QString& key, const QStringList& labels, const QVector<double>& values, double def, const QString& title, const QString& desc);
    void buildShortcuts();
    void rebuildKeyRows();                     // the list on the Shortcuts page, from m_keys
    void startCapture(const QString& action);
    void saveKeymap();
    void buildInfo();
    void buildAbout();
    void buildPrivacy();
    void refreshPrivacy();                     // sizes and counts shown on the Privacy page
    void refreshUpdate();
    void showPage(int p);
    QVector<QRectF> swatchRects() const;

    JsonStore* m_set;
    Updater* m_up;
    QHash<QString, QString> m_keys;
    QString m_diag;
    Segmented* m_tabs;
    IconButton* m_close;
    QVector<Row> m_rows[3];                    // General, Playback, Privacy
    QVector<QWidget*> m_pageWidgets[PageCount];
    int m_page = 0;
    // shortcuts / info / about / legal
    RowList* m_keysList = nullptr;
    TextButton* m_openKeymap = nullptr;
    TextButton* m_resetKeys = nullptr;
    QString m_capture;                         // action waiting for a key press ("" = not capturing)
    QString m_keyNote;                         // e.g. "Moved from Mute"
    bool m_resetArmed = false;
    Stepper* m_skipStepper = nullptr;          // custom skip seconds, shown when "Custom" is chosen
    Segmented* m_skipSeg = nullptr;
    QTextBrowser* m_infoView = nullptr;
    TextButton* m_copyDiag = nullptr;
    TextButton* m_updBtn = nullptr;
    TextButton* m_dlDirect = nullptr;
    Segmented* m_updMode = nullptr;
    QTextBrowser* m_legal = nullptr;
    QString m_updStatus;
    QStringList m_stored;                      // "what kinoz stores" lines on the Privacy page
};
