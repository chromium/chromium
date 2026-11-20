// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/glib/gsettings.h"

#define G_SETTINGS_ENABLE_BACKEND 1
#include <gio/gio.h>
#include <gio/gsettingsbackend.h>

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

ScopedGObject<GSettings> CreateMemorySettings(const char* schema_id) {
  GSettingsSchemaSource* source = g_settings_schema_source_get_default();
  if (!source) {
    return {};
  }
  std::unique_ptr<GSettingsSchema, decltype(&g_settings_schema_unref)> schema(
      g_settings_schema_source_lookup(source, schema_id, /*recursive=*/true),
      &g_settings_schema_unref);
  if (!schema) {
    return {};
  }
  ScopedGObject<GSettingsBackend> backend =
      TakeGObject(g_memory_settings_backend_new());
  return TakeGObject(
      g_settings_new_full(schema.get(), backend.get(), /*path=*/nullptr));
}

}  // namespace

TEST(GSettingsTest, HasKeyHandlesNullInputs) {
  EXPECT_FALSE(
      GSettingsHasKey(nullptr, "show-banners", G_VARIANT_TYPE_BOOLEAN));
  EXPECT_FALSE(GSettingsHasKey(nullptr, "show-banners"));
  EXPECT_FALSE(GSettingsHasKey(nullptr, nullptr));

  ScopedGObject<GSettings> settings =
      CreateMemorySettings("org.gnome.desktop.notifications");
  if (!settings) {
    GTEST_SKIP() << "org.gnome.desktop.notifications schema is not installed.";
  }
  EXPECT_FALSE(
      GSettingsHasKey(settings.get(), nullptr, G_VARIANT_TYPE_BOOLEAN));
  EXPECT_FALSE(GSettingsHasKey(settings.get(), nullptr));
}

TEST(GSettingsTest, HasKeyValidatesPresenceAndType) {
  ScopedGObject<GSettings> settings =
      CreateMemorySettings("org.gnome.desktop.notifications");
  if (!settings) {
    GTEST_SKIP() << "org.gnome.desktop.notifications schema is not installed.";
  }

  // Boolean key matches boolean or wildcard expected type, not string.
  EXPECT_TRUE(
      GSettingsHasKey(settings.get(), "show-banners", G_VARIANT_TYPE_BOOLEAN));
  EXPECT_TRUE(GSettingsHasKey(settings.get(), "show-banners"));
  EXPECT_FALSE(
      GSettingsHasKey(settings.get(), "show-banners", G_VARIANT_TYPE_STRING));

  // String-array key does not match boolean.
  EXPECT_TRUE(GSettingsHasKey(settings.get(), "application-children",
                              G_VARIANT_TYPE_STRING_ARRAY));
  EXPECT_TRUE(GSettingsHasKey(settings.get(), "application-children"));
  EXPECT_FALSE(GSettingsHasKey(settings.get(), "application-children",
                               G_VARIANT_TYPE_BOOLEAN));

  // Missing key returns false regardless of expected type.
  EXPECT_FALSE(GSettingsHasKey(settings.get(), "nonexistent-key",
                               G_VARIANT_TYPE_BOOLEAN));
  EXPECT_FALSE(GSettingsHasKey(settings.get(), "nonexistent-key"));
}

}  // namespace ui
