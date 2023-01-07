// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/background_sync/background_sync_delegate_impl.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

BackgroundSyncDelegateImpl::BackgroundSyncDelegateImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

BackgroundSyncDelegateImpl::~BackgroundSyncDelegateImpl() = default;

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<content::BackgroundSyncController::BackgroundSyncEventKeepAlive>
BackgroundSyncDelegateImpl::CreateBackgroundSyncEventKeepAlive() {
  return nullptr;
}
#endif  // !BUILDFLAG(IS_ANDROID)

void BackgroundSyncDelegateImpl::GetUkmSourceId(
    const url::Origin& origin,
    base::OnceCallback<void(absl::optional<ukm::SourceId>)> callback) {
  // The exact URL which registered the Background Sync event is not saved,
  // and the current main frame URL might not correspond to |origin|. Thus, we
  // associate a new source ID with the origin.
  // The only way WebLayer can lose history information is through the
  // clearBrowsingHistory() API which also deletes all existing service workers.
  // Therefore, if this method is called, it's safe to assume that the origin
  // associated with the Background Sync registration is in WebLayer's browsing
  // history. It's okay to log UKM for it.
  ukm::SourceId source_id = ukm::ConvertToSourceId(
      ukm::AssignNewSourceId(), ukm::SourceIdType::HISTORY_ID);
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);
  recorder->UpdateSourceURL(source_id, origin.GetURL());

  std::move(callback).Run(source_id);
}

void BackgroundSyncDelegateImpl::Shutdown() {
  // Clear the BrowserContext as we're not supposed to use it anymore.
  browser_context_ = nullptr;
}

HostContentSettingsMap*
BackgroundSyncDelegateImpl::GetHostContentSettingsMap() {
  return HostContentSettingsMapFactory::GetForBrowserContext(browser_context_);
}

bool BackgroundSyncDelegateImpl::IsProfileOffTheRecord() {
  DCHECK(browser_context_);
  return browser_context_->IsOffTheRecord();
}

void BackgroundSyncDelegateImpl::NoteSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  // TODO(crbug.com/1091211): Consider site engagement when adding support for
  // Periodic Background Sync.
}

int BackgroundSyncDelegateImpl::GetSiteEngagementPenalty(const GURL& url) {
  // TODO(crbug.com/1091211): Consider site engagement when adding support for
  // Periodic Background Sync.
  return 0;
}

#if BUILDFLAG(IS_ANDROID)

void BackgroundSyncDelegateImpl::ScheduleBrowserWakeUpWithDelay(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay) {
  // TODO(crbug.com/1087486, 1091211): Add logic to wake up the browser.
}

void BackgroundSyncDelegateImpl::CancelBrowserWakeup(
    blink::mojom::BackgroundSyncType sync_type) {
  // TODO(crbug.com/1087486, 1091211): Add logic to wake up the browser.
}

bool BackgroundSyncDelegateImpl::ShouldDisableBackgroundSync() {
  // TODO(crbug.com/1087486, 1091211): Add logic here if we need to support
  // Android L.
  return false;
}

bool BackgroundSyncDelegateImpl::ShouldDisableAndroidNetworkDetection() {
  // TODO(crbug.com/1141778): Remove this once waking up the WebLayer
  // embedder is supported.
  return true;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace weblayer
