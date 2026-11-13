// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <mpv/client.h>
#include <cstdio>

// mpv 0.38 added an "index" argument to `loadfile` (file, flags, index, options). Older libmpv (Ubuntu 22.04, Debian stable)
// takes (file, flags, options) and rejects the new form, so the form is chosen from the version of the library actually loaded.
inline bool mpvLoadfileHasIndex(mpv_handle* h) {
    static int cached = -1;
    if (cached < 0) {
        cached = 1;                                                    // unknown version: assume a recent one
        if (char* s = mpv_get_property_string(h, "mpv-version")) {
            const char* p = s;
            while (*p && (*p < '0' || *p > '9')) ++p;
            int maj = 0, min = 0;
            if (std::sscanf(p, "%d.%d", &maj, &min) == 2) cached = (maj > 0 || min >= 38) ? 1 : 0;
            mpv_free(s);
        }
    }
    return cached == 1;
}
