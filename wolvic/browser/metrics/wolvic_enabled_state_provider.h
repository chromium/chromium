// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_METRICS_WOLVIC_ENABLED_STATE_PROVIDER_H_
#define WOLVIC_BROWSER_METRICS_WOLVIC_ENABLED_STATE_PROVIDER_H_

#include "components/metrics/enabled_state_provider.h"

namespace wolvic {

class WolvicEnabledStateProvider : public metrics::EnabledStateProvider {
 public:
  WolvicEnabledStateProvider() = default;

  WolvicEnabledStateProvider(const WolvicEnabledStateProvider&) = delete;
  WolvicEnabledStateProvider& operator=(const WolvicEnabledStateProvider&) =
      delete;

  ~WolvicEnabledStateProvider() override = default;

  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_METRICS_WOLVIC_ENABLED_STATE_PROVIDER_H_
