// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_
#define UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/files/memory_mapped_file.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// Loads "resources.apk" from the .apk. Falls back to loading from disk, which
// is necessary for tests. Returns true if it succeeds, false otherwise.
UI_BASE_EXPORT void LoadMainAndroidPackFile(
    const char* path_within_apk,
    const base::FilePath& disk_file_path);

// Returns the file descriptor and region for resources.pak.
UI_BASE_EXPORT int GetMainAndroidPackFd(
    base::MemoryMappedFile::Region* out_region);

// Returns the file descriptor and region for chrome_100_percent.pak.
UI_BASE_EXPORT int GetCommonResourcesPackFd(
    base::MemoryMappedFile::Region* out_region);

// Returns the file descriptor and region for the locale .pak file.
UI_BASE_EXPORT int GetLocalePackFd(
    base::MemoryMappedFile::Region* out_region);

// Returns the file descriptor and region for the secondary locale .pak file.
UI_BASE_EXPORT int GetSecondaryLocalePackFd(
    base::MemoryMappedFile::Region* out_region);

// Tell ResourceBundle to locate locale pak files via
// GetPathForAndroidLocalePakWithinApk rather than looking for them on disk.
UI_BASE_EXPORT void SetLocalePaksStoredInApk(bool value);

// Tell ResourceBundle to load secondary locale .pak files.
UI_BASE_EXPORT void SetLoadSecondaryLocalePaks(bool value);

// Returns the path within the apk for the given locale's .pak file, or an
// empty string if it doesn't exist.
// Only locale paks for the active Android language can be retrieved.
UI_BASE_EXPORT std::string GetPathForAndroidLocalePakWithinApk(
    const std::string& locale);

// Get the density of the primary display. Use this instead of using Display
// to avoid initializing Display in child processes.
float GetPrimaryDisplayScale();

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_
