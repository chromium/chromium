// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle_android.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/android/apk_assets.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_paths.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/base/ui_base_jni_headers/ResourceBundle_jni.h"

namespace ui {

namespace {

using FileDescriptor = int;

bool g_locale_paks_in_apk = false;
bool g_load_non_webview_locale_paks = false;
// It is okay to cache and share these file descriptors since the
// ResourceBundle singleton never closes the handles.
FileDescriptor g_chrome_100_percent_fd = -1;
FileDescriptor g_chrome_200_percent_fd = -1;
FileDescriptor g_resources_pack_fd = -1;

std::vector<ResourceBundle::FdAndRegion>& GetLocalePaksGlobal() {
  // Required since `static std::vector<ResourceBundle::FdAndRegion>` requires a
  // global destructor.
  static base::NoDestructor<std::vector<ResourceBundle::FdAndRegion>>
      locale_paks;
  return *locale_paks;
}

base::MemoryMappedFile::Region g_chrome_100_percent_region;
base::MemoryMappedFile::Region g_chrome_200_percent_region;
base::MemoryMappedFile::Region g_resources_pack_region;

bool LoadFromApkOrFile(const char* apk_path,
                       const base::FilePath* disk_path,
                       FileDescriptor* out_fd,
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

// Returns the path within the apk for the given locale's .pak file, or an
// empty string if it doesn't exist.
// Only locale paks for the active Android language can be retrieved.
// If `in_split` is true, look into bundle split-specific location (e.g.
// 'assets/locales#lang_<lang>/<locale>.pak', otherwise use the default
// WebView-related location, i.e. 'assets/stored-locales/<locale>.pak'.
// If `log_error`, logs the path to logcat, but does not abort.
std::string GetPathForAndroidLocalePakWithinApk(std::string_view locale,
                                                ResourceBundle::Gender gender,
                                                bool in_bundle,
                                                bool log_error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> ret =
      Java_ResourceBundle_getLocalePakResourcePath(
          env, base::android::ConvertUTF8ToJavaString(env, locale),
          static_cast<jint>(gender), in_bundle, log_error);
  if (ret.obj() == nullptr) {
    return std::string();
  }
  return base::android::ConvertJavaStringToUTF8(env, ret.obj());
}

FileDescriptor LoadLocalePakFromApk(
    const std::string& app_locale,
    ResourceBundle::Gender gender,
    bool in_split,
    base::MemoryMappedFile::Region* out_region) {
  bool log_error = true;
  std::string locale_path_within_apk = GetPathForAndroidLocalePakWithinApk(
      app_locale, gender, in_split, log_error);
  if (locale_path_within_apk.empty()) {
    return -1;
  }
  return base::android::OpenApkAsset(locale_path_within_apk, out_region);
}

std::unique_ptr<DataPack> LoadDataPackFromLocalePak(
    FileDescriptor locale_pack_fd,
    const base::MemoryMappedFile::Region& region) {
  auto data_pack = std::make_unique<DataPack>(k100Percent);
  CHECK(data_pack->LoadFromFileRegion(base::File(locale_pack_fd), region))
      << "failed to load locale.pak";
  return data_pack;
}

bool LocaleDataPakExists(std::string_view locale,
                         ResourceBundle::Gender gender,
                         bool in_split,
                         bool log_error) {
  return !GetPathForAndroidLocalePakWithinApk(locale, gender, in_split,
                                              log_error)
              .empty();
}

bool LoadLocaleResourcesForLocaleAndGender(
    const std::string& app_locale,
    const ResourceBundle::Gender gender,
    ResourceBundle::FdAndRegion* webview_locale_pack,
    ResourceBundle::FdAndRegion* non_webview_locale_pack,
    std::vector<std::unique_ptr<ResourceHandle>>* locale_resources_data) {
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
  //    as the webview locale pak file.
  //
  //    If false, try to load it from the app bundle specific location
  //    (e.g. locales#lang_<language>/<locale>.pak). If the latter does not
  //    exist, try to lookup the APK-specific locale .pak file.
  //
  //    g_locale_paks_in_apk is set by SetLocalePaksStoredInApk() which
  //    is called from the WebView startup code.
  //
  //  g_load_non_webview_locale_paks:
  //    If true, load the Webview strings from stored-locales/<locale>.pak file
  //    as the non-webview locale pak file. Otherwise don't load a non-webview
  //    locale at all.
  //
  //    This is set by DetectAndSetLoadNonWebViewLocalePaks() which is called
  //    during ChromeMainDelegate::PostEarlyInitialization(). It will set the
  //    value to true iff there are stored-locale/ .pak files.
  //
  // In other words, if both |g_locale_paks_in_apk| and
  // |g_load_non_webview_locale_paks| are true, the stored-locales file will be
  // loaded twice as both the webview and non-webview. However, this should
  // never happen in practice.

  // Load webview locale .pak file.
  if (g_locale_paks_in_apk) {
    webview_locale_pack->fd = LoadLocalePakFromApk(
        app_locale, gender, false /* in_split */, &webview_locale_pack->region);
  } else {
    webview_locale_pack->fd = -1;

    CHECK(ResourceBundle::HasSharedInstance());

    // Support overridden pak path for testing.
    base::FilePath locale_file_path =
        ResourceBundle::GetSharedInstance().GetOverriddenPakPath();
    if (locale_file_path.empty()) {
      // Try to find the uncompressed split-specific asset file.
      webview_locale_pack->fd =
          LoadLocalePakFromApk(app_locale, gender, true /* in_split */,
                               &webview_locale_pack->region);
    }
    if (webview_locale_pack->fd < 0) {
      // Otherwise, try to locate the side-loaded locale .pak file (for tests).
      if (locale_file_path.empty()) {
        auto path =
            ResourceBundle::GetSharedInstance().GetLocaleFilePath(app_locale);
        if (base::PathExists(path)) {
          locale_file_path = std::move(path);
        }
      }

      if (locale_file_path.empty()) {
        // It's possible that there is no locale.pak.
        LOG(WARNING) << "locale_file_path.empty() for locale " << app_locale;
        return false;
      }
      auto flags =
          static_cast<uint32_t>(base::File::FLAG_OPEN | base::File::FLAG_READ);
      webview_locale_pack->fd =
          base::File(locale_file_path, flags).TakePlatformFile();
      webview_locale_pack->region = base::MemoryMappedFile::Region::kWholeFile;
    }
  }

  auto locale_data = LoadDataPackFromLocalePak(webview_locale_pack->fd,
                                               webview_locale_pack->region);

  if (!locale_data.get()) {
    return false;
  }

  locale_resources_data->push_back(std::move(locale_data));

  // Load non-webview locale .pak file if it exists. For debug build monochrome,
  // a non-webview locale pak will always be loaded; however, it should be
  // unnecessary for loading locale resources because the webview locale pak
  // would have a copy of all the resources in the non-webview locale pak.
  if (g_load_non_webview_locale_paks) {
    non_webview_locale_pack->fd =
        LoadLocalePakFromApk(app_locale, gender, false /* in_split */,
                             &non_webview_locale_pack->region);

    locale_data = LoadDataPackFromLocalePak(non_webview_locale_pack->fd,
                                            non_webview_locale_pack->region);

    if (!locale_data.get()) {
      return false;
    }

    locale_resources_data->push_back(std::move(locale_data));
  }

  return true;
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

  if constexpr (BUILDFLAG(ENABLE_HIDPI)) {
    disk_path = disk_path.DirName().AppendASCII("chrome_200_percent.pak");
    success = LoadFromApkOrFile("assets/chrome_200_percent.pak", &disk_path,
                                &g_chrome_200_percent_fd,
                                &g_chrome_200_percent_region);
    if (success) {
      AddDataPackFromFileRegion(base::File(g_chrome_200_percent_fd),
                                g_chrome_200_percent_region, k200Percent);
    }
  }
}

// static
bool ResourceBundle::LocaleDataPakExists(std::string_view locale,
                                         Gender gender) {
  const bool in_split = !g_locale_paks_in_apk;
  const bool exists = ::ui::LocaleDataPakExists(locale, gender, in_split,
                                                /*log_error=*/false);
  if (exists || !in_split) {
    return exists;
  }

  // Fall back to checking on disk, which is necessary only for tests.
  const auto path = GetLocaleFilePath(locale);
  return !path.empty() && base::PathExists(path);
}

std::string ResourceBundle::LoadLocaleResources(const std::string& pref_locale,
                                                bool crash_on_failure) {
  DCHECK_EQ(locale_resources_data_.size(), 0u) << "locale.pak already loaded";
  std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);

  // TODO(crbug.com/420947195): Fetch user's gender (behind a flag).
  const Gender gender = Gender::kDefault;

  FdAndRegion webview_locale_pack;
  FdAndRegion non_webview_locale_pack;
  non_webview_locale_pack.fd = -1;

  if (!LoadLocaleResourcesForLocaleAndGender(
          app_locale, gender, &webview_locale_pack, &non_webview_locale_pack,
          &locale_resources_data_)) {
    return std::string();
  }

  std::vector<ResourceBundle::FdAndRegion>& locale_packs =
      GetLocalePaksGlobal();
  CHECK_EQ(locale_packs.size(), 0u);

  webview_locale_pack.purpose = LocalePakPurpose::kWebViewMain;
  locale_packs.push_back(webview_locale_pack);

  if (non_webview_locale_pack.fd >= 0) {
    non_webview_locale_pack.purpose = LocalePakPurpose::kNonWebViewMain;
    locale_packs.push_back(non_webview_locale_pack);
  }

  if (gender != Gender::kDefault) {
    non_webview_locale_pack.fd = -1;

    if (!LoadLocaleResourcesForLocaleAndGender(
            app_locale, Gender::kDefault, &webview_locale_pack,
            &non_webview_locale_pack, &locale_resources_data_)) {
      return std::string();
    }

    webview_locale_pack.purpose = LocalePakPurpose::kWebViewFallback;
    locale_packs.push_back(webview_locale_pack);

    if (non_webview_locale_pack.fd >= 0) {
      non_webview_locale_pack.purpose = LocalePakPurpose::kNonWebViewFallback;
      locale_packs.push_back(non_webview_locale_pack);
    }
  }

  return app_locale;
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}

void SetLocalePaksStoredInApk(bool value) {
  g_locale_paks_in_apk = value;
}

void DetectAndSetLoadNonWebViewLocalePaks() {
  // Auto-detect based on en-US whether non-webview locale .pak files exist.
  g_load_non_webview_locale_paks =
      LocaleDataPakExists("en-US", ResourceBundle::Gender::kDefault,
                          false /* in_split */, false /* log_error */);
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
  FileDescriptor fd = base::android::OpenApkAsset(path, split_name, &region);
  CHECK_GE(fd, 0) << "Could not find " << path << " in APK.";
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      base::File(fd), region, ui::kScaleFactorNone);
}

FileDescriptor GetMainAndroidPackFd(
    base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_resources_pack_fd, 0);
  *out_region = g_resources_pack_region;
  return g_resources_pack_fd;
}

FileDescriptor GetCommonResourcesPackFd(
    base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_chrome_100_percent_fd, 0);
  *out_region = g_chrome_100_percent_region;
  return g_chrome_100_percent_fd;
}

FileDescriptor Get200PercentResourcesPackFd(
    base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_chrome_200_percent_fd, 0);
  *out_region = g_chrome_200_percent_region;
  return g_chrome_200_percent_fd;
}

const std::vector<ResourceBundle::FdAndRegion>& GetLocalePaks() {
  const std::vector<ResourceBundle::FdAndRegion>& locale_packs =
      GetLocalePaksGlobal();
  CHECK_GT(locale_packs.size(), 0u);
  return locale_packs;
}

void SetNoAvailableLocalePaksForTest() {
  Java_ResourceBundle_setNoAvailableLocalePaks(
      base::android::AttachCurrentThread());
}

void UnloadAndroidLocaleResources() {
  GetLocalePaksGlobal().clear();
}

std::vector<ResourceBundle::FdAndRegion> SwapAndroidGlobalsForTesting(
    const std::vector<ResourceBundle::FdAndRegion>& new_locale_packs) {
  return std::exchange(GetLocalePaksGlobal(), new_locale_packs);
}

}  // namespace ui

DEFINE_JNI(ResourceBundle)
