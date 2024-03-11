// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/metrics/wolvic_enabled_state_provider.h"

namespace wolvic {

bool WolvicEnabledStateProvider::IsConsentGiven() const {
  return false;
}

bool WolvicEnabledStateProvider::IsReportingEnabled() const {
  return false;
}

}  // namespace wolvic
