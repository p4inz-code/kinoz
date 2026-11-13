// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
// Linux: the two D-Bus faces of a media player (MPRIS 2). Desktop shells (GNOME, KDE, ...) and media keys talk to these.
#pragma once
#include <QtGlobal>
#if defined(__linux__) && defined(KINOZ_HAS_DBUS)
#include "core/Smtc.h"
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QStringList>
#include <QVariantMap>

struct MprisShared {                     // what the adaptors read; Smtc keeps it current
    Smtc* owner = nullptr;
    QWidget* window = nullptr;
    QString status = QStringLiteral("Stopped");
    QVariantMap metadata;
    qlonglong posUs = 0;
    bool hasMedia = false;
};

class MprisRoot : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ schemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ mimes)
public:
    MprisRoot(MprisShared* s, QObject* parent) : QDBusAbstractAdaptor(parent), m_s(s) {}
    bool canQuit() const { return false; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("kinoz"); }
    QString desktopEntry() const { return QStringLiteral("kinoz"); }
    QStringList schemes() const { return {QStringLiteral("file")}; }
    QStringList mimes() const { return {}; }
public slots:
    void Raise();
    void Quit() {}
private:
    MprisShared* m_s;
};

class MprisPlayer : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ status)
    Q_PROPERTY(double Rate READ one WRITE ignore)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ one WRITE ignore)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ one)
    Q_PROPERTY(double MaximumRate READ one)
    Q_PROPERTY(bool CanGoNext READ yes)
    Q_PROPERTY(bool CanGoPrevious READ yes)
    Q_PROPERTY(bool CanPlay READ yes)
    Q_PROPERTY(bool CanPause READ yes)
    Q_PROPERTY(bool CanSeek READ yes)
    Q_PROPERTY(bool CanControl READ yes)
public:
    MprisPlayer(MprisShared* s, QObject* parent) : QDBusAbstractAdaptor(parent), m_s(s) {}
    QString status() const { return m_s->status; }
    QVariantMap metadata() const { return m_s->metadata; }
    qlonglong position() const { return m_s->posUs; }
    double one() const { return 1.0; }
    void ignore(double) {}
    bool yes() const { return true; }
public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offsetUs);
    void SetPosition(const QDBusObjectPath&, qlonglong posUs);
    void OpenUri(const QString&) {}
private:
    MprisShared* m_s;
};
#endif
