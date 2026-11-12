// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QString>

// "Make kinoz my default player". No OS lets an app silently take over file types, so this does the part that is
// allowed and opens the right place for the rest:
//   Windows  registers kinoz in Settings > Default apps, then opens kinoz's own page there (one click to set all types)
//   Linux    sets kinoz as the handler for common video/audio/image types with xdg-mime
//   macOS    can't be scripted: returns the two-step instruction (Get Info > Open with > Change All)
namespace DefaultApp {
struct Result { bool ok; QString message; };
Result apply();
}
