// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_ERROR_NAVIGATION_THROTTLE_H_
#define WEBLAYER_BROWSER_NAVIGATION_ERROR_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace weblayer {

// NavigationThrottle implementation that allows the embedder to inject an
// error page for non-ssl errors.
class NavigationErrorNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit NavigationErrorNavigationThrottle(content::NavigationHandle* handle);
  NavigationErrorNavigationThrottle(const NavigationErrorNavigationThrottle&) =
      delete;
  NavigationErrorNavigationThrottle& operator=(
      const NavigationErrorNavigationThrottle&) = delete;
  ~NavigationErrorNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillFailRequest() override;
  const char* GetNameForLogging() override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_ERROR_NAVIGATION_THROTTLE_H_
