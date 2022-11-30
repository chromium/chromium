// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_

#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"

namespace weblayer {

// Factory for creating SafeBrowsingBlockingPage.
class WebLayerSafeBrowsingBlockingPageFactory
    : public safe_browsing::SafeBrowsingBlockingPageFactory {
 public:
  WebLayerSafeBrowsingBlockingPageFactory() = default;
  ~WebLayerSafeBrowsingBlockingPageFactory() override = default;

  // safe_browsing::SafeBrowsingBlockingPageFactory:
  safe_browsing::SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      safe_browsing::BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList&
          unsafe_resources,
      bool should_trigger_reporting) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
