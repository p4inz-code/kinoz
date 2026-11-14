// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/JsonStore.h"
#include "core/Log.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QThreadPool>
#include <QtConcurrent>

namespace {
// One writer thread for every store: saves never run on the UI thread (a disk flush can take 200 ms on a busy
// disk or with antivirus) and stay in order.
QThreadPool& writer() {
    static QThreadPool* p = [] { auto* t = new QThreadPool; t->setMaxThreadCount(1); return t; }();
    return *p;
}
bool writeFile(const QString& path, const QByteArray& data) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { Log::warn("settings", QStringLiteral("can't write %1: %2").arg(QFileInfo(path).fileName(), f.errorString())); return false; }
    f.write(data);
    if (!f.commit()) { Log::warn("settings", QStringLiteral("saving %1 failed: %2").arg(QFileInfo(path).fileName(), f.errorString())); return false; }
    return true;
}
}

JsonStore::JsonStore(const QString& path, QObject* parent) : QObject(parent), m_path(path) {
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray raw = f.readAll();
        f.close();
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(raw, &pe);
        const QString name = QFileInfo(m_path).fileName();
        if (doc.isObject()) {
            m_obj = doc.object();
            if (!m_readOnly) { QFile::remove(m_path + QStringLiteral(".bak")); QFile::copy(m_path, m_path + QStringLiteral(".bak")); }   // the last good state, for recovery
        } else if (!raw.trimmed().isEmpty()) {
            QFile::remove(m_path + QStringLiteral(".corrupt"));
            QFile::copy(m_path, m_path + QStringLiteral(".corrupt"));                                // keep the damaged file for a look
            QFile bak(m_path + QStringLiteral(".bak"));
            const auto bdoc = bak.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(bak.readAll()) : QJsonDocument();
            if (bdoc.isObject()) {                                                                    // restore the copy made at the previous start
                m_obj = bdoc.object(); m_recovered = true; m_dirty = true;
                Log::warn("settings", QStringLiteral("%1 was damaged (%2); restored from its backup").arg(name, pe.errorString()));
            } else Log::warn("settings", QStringLiteral("%1 was damaged (%2) and there is no backup; starting clean").arg(name, pe.errorString()));
        }
    } else if (QFile::exists(m_path)) Log::warn("settings", QStringLiteral("%1 can't be read; starting clean").arg(QFileInfo(m_path).fileName()));
    m_timer.setSingleShot(true);
    m_timer.setInterval(600);
    connect(&m_timer, &QTimer::timeout, this, [this] { flushImpl(false); });
}

JsonStore::~JsonStore() { flush(); }

QJsonValue JsonStore::get(const QString& key, const QJsonValue& def) const {
    auto it = m_obj.constFind(key);
    return it == m_obj.constEnd() ? def : it.value();
}

void JsonStore::set(const QString& key, const QJsonValue& v) {
    if (m_obj.value(key) == v) return;
    m_obj.insert(key, v);
    m_dirty = true;
    m_timer.start();
}

void JsonStore::remove(const QString& key) {
    if (!m_obj.contains(key)) return;
    m_obj.remove(key);
    m_dirty = true;
    m_timer.start();
}

void JsonStore::flush() { flushImpl(true); }

void JsonStore::flushImpl(bool sync) {
    if (!m_dirty || m_readOnly) { if (sync) writer().waitForDone(); return; }
    const QByteArray data = QJsonDocument(m_obj).toJson(QJsonDocument::Compact);    // serialised now, written later
    m_dirty = false;
    const QString path = m_path;
    if (sync) {                                      // closing the app: wait for earlier saves, then write and be sure
        writer().waitForDone();
        if (!writeFile(path, data)) m_dirty = true;
        return;
    }
    QPointer<JsonStore> self(this);
    (void)QtConcurrent::run(&writer(), [self, path, data] {
        if (writeFile(path, data)) return;
        if (self) QMetaObject::invokeMethod(self.data(), [self] { if (self) { self->m_dirty = true; self->m_timer.start(3000); } }, Qt::QueuedConnection);   // try again shortly
    });
}
