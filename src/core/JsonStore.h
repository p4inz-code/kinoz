// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QTimer>

// Small JSON key/value file with debounced writes (so closing never waits on disk).
class JsonStore : public QObject {
    Q_OBJECT
public:
    explicit JsonStore(const QString& path, QObject* parent = nullptr);
    ~JsonStore() override;

    QJsonValue get(const QString& key, const QJsonValue& def = {}) const;
    QString str(const QString& key, const QString& def = {}) const { return get(key, def).toString(); }
    double num(const QString& key, double def = 0) const { return get(key, def).toDouble(def); }
    bool flag(const QString& key, bool def = false) const { return get(key, def).toBool(def); }
    void set(const QString& key, const QJsonValue& v);
    void remove(const QString& key);
    void clear() { m_obj = QJsonObject(); m_dirty = true; m_timer.start(); }
    bool has(const QString& key) const { return m_obj.contains(key); }
    QJsonObject object() const { return m_obj; }
    void flush();
    void setReadOnly(bool r) { m_readOnly = r; }     // extra windows never overwrite the main window's settings
    bool readOnly() const { return m_readOnly; }

private:
    void flushImpl(bool sync);
    bool m_readOnly = false;
    QString m_path;
    QJsonObject m_obj;
    QTimer m_timer;
    bool m_dirty = false;
};
