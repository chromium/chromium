// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/glib/gsettings.h"

#include <gio/gio.h>

namespace ui {

ScopedGObject<GSettings> GSettingsNew(const char* schema) {
  // g_settings_new() will fatally fail if the schema does not exist, so
  // use g_settings_schema_source_lookup() to check for it first.
  auto* schema_source = g_settings_schema_source_get_default();
  auto* settings_schema =
      g_settings_schema_source_lookup(schema_source, schema, true);
  if (!settings_schema) {
    return nullptr;
  }
  auto settings =
      TakeGObject(g_settings_new_full(settings_schema, nullptr, nullptr));
  g_settings_schema_unref(settings_schema);
  return settings;
}

}  // namespace ui
