// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QString>

// Everything the app writes lives next to the exe, in ./data  (portable, never C:).
namespace Paths {
QString appDir();
QString dataDir();
QString cacheDir();
QString thumbsDir();
QString tmpDir();
QString exportsDir();
QString logsDir();
void ensure();
}
