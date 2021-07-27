// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_

#include "weblayer/browser/safe_browsing/safe_browsing_blocking_page.h"

class GURL;

namespace content {
class WebContents;
}

namespace weblayer {

class SafeBrowsingUIManager;

// Factory for creating SafeBrowsingBlockingPage.
class WebLayerSafeBrowsingBlockingPageFactory {
 public:
  WebLayerSafeBrowsingBlockingPageFactory() = default;
  ~WebLayerSafeBrowsingBlockingPageFactory() = default;

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResource& unsafe_resource);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
