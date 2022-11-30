// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBAPPS_WEBAPK_INSTALL_SCHEDULER_BRIDGE_H_
#define WEBLAYER_BROWSER_WEBAPPS_WEBAPK_INSTALL_SCHEDULER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "components/webapps/browser/android/webapk/webapk_icon_hasher.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "weblayer/browser/webapps/webapk_install_scheduler.h"

namespace webapps {
enum class WebApkInstallResult;
}  // namespace webapps

namespace weblayer {

// The native WebApkInstallSchedulerBridge owns itself, and deletes itself and
// its Java counterpart when finished.
class WebApkInstallSchedulerBridge {
 public:
  using FinishCallback = WebApkInstallScheduler::FinishCallback;

  ~WebApkInstallSchedulerBridge();

  WebApkInstallSchedulerBridge(const WebApkInstallSchedulerBridge&) = delete;
  WebApkInstallSchedulerBridge& operator=(const WebApkInstallSchedulerBridge&) =
      delete;

  static void ScheduleWebApkInstallWithChrome(
      std::unique_ptr<std::string> serialized_proto,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable,
      FinishCallback finish_callback);

  static bool IsInstallServiceAvailable();

  void OnInstallFinished(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint result);

 private:
  WebApkInstallSchedulerBridge(FinishCallback finish_callback);

  void ScheduleWebApkInstallWithChrome(
      std::unique_ptr<std::string> serialized_proto,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable);

  FinishCallback finish_callback_;

  // Points to the Java Object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBAPPS_WEBAPK_INSTALL_SCHEDULER_BRIDGE_H_
