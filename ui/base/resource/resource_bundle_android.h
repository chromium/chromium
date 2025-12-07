// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_
#define UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_

#include <jni.h>

#include <string>

#include "base/component_export.h"
#include "base/files/memory_mapped_file.h"
#include "ui/base/resource/resource_bundle.h"

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

// Returns the file descriptor and region for chrome_200_percent.pak.
// This requires buildflag `enable_hidpi` to be set to `true`.
COMPONENT_EXPORT(UI_BASE)
int Get200PercentResourcesPackFd(base::MemoryMappedFile::Region* out_region);

// Returns the file descriptors and regions for the locale .pak files. The first
// item in the vector is the main translation, and if a string cannot be found
// there, then we should fall back to the next item in the vector, and so on.
COMPONENT_EXPORT(UI_BASE)
const std::vector<ResourceBundle::FdAndRegion>& GetLocalePaks();

// Tell ResourceBundle to locate locale pak files via
// GetPathForAndroidLocalePakWithinApk rather than looking for them on disk.
COMPONENT_EXPORT(UI_BASE) void SetLocalePaksStoredInApk(bool value);

// Tell ResourceBundle to determine whether load non-webview locale .pak files
// based on whether non-webview locale .pak files exist.
COMPONENT_EXPORT(UI_BASE) void DetectAndSetLoadNonWebViewLocalePaks();

// Called in test when there are no locale pak files available.
COMPONENT_EXPORT(UI_BASE) void SetNoAvailableLocalePaksForTest();

// Called by ResourceBundle::UnloadLocaleResources() to clear Android-specific
// locale state.
COMPONENT_EXPORT(UI_BASE) void UnloadAndroidLocaleResources();

// Returns the existing locale packs and sets them to the given instance. This
// passes vectors by copy, but the vectors should be only up to size 4.
COMPONENT_EXPORT(UI_BASE)
std::vector<ResourceBundle::FdAndRegion> SwapAndroidGlobalsForTesting(
    const std::vector<ResourceBundle::FdAndRegion>& new_locale_packs);

// Get the density of the primary display. Use this instead of using Display
// to avoid initializing Display in child processes.
float GetPrimaryDisplayScale();

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_
