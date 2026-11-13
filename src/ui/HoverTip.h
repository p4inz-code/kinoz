// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QRect>
#include <QString>

// A small themed label that names an icon and shows its shortcut ("Fullscreen<TAB>F").
// Appears after a short pause over a button, at once when you slide from one button to the next, and vanishes
// the moment you click or leave. Drawn by kinoz, so it matches the theme.
namespace HoverTip {
void request(const QRect& anchorGlobal, const QString& text);   // text is "Label" or "Label\tShortcut"
void cancel();
}
