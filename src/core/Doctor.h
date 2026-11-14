// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

// The platform-neutral part of the Doctor: playback engine, TLS for updates, settings files (with their backups), crash
// reports, leftover update files, cache sizes, and whether github.com can be reached (a plain TCP connect, nothing sent).
// The Windows setup window adds the install-specific checks (files, shortcuts, registry) on top; macOS and Linux use
// `kinoz --doctor` for a text report. The Doctor never touches the user's media, drivers or system settings.
namespace Doctor {
struct Check { QString name, detail; int state = 0; bool fixable = false; QString fix; };   // state: 0 ok, 1 warning, 2 problem

// basics: also check that the data folder is writable and how much disk is free (the Windows window does that itself).
QVector<Check> runCore(const QString& dataDir, bool network, bool engine, bool basics);
void applyCoreFix(const QString& fixId, const QString& dataDir);       // fixId as returned in Check::fix
QString textReport(const QVector<Check>& checks);                       // one line per check, with a summary line
int runTextMode(const QStringList& args);                               // `kinoz --doctor[-text] [--fix]`; returns the process exit code
}
