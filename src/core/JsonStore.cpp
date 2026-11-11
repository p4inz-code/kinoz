#include "core/JsonStore.h"
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

JsonStore::JsonStore(const QString& path, QObject* parent) : QObject(parent), m_path(path) {
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray raw = f.readAll();
        f.close();
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(raw, &pe);
        if (doc.isObject()) m_obj = doc.object();
        else if (!raw.trimmed().isEmpty()) QFile::copy(m_path, m_path + QStringLiteral(".corrupt"));   // keep the damaged file, start clean
    }
    m_timer.setSingleShot(true);
    m_timer.setInterval(600);
    connect(&m_timer, &QTimer::timeout, this, &JsonStore::flush);
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

void JsonStore::flush() {
    if (!m_dirty || m_readOnly) return;
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(m_obj).toJson(QJsonDocument::Compact));
    if (f.commit()) m_dirty = false;
}
