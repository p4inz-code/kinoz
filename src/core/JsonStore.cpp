#include "core/JsonStore.h"
#include <QFile>
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
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(data);
    return f.commit();
}
}

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
