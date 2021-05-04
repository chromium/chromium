// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/android/metrics/weblayer_metrics_navigation_throttle.h"

#include "base/metrics/histogram_functions.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"

namespace weblayer {

WebLayerMetricsNavigationThrottle::WebLayerMetricsNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

const char* WebLayerMetricsNavigationThrottle::GetNameForLogging() {
  return "WebLayerMetricsNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
WebLayerMetricsNavigationThrottle::WillStartRequest() {
  auto* metrics_client = WebLayerMetricsServiceClient::GetInstance();
  base::UmaHistogramBoolean("WebLayer.NavigationStart.ConsentDetermined",
                            metrics_client->IsConsentDetermined());
  return content::NavigationThrottle::PROCEED;
}

}  // namespace weblayer
