// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_TAB_OBSERVER_DELEGATE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_TAB_OBSERVER_DELEGATE_H_

#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"

namespace weblayer {

// Provides embedder-specific logic for SafeBrowsingTabObserver.
class WebLayerSafeBrowsingTabObserverDelegate
    : public safe_browsing::SafeBrowsingTabObserver::Delegate {
 public:
  WebLayerSafeBrowsingTabObserverDelegate();
  ~WebLayerSafeBrowsingTabObserverDelegate() override;

  WebLayerSafeBrowsingTabObserverDelegate(
      const WebLayerSafeBrowsingTabObserverDelegate&) = delete;
  WebLayerSafeBrowsingTabObserverDelegate& operator=(
      const WebLayerSafeBrowsingTabObserverDelegate&) = delete;

  // SafeBrowsingTabObserver::Delegate:
  PrefService* GetPrefs(content::BrowserContext* browser_context) override;
  safe_browsing::ClientSideDetectionService*
  GetClientSideDetectionServiceIfExists(
      content::BrowserContext* browser_context) override;
  bool DoesSafeBrowsingServiceExist() override;
  std::unique_ptr<safe_browsing::ClientSideDetectionHost>
  CreateClientSideDetectionHost(content::WebContents* web_contents) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_TAB_OBSERVER_DELEGATE_H_
