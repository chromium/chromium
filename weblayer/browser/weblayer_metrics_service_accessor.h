// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_METRICS_SERVICE_ACCESSOR_H_
#define WEBLAYER_BROWSER_WEBLAYER_METRICS_SERVICE_ACCESSOR_H_

#include "components/metrics/metrics_service_accessor.h"

namespace weblayer {

// This class gives and documents access to metrics::MetricsServiceAccessor
// methods. Since these methods are protected in the base case, each user has to
// be explicitly declared as a 'friend' below.
class WebLayerMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 private:
  friend class WebLayerSafeBrowsingUIManagerDelegate;
  friend class WebLayerAssistantFieldTrialUtil;

  WebLayerMetricsServiceAccessor() = delete;
  WebLayerMetricsServiceAccessor(const WebLayerMetricsServiceAccessor&) =
      delete;
  WebLayerMetricsServiceAccessor& operator=(
      const WebLayerMetricsServiceAccessor&) = delete;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_METRICS_SERVICE_ACCESSOR_H_
