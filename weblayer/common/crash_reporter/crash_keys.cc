// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/common/crash_reporter/crash_keys.h"

#include "base/android/build_info.h"
#include "base/strings/string_number_conversions.h"
#include "components/crash/core/common/crash_key.h"

namespace weblayer {
namespace crash_keys {

const char kAppPackageName[] = "app-package-name";
const char kAppPackageVersionCode[] = "app-package-version-code";

const char kAndroidSdkInt[] = "android-sdk-int";

const char kWeblayerWebViewCompatMode[] = "WEBLAYER_WEB_VIEW_COMPAT_MODE";

// clang-format off
const char* const kWebLayerCrashKeyAllowList[] = {
    kAppPackageName, kAppPackageVersionCode, kAndroidSdkInt,
    kWeblayerWebViewCompatMode,

    // process type
    "ptype",

    // Java exception stack traces
    "exception_info",

    // gpu
    "gpu-driver", "gpu-psver", "gpu-vsver", "gpu-gl-vendor", "gpu-gl-renderer",
    "oop_read_failure", "gpu-gl-error-message",

    // content/:
    "bad_message_reason", "discardable-memory-allocated",
    "discardable-memory-free", "mojo-message-error",
    "total-discardable-memory-allocated",

    // crash keys needed for recording finch trials
    "variations", "num-experiments",

    nullptr};
// clang-format on

}  // namespace crash_keys

void SetWebLayerCrashKeys() {
  base::android::BuildInfo* android_build_info =
      base::android::BuildInfo::GetInstance();

  static ::crash_reporter::CrashKeyString<64> app_name_key(
      crash_keys::kAppPackageName);
  app_name_key.Set(android_build_info->host_package_name());

  static ::crash_reporter::CrashKeyString<64> app_version_key(
      crash_keys::kAppPackageVersionCode);
  app_version_key.Set(android_build_info->host_version_code());

  static ::crash_reporter::CrashKeyString<8> sdk_int_key(
      crash_keys::kAndroidSdkInt);
  sdk_int_key.Set(base::NumberToString(android_build_info->sdk_int()));
}

}  // namespace weblayer
