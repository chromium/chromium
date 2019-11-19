// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/content/app/crashpad.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"
#include "weblayer/common/weblayer_paths.h"

namespace weblayer {

namespace {

class CrashReporterClientImpl : public crash_reporter::CrashReporterClient {
 public:
  CrashReporterClientImpl() = default;

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
    return base::PathService::Get(weblayer::DIR_CRASH_DUMPS, crash_dir);
  }

  void GetSanitizationInformation(const char* const** annotations_whitelist,
                                  void** target_module,
                                  bool* sanitize_stacks) override {
    // TODO(tobiasjs) implement appropriate crash filtering.
    *annotations_whitelist = nullptr;
    *target_module = nullptr;
    *sanitize_stacks = false;
  }

  static CrashReporterClientImpl* Get() {
    static base::NoDestructor<CrashReporterClientImpl> crash_reporter_client;
    return crash_reporter_client.get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashReporterClientImpl);
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
