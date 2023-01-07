// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/common/crash_reporter/crash_reporter_client.h"

#include <stdint.h>

#include "base/android/java_exception_reporter.h"
#include "base/android/path_utils.h"
#include "base/base_paths_android.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/crashpad.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"
#include "weblayer/common/crash_reporter/crash_keys.h"
#include "weblayer/common/weblayer_paths.h"

namespace weblayer {

namespace {

class CrashReporterClientImpl : public crash_reporter::CrashReporterClient {
 public:
  CrashReporterClientImpl() = default;

  CrashReporterClientImpl(const CrashReporterClientImpl&) = delete;
  CrashReporterClientImpl& operator=(const CrashReporterClientImpl&) = delete;

  // crash_reporter::CrashReporterClient implementation.
  bool IsRunningUnattended() override { return false; }
  bool GetCollectStatsConsent() override { return false; }
  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override {
    *version = version_info::GetVersionNumber();
    *product_name = "WebLayer";
    *channel =
        version_info::GetChannelString(version_info::android::GetChannel());
  }

  bool GetCrashDumpLocation(base::FilePath* crash_dir) override {
    return base::PathService::Get(DIR_CRASH_DUMPS, crash_dir);
  }

  void GetSanitizationInformation(const char* const** crash_key_allowlist,
                                  void** target_module,
                                  bool* sanitize_stacks) override {
    *crash_key_allowlist = crash_keys::kWebLayerCrashKeyAllowList;
#if defined(COMPONENT_BUILD)
    *target_module = nullptr;
#else
    // The supplied address is used to identify the .so containing WebLayer.
    *target_module = reinterpret_cast<void*>(&EnableCrashReporter);
#endif
    *sanitize_stacks = true;
  }

  static CrashReporterClientImpl* Get() {
    static base::NoDestructor<CrashReporterClientImpl> crash_reporter_client;
    return crash_reporter_client.get();
  }
};

}  // namespace

void EnableCrashReporter(const std::string& process_type) {
  static bool enabled = false;
  DCHECK(!enabled) << "EnableCrashReporter called more than once";

  crash_reporter::SetCrashReporterClient(CrashReporterClientImpl::Get());
  crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
  if (process_type.empty())
    base::android::InitJavaExceptionReporter();
  else
    base::android::InitJavaExceptionReporterForChildProcess();
  enabled = true;
}

}  // namespace weblayer
