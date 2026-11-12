// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/Theme.h"
#include "core/Paths.h"
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStyleHints>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

Theme& Theme::i() {
    static Theme t;
    return t;
}

Theme::Theme() {
    apply();
    // follow the OS light/dark switch live when in System mode
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_mode == Mode::System) apply();
    });
}

void Theme::setMode(Mode m) {
    m_mode = m;
    apply();
}

int Theme::accentCount() { return 6; }

QColor Theme::accentSample(int i, bool dark) {
    static const char* light[] = {"#0c6a6e", "#1f5fbf", "#6a4bb5", "#b4375f", "#9a5b00", "#1b7344"};
    static const char* darkc[] = {"#4fd1d6", "#7fb0ff", "#c0a8ff", "#ff8fb1", "#f2b24d", "#5ed39a"};
    i = qBound(0, i, 5);
    return QColor(dark ? darkc[i] : light[i]);
}

void Theme::setAccentIndex(int i) { m_accent = qBound(0, i, 5); apply(); }

void Theme::setMotionMode(int m) {
    m_motionMode = m;
    if (m == 1) m_reduceMotion = false;
    else if (m == 2) m_reduceMotion = true;
    else {
#ifdef Q_OS_WIN
        BOOL on = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0);
        m_reduceMotion = !on;
#else
        m_reduceMotion = false;
#endif
    }
    emit changed();
}

void Theme::setCompact(bool c) {
    m_compact = c;
    emit changed();
}

void Theme::apply() {
    bool dark = m_mode == Mode::Dark;
    if (m_mode == Mode::System) dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    Palette p;
    p.dark = dark;
    if (!dark) {
        p.bg = "#fdfcf9";       p.surface = "#f3f1ea";   p.sunken = "#e8e5db";   p.raised = "#ffffff";
        p.border = "#dbd7cb";   p.borderStrong = "#706e63";
        p.text = "#1b1a15";     p.muted = "#57544a";
        p.accent = "#0c6a6e";   p.onAccent = "#ffffff";  p.accentSubtle = "#dbeded";
        p.danger = "#b3261e";   p.scrim = QColor(27, 26, 21, 92);  p.shadow = QColor(27, 26, 21, 41);
    } else {
        p.bg = "#14140f";       p.surface = "#1c1c16";   p.sunken = "#0e0e0a";   p.raised = "#25251e";
        p.border = "#35352c";   p.borderStrong = "#8a897c";
        p.text = "#edebe2";     p.muted = "#aba99b";
        p.accent = "#4fd1d6";   p.onAccent = "#04282a";  p.accentSubtle = "#123134";
        p.danger = "#ff8a80";   p.scrim = QColor(0, 0, 0, 140);    p.shadow = QColor(0, 0, 0, 128);
    }
    {   // accent colour (user choice): derive the soft tint and the text-on-accent colour from it
        const QColor a = accentSample(m_accent, dark);
        if (m_accent != 0) {
            p.accent = a;
            p.accentSubtle = QColor::fromRgbF(a.redF() * 0.16 + p.bg.redF() * 0.84, a.greenF() * 0.16 + p.bg.greenF() * 0.84, a.blueF() * 0.16 + p.bg.blueF() * 0.84);
            p.onAccent = a.lightnessF() > 0.55 ? QColor("#08141a") : QColor("#ffffff");
        }
    }
    p.stage = QColor("#000000");
    p.overlay = QColor(20, 20, 15, 214);
    p.overlayText = QColor("#edebe2");
    p.overlayMuted = QColor(237, 235, 226, 150);
    p.overlayAccent = accentSample(m_accent, true);
    m_pal = p;
    emit changed();
}

void Theme::loadFonts() {
    const QString dir = Paths::appDir() + QStringLiteral("/fonts");
    for (const QFileInfo& fi : QDir(dir).entryInfoList({QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files)) {
        const int id = QFontDatabase::addApplicationFont(fi.absoluteFilePath());
        if (id < 0) continue;
    }
    const auto fams = QFontDatabase::families();
    if (fams.contains(QStringLiteral("Atkinson Hyperlegible Next"))) m_body = QStringLiteral("Atkinson Hyperlegible Next");
    if (fams.contains(QStringLiteral("Familjen Grotesk"))) m_head = QStringLiteral("Familjen Grotesk");
    else m_head = m_body;
}

QFont Theme::font(qreal px, int weight, bool heading) const {
    QFont f(heading ? m_head : m_body);
    f.setPixelSize(int(px));
    f.setWeight(QFont::Weight(weight));
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}
