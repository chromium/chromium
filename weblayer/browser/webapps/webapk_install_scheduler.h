// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBAPPS_WEBAPK_INSTALL_SCHEDULER_H_
#define WEBLAYER_BROWSER_WEBAPPS_WEBAPK_INSTALL_SCHEDULER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/webapk/webapk_icon_hasher.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "weblayer/browser/webapps/weblayer_webapps_client.h"

namespace content {
class WebContents;
}  // namespace content

namespace webapps {
struct ShortcutInfo;
enum class WebApkInstallResult;
}  // namespace webapps

namespace weblayer {

class WebApkInstallSchedulerBridge;

// Class that schedules the WebAPK installation via the Chrome
// WebApkInstallCoordinatorService. Creates a self-owned
// WebApkInstallSchedulerBridge instance when building the proto is complete.
// |finish_callback| is called once the install completed or failed.
class WebApkInstallScheduler {
 public:
  // Called when the scheduling of an WebAPK-installation with the Chrome
  // service finished or failed.
  using FinishCallback = base::OnceCallback<void(webapps::WebApkInstallResult)>;

  virtual ~WebApkInstallScheduler();
  using WebApkInstallFinishedCallback =
      weblayer::WebLayerWebappsClient::WebApkInstallFinishedCallback;

  WebApkInstallScheduler(const WebApkInstallScheduler&) = delete;
  WebApkInstallScheduler& operator=(const WebApkInstallScheduler&) = delete;

  static void FetchProtoAndScheduleInstall(
      content::WebContents* web_contents,
      const webapps::ShortcutInfo& shortcut_info,
      const SkBitmap& primary_icon,
      bool is_primary_icon_maskable,
      WebApkInstallFinishedCallback callback);

  void FetchProtoAndScheduleInstallForTesting(
      content::WebContents* web_contents);

  static bool IsInstallServiceAvailable();

 private:
  WebApkInstallScheduler(const webapps::ShortcutInfo& shortcut_info,
                         const SkBitmap& primary_icon,
                         bool is_primary_icon_maskable,
                         WebApkInstallFinishedCallback callback);

  friend class TestWebApkInstallScheduler;

  void FetchMurmur2Hashes(content::WebContents* web_contents);

  void OnGotIconMurmur2HashesBuildProto(
      absl::optional<std::map<std::string, webapps::WebApkIconHasher::Icon>>
          hashes);

  virtual void ScheduleWithChrome(
      std::unique_ptr<std::string> serialized_proto);

  virtual void OnResult(webapps::WebApkInstallResult result);

  WebApkInstallFinishedCallback webapps_client_callback_;
  std::unique_ptr<webapps::ShortcutInfo> shortcut_info_;
  const SkBitmap primary_icon_;
  bool is_primary_icon_maskable_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstallScheduler> weak_ptr_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBAPPS_WEBAPK_INSTALL_SCHEDULER_H_
