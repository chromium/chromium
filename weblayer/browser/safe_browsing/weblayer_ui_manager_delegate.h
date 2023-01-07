// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_UI_MANAGER_DELEGATE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_UI_MANAGER_DELEGATE_H_

#include "components/safe_browsing/content/browser/ui_manager.h"

namespace weblayer {

// Provides embedder-specific logic for SafeBrowsingUIManager.
class WebLayerSafeBrowsingUIManagerDelegate
    : public safe_browsing::SafeBrowsingUIManager::Delegate {
 public:
  WebLayerSafeBrowsingUIManagerDelegate();
  ~WebLayerSafeBrowsingUIManagerDelegate() override;

  WebLayerSafeBrowsingUIManagerDelegate(
      const WebLayerSafeBrowsingUIManagerDelegate&) = delete;
  WebLayerSafeBrowsingUIManagerDelegate& operator=(
      const WebLayerSafeBrowsingUIManagerDelegate&) = delete;

  // safe_browsing::SafeBrowsingUIManager::Delegate:
  std::string GetApplicationLocale() override;
  void TriggerSecurityInterstitialShownExtensionEventIfDesired(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& reason,
      int net_error_code) override;
  void TriggerSecurityInterstitialProceededExtensionEventIfDesired(
      content::WebContents* web_contents,
      const GURL& page_url,
      const std::string& reason,
      int net_error_code) override;
  prerender::NoStatePrefetchContents* GetNoStatePrefetchContentsIfExists(
      content::WebContents* web_contents) override;
  bool IsHostingExtension(content::WebContents* web_contents) override;
  PrefService* GetPrefs(content::BrowserContext* browser_context) override;
  history::HistoryService* GetHistoryService(
      content::BrowserContext* browser_context) override;
  safe_browsing::PingManager* GetPingManager(
      content::BrowserContext* browser_context) override;
  bool IsMetricsAndCrashReportingEnabled() override;
  bool IsSendingOfHitReportsEnabled() override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_UI_MANAGER_DELEGATE_H_
