// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GLIB_GSETTINGS_H_
#define UI_BASE_GLIB_GSETTINGS_H_

#include "ui/base/glib/scoped_gobject.h"

using GSettings = struct _GSettings;

namespace ui {

// Creates a new GSettings object for the given schema.  If the schema does not
// exist, this will return nullptr.
ScopedGObject<GSettings> GSettingsNew(const char* schema);

}  // namespace ui

#endif  // UI_BASE_GLIB_GSETTINGS_H_
