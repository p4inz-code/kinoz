// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "library/FolderScan.h"
#include "core/JsonStore.h"
#include <QCollator>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocale>
#include <QRandomGenerator>
#include <algorithm>

namespace FolderScan {

bool Options::allows(MediaKind k) const {
    switch (k) {
    case MediaKind::Video: return videos;
    case MediaKind::Gif: return gifs;
    case MediaKind::Audio: return music;
    case MediaKind::Image: return pictures;
    default: return false;
    }
}

QJsonObject Options::toJson() const {
    QJsonObject o;
    o["sub"] = subfolders; o["v"] = videos; o["g"] = gifs; o["m"] = music; o["p"] = pictures; o["s"] = shuffle;
    return o;
}

Options Options::fromJson(const QJsonObject& o) {
    Options r;
    if (o.isEmpty()) return r;
    r.subfolders = o.value("sub").toBool(true); r.videos = o.value("v").toBool(true); r.gifs = o.value("g").toBool(true);
    r.music = o.value("m").toBool(true); r.pictures = o.value("p").toBool(false); r.shuffle = o.value("s").toBool(false);
    return r;
}

namespace {
QCollator& collator() { static QCollator c = [] { QCollator x; x.setNumericMode(true); x.setCaseSensitivity(Qt::CaseInsensitive); return x; }(); return c; }

// Folders that are never worth walking. Windows and Program Files only count when they sit directly on a drive.
bool skipDir(const QFileInfo& fi) {
    const QString n = fi.fileName().toLower();
    if (n.startsWith(QLatin1Char('$')) || n == QLatin1String("system volume information") || n == QLatin1String("node_modules") || n == QLatin1String(".git")) return true;
    if (n == QLatin1String("windows") || n.startsWith(QLatin1String("program files"))) return QDir(fi.absolutePath()).isRoot();
    return false;
}

struct Walker {
    const Options& opts;
    const std::atomic<bool>* cancel;
    const std::function<void(int)>& progress;
    int maxFiles, maxMs;
    QElapsedTimer clock;
    Result res;
    int lastReport = 0;

    bool stop() {
        if (cancel && cancel->load()) { res.cancelled = true; return true; }
        if (clock.elapsed() > maxMs) { res.timedOut = true; return true; }
        if (res.files.size() >= maxFiles) { res.truncated = true; return true; }
        return false;
    }

    void walk(const QString& dir, int depth) {
        if (stop() || depth > 64) return;
        QDir d(dir);
        auto list = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::NoSort);   // hidden and system entries are left out by default
        std::sort(list.begin(), list.end(), [](const QFileInfo& a, const QFileInfo& b) { return collator().compare(a.fileName(), b.fileName()) < 0; });
        for (const QFileInfo& fi : list) {                                  // this folder's own files first ...
            if (fi.isDir()) continue;
            if (!opts.allows(Media::kindFor(fi.fileName()))) continue;
            res.files << fi.absoluteFilePath();
            if (res.files.size() - lastReport >= 250) { lastReport = int(res.files.size()); if (progress) progress(lastReport); }
            if (res.files.size() >= maxFiles) { res.truncated = true; return; }
        }
        if (!opts.subfolders) return;
        for (const QFileInfo& fi : list) {                                  // ... then each subfolder, depth first
            if (!fi.isDir() || skipDir(fi)) continue;
            walk(fi.absoluteFilePath(), depth + 1);
            if (res.cancelled || res.timedOut || res.truncated) return;
        }
    }
};
}

Result scan(const QStringList& roots, const Options& opts, const std::atomic<bool>* cancel, const std::function<void(int)>& progress, int maxFiles, int maxMs) {
    Walker w{opts, cancel, progress, maxFiles, maxMs, {}, {}, 0};
    w.clock.start();
    for (const QString& r : roots) {
        const QFileInfo fi(r);
        if (fi.isFile()) { if (opts.allows(Media::kindFor(fi.fileName()))) w.res.files << fi.absoluteFilePath(); continue; }
        w.walk(fi.absoluteFilePath(), 0);
        if (w.res.cancelled || w.res.timedOut || w.res.truncated) break;
    }
    if (progress) progress(int(w.res.files.size()));
    return w.res;
}

Counts count(const QString& dir, int maxEntries, int maxMs, const std::atomic<bool>* cancel) {
    Counts c;
    QElapsedTimer clock; clock.start();
    QStringList stack{QFileInfo(dir).absoluteFilePath()};
    int seen = 0, depth0 = 0;
    QStringList level;                                           // direct subfolders are counted once, for the first directory only
    while (!stack.isEmpty()) {
        if ((cancel && cancel->load()) || clock.elapsed() > maxMs || seen >= maxEntries) { c.partial = true; break; }
        const QString cur = stack.takeLast();
        const auto list = QDir(cur).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::NoSort);
        for (const QFileInfo& fi : list) {
            if (fi.isDir()) {
                if (skipDir(fi)) continue;
                if (depth0 == 0) ++c.folders;
                stack << fi.absoluteFilePath();
                continue;
            }
            ++seen;
            switch (Media::kindFor(fi.fileName())) {
            case MediaKind::Video: ++c.videos; break;
            case MediaKind::Gif: ++c.gifs; break;
            case MediaKind::Image: ++c.pictures; break;
            case MediaKind::Audio: ++c.music; break;
            default: break;
            }
        }
        depth0 = 1;
    }
    return c;
}

QString Counts::text() const {
    auto part = [](int n, const char* one, const char* many) { return n == 1 ? QObject::tr(one).arg(1) : QObject::tr(many).arg(QLocale().toString(n)); };
    QStringList p;
    const QString plus = partial ? QStringLiteral("+") : QString();
    if (videos) p << part(videos, "%1 video", "%1 videos") + plus;
    if (gifs) p << part(gifs, "%1 GIF", "%1 GIFs") + plus;
    if (music) p << part(music, "%1 song", "%1 songs") + plus;
    if (pictures) p << part(pictures, "%1 picture", "%1 pictures") + plus;
    if (p.isEmpty()) p << (partial ? QObject::tr("Looking…") : QObject::tr("No media"));
    if (folders) p << part(folders, "%1 subfolder", "%1 subfolders");
    return p.join(QStringLiteral(" · "));
}

void shuffle(QStringList& list, int keepFirst) {
    for (int i = int(list.size()) - 1; i > keepFirst; --i) {
        const int j = keepFirst + int(QRandomGenerator::global()->bounded(i - keepFirst + 1));
        if (i != j) list.swapItemsAt(i, j);
    }
}

Options optionsFor(JsonStore* settings, const QString& dir) {
    return Options::fromJson(settings->get(QStringLiteral("folderOptions")).toObject().value(QDir::cleanPath(dir).toLower()).toObject());
}

void rememberOptions(JsonStore* settings, const QString& dir, const Options& o) {
    QJsonObject all = settings->get(QStringLiteral("folderOptions")).toObject();
    all.insert(QDir::cleanPath(dir).toLower(), o.toJson());
    while (all.size() > 200) all.remove(all.keys().first());                       // bounded
    settings->set(QStringLiteral("folderOptions"), all);
}

}
