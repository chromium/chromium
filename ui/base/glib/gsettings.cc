// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/glib/gsettings.h"

#include <gio/gio.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/strings/strcat.h"

namespace ui {

namespace {

struct GFreeDeleter {
  void operator()(gchar* ptr) const { g_free(ptr); }
};
struct GVariantDeleter {
  void operator()(GVariant* ptr) const { g_variant_unref(ptr); }
};
struct GSettingsSchemaKeyDeleter {
  void operator()(GSettingsSchemaKey* ptr) const {
    g_settings_schema_key_unref(ptr);
  }
};
struct GSettingsSchemaDeleter {
  void operator()(GSettingsSchema* ptr) const { g_settings_schema_unref(ptr); }
};

// Logs the reason the schemas are invalid and records it in a crash key so
// that crashes that happen anyway (eg. if another library initializes GTK3)
// can be diagnosed.
void ReportInvalidSchema(std::string_view schema,
                         std::string_view key,
                         std::string_view reason) {
  static auto* const crash_key = base::debug::AllocateCrashKeyString(
      "gdk-wayland-font-schema-error", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(crash_key,
                                 base::StrCat({schema, ":", key, ":", reason}));
  LOG(ERROR) << "Invalid GSettings schema " << schema << " for key " << key
             << ": " << reason;
}

}  // namespace

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

bool AreGdkWaylandFontSchemasValid() {
  // These must be kept in sync with update_xft_settings() in GDK's
  // gdk/wayland/gdkscreen-wayland.c.
  struct {
    const char* interface;
    std::array<const char*, 3> keys;
  } static constexpr kInterfaces[] = {
      {"org.gnome.desktop.interface",
       {"font-antialiasing", "font-hinting", "font-rgba-order"}},
      {"org.gnome.settings-daemon.plugins.xsettings",
       {"antialiasing", "hinting", "rgba-order"}},
  };

  auto* source = g_settings_schema_source_get_default();
  if (!source) {
    return true;
  }

  for (const auto& interface : kInterfaces) {
    std::unique_ptr<GSettingsSchema, GSettingsSchemaDeleter> schema(
        g_settings_schema_source_lookup(source, interface.interface,
                                        /*recursive=*/true));
    if (!schema) {
      // Not an error, try the next schema.
      continue;
    }

    for (const char* key_string : interface.keys) {
      // Checking for the key first is required, otherwise
      // g_settings_schema_get_key() could crash.
      if (!g_settings_schema_has_key(schema.get(), key_string)) {
        ReportInvalidSchema(interface.interface, key_string, "missing");
        return false;
      }

      std::unique_ptr<GSettingsSchemaKey, GSettingsSchemaKeyDeleter> key(
          g_settings_schema_get_key(schema.get(), key_string));
      if (!key) {
        ReportInvalidSchema(interface.interface, key_string, "get-key-failed");
        return false;
      }

      std::unique_ptr<GVariant, GVariantDeleter> range(
          g_settings_schema_key_get_range(key.get()));
      if (!range) {
        ReportInvalidSchema(interface.interface, key_string, "no-range");
        return false;
      }

      char* type_string = nullptr;
      g_variant_get(range.get(), "(sv)", &type_string, nullptr);
      std::unique_ptr<gchar, GFreeDeleter> type_string_deleter(type_string);
      if (!type_string || type_string != std::string_view("enum")) {
        ReportInvalidSchema(
            interface.interface, key_string,
            base::StrCat(
                {"not-enum(", type_string ? type_string : "null", ")"}));
        return false;
      }
    }

    // Valid schema. Return now since GDK uses the first present schema.
    return true;
  }

  // No schema found. This is acceptable, because GDK will fallback to using
  // default values.
  return true;
}

}  // namespace ui
