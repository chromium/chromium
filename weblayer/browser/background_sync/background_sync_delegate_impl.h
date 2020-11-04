// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_

#include "build/build_config.h"
#include "components/background_sync/background_sync_delegate.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace weblayer {

// WebLayer's customization of the logic in components/background_sync.
class BackgroundSyncDelegateImpl
    : public background_sync::BackgroundSyncDelegate {
 public:
  explicit BackgroundSyncDelegateImpl(content::BrowserContext* browser_context);
  ~BackgroundSyncDelegateImpl() override;

  void GetUkmSourceId(const url::Origin& origin,
                      base::OnceCallback<void(base::Optional<ukm::SourceId>)>
                          callback) override;
  void Shutdown() override;
  HostContentSettingsMap* GetHostContentSettingsMap() override;
  bool IsProfileOffTheRecord() override;
  void NoteSuspendedPeriodicSyncOrigins(
      std::set<url::Origin> suspended_origins) override;
  int GetSiteEngagementPenalty(const GURL& url) override;
#if defined(OS_ANDROID)
  void ScheduleBrowserWakeUpWithDelay(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta delay) override;
  void CancelBrowserWakeup(blink::mojom::BackgroundSyncType sync_type) override;
  bool ShouldDisableBackgroundSync() override;
  bool ShouldDisableAndroidNetworkDetection() override;
#endif  // defined(OS_ANDROID)

 private:
  content::BrowserContext* browser_context_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_