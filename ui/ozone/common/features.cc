
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/features.h"
#include "build/chromeos_buildflags.h"

namespace ui {

const base::Feature kWaylandOverlayDelegation {
  "WaylandOverlayDelegation",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// This feature flag is used for fractional display scale factor development for
// LaCros. When enabled, the wayland client would use the xdg output protocol to
// receive extra output metrics (logical size) to calculate the scale factor.
const base::Feature kXdgOutputProtocolSupport{
    "XdgOutputProtocolSupport", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsWaylandOverlayDelegationEnabled() {
  return base::FeatureList::IsEnabled(kWaylandOverlayDelegation);
}

bool IsXdgOutputProtocolSupportEnabled() {
  return base::FeatureList::IsEnabled(kXdgOutputProtocolSupport);
}

}  // namespace ui
