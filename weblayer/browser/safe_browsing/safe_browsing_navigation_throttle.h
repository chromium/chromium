// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace weblayer {
class SafeBrowsingUIManager;

class SafeBrowsingNavigationThrottle : public content::NavigationThrottle {
 public:
  SafeBrowsingNavigationThrottle(content::NavigationHandle* handle,
                                 SafeBrowsingUIManager* ui_manager);
  ~SafeBrowsingNavigationThrottle() override {}
  const char* GetNameForLogging() override;
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;

 private:
  SafeBrowsingUIManager* ui_manager_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_THROTTLE_H_