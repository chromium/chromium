
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

bool IsWaylandOverlayDelegationEnabled() {
  return base::FeatureList::IsEnabled(kWaylandOverlayDelegation);
}

}  // namespace ui
