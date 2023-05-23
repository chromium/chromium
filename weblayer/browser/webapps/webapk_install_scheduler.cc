// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webapps/webapk_install_scheduler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_icon_hasher.h"
#include "components/webapps/browser/android/webapk/webapk_proto_builder.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/android/webapps_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"
#include "weblayer/browser/webapps/webapk_install_scheduler_bridge.h"

namespace weblayer {

WebApkInstallScheduler::WebApkInstallScheduler(
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    WebApkInstallFinishedCallback callback)
    : webapps_client_callback_(std::move(callback)),
      primary_icon_(primary_icon) {
  shortcut_info_ = std::make_unique<webapps::ShortcutInfo>(shortcut_info);
}

WebApkInstallScheduler::~WebApkInstallScheduler() = default;

// static
void WebApkInstallScheduler::FetchProtoAndScheduleInstall(
    content::WebContents* web_contents,
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    WebApkInstallFinishedCallback callback) {
  // Self owned WebApkInstallScheduler that destroys itself as soon as its
  // OnResult function is called when the scheduled installation failed or
  // finished.
  WebApkInstallScheduler* scheduler = new WebApkInstallScheduler(
      shortcut_info, primary_icon, std::move(callback));
  scheduler->FetchMurmur2Hashes(web_contents);
}

void WebApkInstallScheduler::FetchProtoAndScheduleInstallForTesting(
    content::WebContents* web_contents) {
  FetchMurmur2Hashes(web_contents);
}

void WebApkInstallScheduler::FetchMurmur2Hashes(
    content::WebContents* web_contents) {
  // We need to take the hash of the bitmap at the icon URL prior to any
  // transformations being applied to the bitmap (such as encoding/decoding
  // the bitmap). The icon hash is used to determine whether the icon that
  // the user sees matches the icon of a WebAPK that the WebAPK server
  // generated for another user. (The icon can be dynamically generated.)
  //
  // We redownload the icon in order to take the Murmur2 hash. The redownload
  // should be fast because the icon should be in the HTTP cache.
  std::vector<webapps::WebappIcon> icons = shortcut_info_->GetWebApkIcons();

  webapps::WebApkIconHasher::DownloadAndComputeMurmur2Hash(
      web_contents->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      web_contents->GetWeakPtr(), url::Origin::Create(shortcut_info_->url),
      icons,
      base::BindOnce(&WebApkInstallScheduler::OnGotIconMurmur2HashesBuildProto,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstallScheduler::OnGotIconMurmur2HashesBuildProto(
    absl::optional<std::map<std::string, webapps::WebApkIconHasher::Icon>>
        hashes) {
  if (!hashes) {
    OnResult(webapps::WebApkInstallResult::ICON_HASHER_ERROR);
    return;
  }

  webapps::BuildProto(
      *shortcut_info_.get(), shortcut_info_->manifest_id,
      std::string() /* primary_icon_data */,
      std::string() /* splash_icon_data */, "" /* package_name */,
      "" /* version */, std::move(*hashes), false /* is_manifest_stale */,
      false /* is_app_identity_update_supported */,
      base::BindOnce(&WebApkInstallScheduler::ScheduleWithChrome,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstallScheduler::ScheduleWithChrome(
    std::unique_ptr<std::string> serialized_proto) {
  WebApkInstallSchedulerBridge::ScheduleWebApkInstallWithChrome(
      std::move(serialized_proto), primary_icon_,
      shortcut_info_->is_primary_icon_maskable,
      base::BindOnce(&WebApkInstallScheduler::OnResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstallScheduler::OnResult(webapps::WebApkInstallResult result) {
  // Toasts have to be called on the UI thread, but the
  // WebApkInstallSchedulerClient already makes sure that the callback, which is
  // triggered by the Chrome-service, is invoked on the UI thread.
  webapps::WebappsUtils::ShowWebApkInstallResultToast(result);

  std::move(webapps_client_callback_).Run(shortcut_info_->manifest_id);
  delete this;
}

// static
bool WebApkInstallScheduler::IsInstallServiceAvailable() {
  return WebApkInstallSchedulerBridge::IsInstallServiceAvailable();
}

}  // namespace weblayer
