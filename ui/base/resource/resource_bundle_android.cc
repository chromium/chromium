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
#include "base/i18n/language_tag.h"
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
// Only locale paks for the active Android language can be retrieved when
// installed as splits.
// If `log_error`, logs the path to logcat, but does not abort.
std::string GetPathForAndroidLocalePakWithinApk(std::string_view locale,
                                                ResourceBundle::Gender gender,
                                                bool log_error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> ret =
      Java_ResourceBundle_getLocalePakResourcePath(
          env, base::android::ConvertUTF8ToJavaString(env, locale),
          static_cast<int32_t>(gender), log_error);
  if (ret.obj() == nullptr) {
    return std::string();
  }
  return base::android::ConvertJavaStringToUTF8(env, ret.obj());
}

FileDescriptor LoadLocalePakFromApk(
    const std::string& app_locale,
    ResourceBundle::Gender gender,
    base::MemoryMappedFile::Region* out_region) {
  std::string locale_path_within_apk = GetPathForAndroidLocalePakWithinApk(
      app_locale, gender, /*log_error=*/true);
  if (locale_path_within_apk.empty()) {
    return -1;
  }
  return base::android::OpenApkAsset(locale_path_within_apk, out_region);
}

bool LoadLocaleResourcesForLocaleAndGender(
    const std::string& app_locale,
    const ResourceBundle::Gender gender,
    ResourceBundle::FdAndRegion* out_locale_pack,
    std::vector<std::unique_ptr<ResourceHandle>>* locale_resources_data) {
  // Locale .pak files are stored uncompressed inside the APK or App Bundle
  // under assets/locales#lang_<lang>/ (where <lang> is an Android language
  // code) and assets/fallback-locales/ (for the non-gendered en-US.pak only).
  // For App Bundles with language splitting enabled, bundletool puts the former
  // into language splits.
  //
  // When `g_locale_paks_in_apk` is true (set by SetLocalePaksStoredInApk() in
  // WebView and Cast), only APK assets are checked. Otherwise, overridden and
  // side-loaded disk .pak files are also checked for tests.
  if (g_locale_paks_in_apk) {
    out_locale_pack->fd =
        LoadLocalePakFromApk(app_locale, gender, &out_locale_pack->region);
  } else {
    out_locale_pack->fd = -1;

    CHECK(ResourceBundle::HasSharedInstance());

    // Support overridden pak path for testing.
    base::FilePath locale_file_path =
        ResourceBundle::GetSharedInstance().GetOverriddenPakPath();
    if (locale_file_path.empty()) {
      // Try to find the uncompressed asset file.
      out_locale_pack->fd =
          LoadLocalePakFromApk(app_locale, gender, &out_locale_pack->region);
    }
    if (out_locale_pack->fd < 0) {
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
      out_locale_pack->fd =
          base::File(locale_file_path, flags).TakePlatformFile();
      out_locale_pack->region = base::MemoryMappedFile::Region::kWholeFile;
    }
  }

  auto locale_data = std::make_unique<DataPack>(k100Percent);
  CHECK(locale_data->LoadFromFileRegion(base::File(out_locale_pack->fd),
                                        out_locale_pack->region))
      << "failed to load locale.pak";
  locale_resources_data->push_back(std::move(locale_data));
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
  // TODO(crbug.com/586004162): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
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
bool ResourceBundle::LocaleDataPakExists(const base::i18n::LanguageTag& locale,
                                         Gender gender) {
  const bool exists = !GetPathForAndroidLocalePakWithinApk(
                           locale.tag_string(), gender, /*log_error=*/false)
                           .empty();
  if (exists || g_locale_paks_in_apk) {
    return exists;
  }

  // Fall back to checking on disk, which is necessary only for tests.
  const auto path = GetLocaleFilePath(locale.tag_string());
  return !path.empty() && base::PathExists(path);
}

std::string ResourceBundle::LoadLocaleResources(const std::string& pref_locale,
                                                bool crash_on_failure) {
  DCHECK_EQ(locale_resources_data_.size(), 0u) << "locale.pak already loaded";
  std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);

  // TODO(crbug.com/420947195): Fetch user's gender (behind a flag).
  const Gender gender = Gender::kDefault;

  FdAndRegion locale_pack;
  if (!LoadLocaleResourcesForLocaleAndGender(app_locale, gender, &locale_pack,
                                             &locale_resources_data_)) {
    return std::string();
  }

  std::vector<ResourceBundle::FdAndRegion>& locale_packs =
      GetLocalePaksGlobal();
  CHECK_EQ(locale_packs.size(), 0u);
  locale_packs.push_back(locale_pack);

  if (gender != Gender::kDefault) {
    if (!LoadLocaleResourcesForLocaleAndGender(app_locale, Gender::kDefault,
                                               &locale_pack,
                                               &locale_resources_data_)) {
      return std::string();
    }
    locale_packs.push_back(locale_pack);
  }

  return app_locale;
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}

void SetLocalePaksStoredInApk(bool value) {
  g_locale_paks_in_apk = value;
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
  // TODO(crbug.com/586458730): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK_GE(g_resources_pack_fd, 0);
  *out_region = g_resources_pack_region;
  return g_resources_pack_fd;
}

FileDescriptor GetCommonResourcesPackFd(
    base::MemoryMappedFile::Region* out_region) {
  CHECK_GE(g_chrome_100_percent_fd, 0, base::NotFatalUntil::M163);
  *out_region = g_chrome_100_percent_region;
  return g_chrome_100_percent_fd;
}

FileDescriptor Get200PercentResourcesPackFd(
    base::MemoryMappedFile::Region* out_region) {
  CHECK_GE(g_chrome_200_percent_fd, 0, base::NotFatalUntil::M163);
  *out_region = g_chrome_200_percent_region;
  return g_chrome_200_percent_fd;
}

const std::vector<ResourceBundle::FdAndRegion>& GetLocalePaks() {
  const std::vector<ResourceBundle::FdAndRegion>& locale_packs =
      GetLocalePaksGlobal();
  CHECK_GT(locale_packs.size(), 0u);
  return locale_packs;
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
