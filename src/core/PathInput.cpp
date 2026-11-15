// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/PathInput.h"
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QUrl>

namespace PathInput {

static QString stripQuotes(QString s) {
    s = s.trimmed();
    static const QString open = QStringLiteral("\"'\u201C\u2018<"), close = QStringLiteral("\"'\u201D\u2019>");
    while (s.size() >= 2) {
        const int i = open.indexOf(s.front());
        if (i < 0 || s.back() != close.at(i)) break;
        s = s.mid(1, s.size() - 2).trimmed();
    }
    // a lone leading or trailing quote left over from a sloppy copy
    while (!s.isEmpty() && QStringLiteral("\"'\u201C\u201D\u2018\u2019").contains(s.front())) s.remove(0, 1);
    while (!s.isEmpty() && QStringLiteral("\"'\u201C\u201D\u2018\u2019").contains(s.back())) s.chop(1);
    return s.trimmed();
}

QString normalize(const QString& raw) {
    QString s = stripQuotes(raw);
    if (s.isEmpty()) return {};
    if (s.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {                    // file:///C:/Videos or file://server/share
        const QUrl u(s);
        s = u.isLocalFile() ? u.toLocalFile() : QStringLiteral("//") + u.host() + u.path();
        if (s.isEmpty()) return {};
    }
#ifdef Q_OS_WIN
    static const QRegularExpression env(QStringLiteral("%([A-Za-z0-9_()]+)%"));        // %USERPROFILE%\Videos
    QString out; int last = 0;
    for (auto it = env.globalMatch(s); it.hasNext();) {
        const auto m = it.next();
        const QByteArray name = m.captured(1).toLocal8Bit();
        out += s.mid(last, m.capturedStart() - last);
        out += qEnvironmentVariableIsSet(name.constData()) ? qEnvironmentVariable(name.constData()) : m.captured(0);
        last = int(m.capturedEnd());
    }
    s = out + s.mid(last);
#endif
    if (s == QLatin1String("~") || s.startsWith(QLatin1String("~/")) || s.startsWith(QLatin1String("~\\"))) s = QDir::homePath() + s.mid(1);
    s = QDir::fromNativeSeparators(s);
    if (s.size() == 2 && s.at(1) == QLatin1Char(':') && s.at(0).isLetter()) s += QLatin1Char('/');      // "D:" means the drive
    if (!QFileInfo(s).isAbsolute()) return {};                                           // a bare word is not a path (the picker treats it as a filter)
    s = QDir::cleanPath(s);
    return s;
}

bool looksLikePath(const QString& text) {
    const QString t = stripQuotes(text);
    if (t.isEmpty() || t.contains(QLatin1Char('\n'))) return !normalize(t.section(QLatin1Char('\n'), 0, 0)).isEmpty();
    static const QRegularExpression re(QStringLiteral("^([A-Za-z]:[\\\\/]?|\\\\\\\\|//|/|~|%[A-Za-z]|file:)"));
    return re.match(t).hasMatch() && !normalize(t).isEmpty();
}

QVector<Item> parse(const QString& text) {
    QVector<Item> out;
    for (const QString& line : text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)) {
        if (line.trimmed().isEmpty() || line.trimmed().startsWith(QLatin1Char('#'))) continue;
        Item it;
        it.path = normalize(line);
        if (it.path.isEmpty()) it.error = QObject::tr("That doesn't look like a folder path. Type the full path, for example C:\\Videos\\Memes.");
        else {
            const QFileInfo fi(it.path);
            if (!fi.exists()) it.error = QObject::tr("That folder doesn't exist.");
            else if (fi.isFile()) it.isFile = true;
            else if (!fi.isReadable()) it.error = QObject::tr("kinoz isn't allowed to read that folder.");
        }
        out.push_back(it);
        if (out.size() >= 200) break;                                                     // a paste is never this long
    }
    return out;
}

}
