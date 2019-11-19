// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/interstitial_utils.h"

#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "weblayer/browser/ssl_blocking_page.h"
#include "weblayer/browser/tab_impl.h"

namespace weblayer {

namespace {

// Returns the security interstitial currently showing in |tab|, or null if
// there is no such interstitial.
security_interstitials::SecurityInterstitialPage*
GetCurrentlyShowingInterstitial(Tab* tab) {
  TabImpl* tab_impl = static_cast<TabImpl*>(tab);

  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab_impl->web_contents());

  return helper
             ? helper
                   ->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
             : nullptr;
}

}  // namespace

bool IsShowingSecurityInterstitial(Tab* tab) {
  return GetCurrentlyShowingInterstitial(tab) != nullptr;
}

bool IsShowingSSLInterstitial(Tab* tab) {
  auto* blocking_page = GetCurrentlyShowingInterstitial(tab);

  if (!blocking_page)
    return false;

  return blocking_page->GetTypeForTesting() == SSLBlockingPage::kTypeForTesting;
}

}  // namespace weblayer
