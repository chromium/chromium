// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_subresource_helper.h"

#include "base/memory/ptr_util.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "weblayer/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

namespace weblayer {

SafeBrowsingSubresourceHelper::~SafeBrowsingSubresourceHelper() = default;

void SafeBrowsingSubresourceHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() == net::ERR_BLOCKED_BY_CLIENT) {
    if (!ui_manager_)
      return;

    security_interstitials::UnsafeResource resource;
    if (ui_manager_->PopUnsafeResourceForURL(navigation_handle->GetURL(),
                                             &resource)) {
      SafeBrowsingBlockingPage* blocking_page =
          SafeBrowsingBlockingPage::CreateBlockingPage(
              ui_manager_, navigation_handle->GetWebContents(),
              navigation_handle->GetURL(), resource);
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(navigation_handle->GetWebContents(),
                                navigation_handle->GetNavigationId(),
                                base::WrapUnique(blocking_page));
    }
  }
}

SafeBrowsingSubresourceHelper::SafeBrowsingSubresourceHelper(
    content::WebContents* web_contents,
    SafeBrowsingUIManager* ui_manager)
    : WebContentsObserver(web_contents), ui_manager_(ui_manager) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SafeBrowsingSubresourceHelper)

}  // namespace weblayer
