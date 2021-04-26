// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_NAVIGATION_THROTTLE_H_
#define WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace weblayer {

// Logs WebLayer specific metrics.
class WebLayerMetricsNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit WebLayerMetricsNavigationThrottle(content::NavigationHandle* handle);
  WebLayerMetricsNavigationThrottle(const WebLayerMetricsNavigationThrottle&) =
      delete;
  WebLayerMetricsNavigationThrottle& operator=(
      const WebLayerMetricsNavigationThrottle&) = delete;
  ~WebLayerMetricsNavigationThrottle() override = default;

  // content::NavigationThrottle:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_NAVIGATION_THROTTLE_H_
