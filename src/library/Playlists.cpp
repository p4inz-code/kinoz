// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "library/Playlists.h"
#include "core/Paths.h"
#include "media/MediaKind.h"
#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <algorithm>

namespace Playlists {

QString dir() {
    const QString d = Paths::dataDir() + QStringLiteral("/playlists");
    QDir().mkpath(d);
    return d;
}

QString sanitize(const QString& name) {
    QString s = name.trimmed();
    for (QChar& c : s) if (QStringLiteral("\\/:*?\"<>|").contains(c) || c.unicode() < 32) c = QLatin1Char('_');
    while (s.endsWith(QLatin1Char('.'))) s.chop(1);
    s = s.left(80).trimmed();
    static const QStringList reserved{"con", "prn", "aux", "nul", "com1", "com2", "com3", "lpt1", "lpt2", "lpt3"};   // Windows device names
    if (s.isEmpty() || reserved.contains(s.toLower())) s = QStringLiteral("Playlist");
    return s;
}

QString fileFor(const QString& name) { return dir() + QLatin1Char('/') + sanitize(name) + QStringLiteral(".m3u8"); }
bool exists(const QString& name) { return QFileInfo::exists(fileFor(name)); }

QStringList names() {
    QStringList out;
    for (const QFileInfo& fi : QDir(dir()).entryInfoList({QStringLiteral("*.m3u8")}, QDir::Files)) out << fi.completeBaseName();
    QCollator col; col.setNumericMode(true); col.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(out.begin(), out.end(), [&](const QString& a, const QString& b) { return col.compare(a, b) < 0; });
    return out;
}

QString uniqueName(const QString& base) {
    const QString b = sanitize(base);
    if (!exists(b)) return b;
    for (int i = 2; i < 10000; ++i) { const QString n = QStringLiteral("%1 %2").arg(b).arg(i); if (!exists(n)) return n; }
    return b;
}

bool save(const QString& name, const QStringList& paths, QString* error) {
    QSaveFile f(fileFor(name));                      // all-or-nothing: a crash mid-save never leaves half a playlist
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = f.errorString(); return false; }
    QTextStream t(&f);
    t.setEncoding(QStringConverter::Utf8);
    t << "#EXTM3U\n";
    for (const QString& p : paths) t << QDir::toNativeSeparators(p) << '\n';
    t.flush();
    if (!f.commit()) { if (error) *error = f.errorString(); return false; }
    return true;
}

bool remove(const QString& name) { return QFile::remove(fileFor(name)); }

bool rename(const QString& from, const QString& to, QString* error) {
    const QString dst = fileFor(to);
    if (QFileInfo::exists(dst) && dst.compare(fileFor(from), Qt::CaseInsensitive) != 0) { if (error) *error = QObject::tr("A playlist with that name already exists."); return false; }
    if (!QFile::rename(fileFor(from), dst)) { if (error) *error = QObject::tr("Couldn't rename the playlist."); return false; }
    return true;
}

bool isPlaylistFile(const QString& path) {
    const QString s = QFileInfo(path).suffix().toLower();
    return s == QLatin1String("m3u") || s == QLatin1String("m3u8") || s == QLatin1String("pls");
}

QStringList load(const QString& file) {
    QStringList out;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 20ll * 1024 * 1024) return out;           // a playlist is never 20 MB: refuse junk
    const QByteArray raw = f.readAll();
    // M3U8 is UTF-8; plain .m3u from old software is usually the system code page.
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter)) text = QString::fromLocal8Bit(raw);
    const QDir base = QFileInfo(file).absoluteDir();
    const bool pls = QFileInfo(file).suffix().compare(QLatin1String("pls"), Qt::CaseInsensitive) == 0;
    QSet<QString> seen;                                                                       // O(1) duplicate check (a 50000-line playlist stays instant)
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        if (pls) {
            if (!line.startsWith(QLatin1String("File"), Qt::CaseInsensitive)) continue;
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq < 0) continue;
            line = line.mid(eq + 1).trimmed();
        }
        if (line.startsWith(QStringLiteral("file:///"))) line = line.mid(8);
        // Only local files. Network paths (\\server\share, //server/share) are refused outright: merely checking that such a
        // file exists makes Windows sign in to that server, which can leak the user's login hash to whoever wrote the playlist.
        if (line.contains(QLatin1String("://")) || line.startsWith(QLatin1String("\\\\")) || line.startsWith(QLatin1String("//"))) continue;
        if (out.size() >= 50000) break;
        const QString abs = QFileInfo(line).isAbsolute() ? QDir::cleanPath(line) : QDir::cleanPath(base.absoluteFilePath(line));
        if (Media::isMedia(abs) && !seen.contains(abs) && QFileInfo::exists(abs)) { seen.insert(abs); out << abs; }
    }
    return out;
}

QStringList tracks(const QString& name) { return load(fileFor(name)); }

}  // namespace Playlists
