// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/apk_assets.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_paths.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/base/ui_base_jni_headers/ResourceBundle_jni.h"

namespace ui {

namespace {

bool g_locale_paks_in_apk = false;
bool g_load_secondary_locale_paks = false;
// It is okay to cache and share these file descriptors since the
// ResourceBundle singleton never closes the handles.
int g_chrome_100_percent_fd = -1;
int g_resources_pack_fd = -1;
int g_locale_pack_fd = -1;
int g_secondary_locale_pack_fd = -1;
base::MemoryMappedFile::Region g_chrome_100_percent_region;
base::MemoryMappedFile::Region g_resources_pack_region;
base::MemoryMappedFile::Region g_locale_pack_region;
base::MemoryMappedFile::Region g_secondary_locale_pack_region;

bool LoadFromApkOrFile(const char* apk_path,
                       const base::FilePath* disk_path,
                       int* out_fd,
                       base::MemoryMappedFile::Region* out_region) {
  DCHECK_EQ(*out_fd, -1) << "Attempt to load " << apk_path << " twice.";
  if (apk_path != nullptr) {
    *out_fd = base::android::OpenApkAsset(apk_path, out_region);
  }
  // For unit tests, the file exists on disk.
  if (*out_fd < 0 && disk_path != nullptr) {
    auto flags =
        static_cast<uint32_t>(base::File::FLAG_OPEN | base::File::FLAG_READ);
    *out_fd = base::File(*disk_path, flags).TakePlatformFile();
    *out_region = base::MemoryMappedFile::Region::kWholeFile;
  }
  bool success = *out_fd >= 0;
  if (!success) {
    LOG(ERROR) << "Failed to open pak file: " << apk_path;
    base::android::DumpLastOpenApkAssetFailure();
  }
  return success;
}

int LoadLocalePakFromApk(const std::string& app_locale,
                         bool in_split,
                         base::MemoryMappedFile::Region* out_region) {
  bool log_error = true;
  std::string locale_path_within_apk =
      GetPathForAndroidLocalePakWithinApk(app_locale, in_split, log_error);
  if (locale_path_within_apk.empty()) {
    return -1;
  }
  return base::android::OpenApkAsset(locale_path_within_apk, out_region);
}

std::unique_ptr<DataPack> LoadDataPackFromLocalePak(
    int locale_pack_fd,
    const base::MemoryMappedFile::Region& region) {
  auto data_pack = std::make_unique<DataPack>(k100Percent);
  CHECK(data_pack->LoadFromFileRegion(base::File(locale_pack_fd), region))
      << "failed to load locale.pak";
  return data_pack;
}

}  // namespace

void ResourceBundle::LoadCommonResources() {
  base::FilePath disk_path;
  base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &disk_path);
  disk_path = disk_path.AppendASCII("chrome_100_percent.pak");
  bool success =
      LoadFromApkOrFile("assets/chrome_100_percent.pak", &disk_path,
                        &g_chrome_100_percent_fd, &g_chrome_100_percent_region);
  DCHECK(success);

  AddDataPackFromFileRegion(base::File(g_chrome_100_percent_fd),
                            g_chrome_100_percent_region, k100Percent);
}

// static
bool ResourceBundle::LocaleDataPakExists(const std::string& locale) {
  bool log_error = false;
  bool in_split = !g_locale_paks_in_apk;
  if (!in_split) {
    return !GetPathForAndroidLocalePakWithinApk(locale, in_split, log_error)
                .empty();
  }
  if (!GetPathForAndroidLocalePakWithinApk(locale, in_split, log_error).empty())
    return true;

  // Fall back to checking on disk, which is necessary only for tests.
  const auto path = GetLocaleFilePath(locale);
  return !path.empty() && base::PathExists(path);
}

std::string ResourceBundle::LoadLocaleResources(const std::string& pref_locale,
                                                bool crash_on_failure) {
  DCHECK(!locale_resources_data_.get() &&
         !secondary_locale_resources_data_.get())
             << "locale.pak already loaded";
  std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);

  // Some Chromium apps have two sets of .pak files for their UI strings, i.e.:
  //
  // a) WebView strings, which are always stored uncompressed under
  //    assets/stored-locales/ inside the APK or App Bundle.
  //
  // b) For APKs, the Chrome UI strings are stored under assets/locales/.
  //
  // c) For App Bundles, Chrome UI strings are stored uncompressed under
  //    assets/locales#lang_<lang>/ (where <lang> is an Android language code)
  //    and assets/fallback-locales/ (for en-US.pak only).
  //
  // Which .pak files to load are determined here by two global variables with
  // the following meaning:
  //
  //  g_locale_paks_in_apk:
  //    If true, load the WebView strings from stored-locales/<locale>.pak file
  //    as the primary locale pak file.
  //
  //    If false, try to load it from the app bundle specific location
  //    (e.g. locales#lang_<language>/<locale>.pak). If the latter does not
  //    exist, try to lookup the APK-specific locale .pak file.
  //
  //    g_locale_paks_in_apk is set by SetLocalePaksStoredInApk() which
  //    is called from the WebView startup code.
  //
  //  g_load_secondary_locale_paks:
  //    If true, load the Webview strings from stored-locales/<locale>.pak file
  //    as the secondary locale pak file. Otherwise don't load a secondary
  //    locale at all.
  //
  //    This is set by SetLoadSecondaryLocalePaks() which is called
  //    during ChromeMainDelegate::PostEarlyInitialization() with a value
  //    that is true iff there are stored-locale/ .pak files.
  //
  // In other words, if both |g_locale_paks_in_apk| and
  // |g_load_secondary_locale_paks| are true, the stored-locales file will be
  // loaded twice as both the primary and secondary. However, this should
  // never happen in practice.

  // Load primary locale .pak file.
  if (g_locale_paks_in_apk) {
    g_locale_pack_fd =
        LoadLocalePakFromApk(app_locale, false, &g_locale_pack_region);
  } else {
    // Support overridden pak path for testing.
    base::FilePath locale_file_path = GetOverriddenPakPath();
    if (locale_file_path.empty()) {
      // Try to find the uncompressed split-specific asset file.
      g_locale_pack_fd =
          LoadLocalePakFromApk(app_locale, true, &g_locale_pack_region);
    }
    if (g_locale_pack_fd < 0) {
      // Otherwise, try to locate the side-loaded locale .pak file (for tests).
      if (locale_file_path.empty()) {
        auto path = GetLocaleFilePath(app_locale);
        if (base::PathExists(path))
          locale_file_path = std::move(path);
      }

      if (locale_file_path.empty()) {
        // It's possible that there is no locale.pak.
        LOG(WARNING) << "locale_file_path.empty() for locale " << app_locale;
        return std::string();
      }
      auto flags =
          static_cast<uint32_t>(base::File::FLAG_OPEN | base::File::FLAG_READ);
      g_locale_pack_fd = base::File(locale_file_path, flags).TakePlatformFile();
      g_locale_pack_region = base::MemoryMappedFile::Region::kWholeFile;
    }
  }

  locale_resources_data_ = LoadDataPackFromLocalePak(
      g_locale_pack_fd, g_locale_pack_region);

  if (!locale_resources_data_.get())
    return std::string();

  // Load secondary locale .pak file if it exists. For debug build monochrome,
  // a secondary locale pak will always be loaded; however, it should be
  // unnecessary for loading locale resources because the primary locale pak
  // would have a copy of all the resources in the secondary locale pak.
  if (g_load_secondary_locale_paks) {
    g_secondary_locale_pack_fd = LoadLocalePakFromApk(
        app_locale, false, &g_secondary_locale_pack_region);

    secondary_locale_resources_data_ = LoadDataPackFromLocalePak(
        g_secondary_locale_pack_fd, g_secondary_locale_pack_region);

    if (!secondary_locale_resources_data_.get())
      return std::string();
  }

  return app_locale;
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}

void SetLocalePaksStoredInApk(bool value) {
  g_locale_paks_in_apk = value;
}

void SetLoadSecondaryLocalePaks(bool value) {
  g_load_secondary_locale_paks = value;
}

void LoadMainAndroidPackFile(const char* path_within_apk,
                             const base::FilePath& disk_file_path) {
  if (LoadFromApkOrFile(path_within_apk,
                        &disk_file_path,
                        &g_resources_pack_fd,
                        &g_resources_pack_region)) {
    ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
        base::File(g_resources_pack_fd), g_resources_pack_region,
        kScaleFactorNone);
  }
}

void LoadPackFileFromApk(const std::string& path,
                         const std::string& split_name) {
  base::MemoryMappedFile::Region region;
  int fd = base::android::OpenApkAsset(path, split_name, &region);
  CHECK_GE(fd, 0) << "Could not find " << path << " in APK.";
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      base::File(fd), region, ui::kScaleFactorNone);
}

int GetMainAndroidPackFd(base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_resources_pack_fd, 0);
  *out_region = g_resources_pack_region;
  return g_resources_pack_fd;
}

int GetCommonResourcesPackFd(base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_chrome_100_percent_fd, 0);
  *out_region = g_chrome_100_percent_region;
  return g_chrome_100_percent_fd;
}

int GetLocalePackFd(base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_locale_pack_fd, 0);
  *out_region = g_locale_pack_region;
  return g_locale_pack_fd;
}

int GetSecondaryLocalePackFd(base::MemoryMappedFile::Region* out_region) {
  *out_region = g_secondary_locale_pack_region;
  return g_secondary_locale_pack_fd;
}

std::string GetPathForAndroidLocalePakWithinApk(const std::string& locale,
                                                bool in_bundle,
                                                bool log_error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> ret =
      Java_ResourceBundle_getLocalePakResourcePath(
          env, base::android::ConvertUTF8ToJavaString(env, locale), in_bundle,
          log_error);
  if (ret.obj() == nullptr) {
    return std::string();
  }
  return base::android::ConvertJavaStringToUTF8(env, ret.obj());
}

void SetNoAvailableLocalePaksForTest() {
  Java_ResourceBundle_setNoAvailableLocalePaks(
      base::android::AttachCurrentThread());
}

}  // namespace ui
