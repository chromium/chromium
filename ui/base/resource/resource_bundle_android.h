// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_
#define UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/component_export.h"
#include "base/files/memory_mapped_file.h"

namespace ui {

// Loads "resources.apk" from the .apk. Falls back to loading from disk, which
// is necessary for tests.
COMPONENT_EXPORT(UI_BASE)
void LoadMainAndroidPackFile(const char* path_within_apk,
                             const base::FilePath& disk_file_path);

// Loads a PAK file from the APK and makes the contained resources accessible.
COMPONENT_EXPORT(UI_BASE)
void LoadPackFileFromApk(const std::string& path,
                         const std::string& split_name);

// Returns the file descriptor and region for resources.pak.
COMPONENT_EXPORT(UI_BASE)
int GetMainAndroidPackFd(base::MemoryMappedFile::Region* out_region);

// Returns the file descriptor and region for chrome_100_percent.pak.
COMPONENT_EXPORT(UI_BASE)
int GetCommonResourcesPackFd(base::MemoryMappedFile::Region* out_region);

// Returns the file descriptor and region for the locale .pak file.
COMPONENT_EXPORT(UI_BASE)
int GetLocalePackFd(base::MemoryMappedFile::Region* out_region);

// Returns the file descriptor and region for the secondary locale .pak file.
COMPONENT_EXPORT(UI_BASE)
int GetSecondaryLocalePackFd(base::MemoryMappedFile::Region* out_region);

// Tell ResourceBundle to locate locale pak files via
// GetPathForAndroidLocalePakWithinApk rather than looking for them on disk.
COMPONENT_EXPORT(UI_BASE) void SetLocalePaksStoredInApk(bool value);

// Tell ResourceBundle to load secondary locale .pak files.
COMPONENT_EXPORT(UI_BASE) void SetLoadSecondaryLocalePaks(bool value);

// Returns the path within the apk for the given locale's .pak file, or an
// empty string if it doesn't exist.
// Only locale paks for the active Android language can be retrieved.
// If |in_split| is true, look into bundle split-specific location (e.g.
// 'assets/locales#lang_<lang>/<locale>.pak', otherwise use the default
// WebView-related location, i.e. 'assets/stored-locales/<locale>.pak'.
// If |log_error|, logs the path to logcat, but does not abort.
COMPONENT_EXPORT(UI_BASE)
std::string GetPathForAndroidLocalePakWithinApk(const std::string& locale,
                                                bool in_split,
                                                bool log_error);

// Called in test when there are no locale pak files available.
COMPONENT_EXPORT(UI_BASE) void SetNoAvailableLocalePaksForTest();

// Get the density of the primary display. Use this instead of using Display
// to avoid initializing Display in child processes.
float GetPrimaryDisplayScale();

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_
