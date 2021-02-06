// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_HELPER_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace weblayer {
class SafeBrowsingUIManager;

// This observer creates a blocking page for the web contents if any subresource
// triggered a safe browsing interstitial. Main frame safe browsing errors are
// handled separately (in SafeBrowsingNavigationThrottle).
class SafeBrowsingSubresourceHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SafeBrowsingSubresourceHelper> {
 public:
  ~SafeBrowsingSubresourceHelper() override;

  // WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit SafeBrowsingSubresourceHelper(content::WebContents* web_contents,
                                         SafeBrowsingUIManager* ui_manager);
  friend class content::WebContentsUserData<SafeBrowsingSubresourceHelper>;

  SafeBrowsingUIManager* ui_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingSubresourceHelper);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_HELPER_H_
