// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GLIB_GSETTINGS_H_
#define UI_BASE_GLIB_GSETTINGS_H_

#include "ui/base/glib/scoped_gobject.h"

using GSettings = struct _GSettings;
using GVariantType = struct _GVariantType;

namespace ui {

// Creates a new GSettings object for the given schema.  If the schema does not
// exist, this will return nullptr.
ScopedGObject<GSettings> GSettingsNew(const char* schema);

// Returns true if `settings` has a key named `key_name` whose value type
// matches `expected_type` (or any type if `expected_type` is null).  A missing
// key causes `g_settings_get_*()` to abort via `g_error()`; a wrong-typed key
// logs `g_critical` and returns a fallback value (`FALSE` for
// `g_settings_get_boolean()`, `nullptr` for `g_settings_get_string()`) rather
// than aborting.
bool GSettingsHasKey(GSettings* settings,
                     const char* key_name,
                     const GVariantType* expected_type = nullptr);

// GDK3's Wayland backend reads font rendering settings from GSettings using
// g_settings_get_enum(), which aborts the process if a key is missing or is not
// an enum.  This can happen on misconfigured systems with missing, stale, or
// corrupted schemas (see https://crbug.com/434763642 and
// https://crbug.com/573086583).  Returns false if the installed schemas would
// cause GDK to abort, in which case GTK3 must not be initialized on Wayland,
// either directly or indirectly (eg. by QT's GTK3 platform theme).  On failure,
// the reason is logged and recorded in a crash key.
bool AreGdkWaylandFontSchemasValid();

}  // namespace ui

#endif  // UI_BASE_GLIB_GSETTINGS_H_
