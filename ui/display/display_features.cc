// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_features.h"

#include "build/build_config.h"

namespace display {
namespace features {

#if defined(OS_CHROMEOS)
// Enables using HDR transfer function if the monitor says it supports it.
const base::Feature kUseHDRTransferFunction {
  "UseHDRTransferFunction",
  // TODO(b/168843009): Temporarily disable on ARM while investigating.
#if defined(ARCH_CPU_ARM_FAMILY)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};
#endif

// This features allows listing all display modes of external displays in the
// display settings and setting any one of them exactly as requested, which can
// be very useful for debugging and development purposes.
const base::Feature kListAllDisplayModes = {"ListAllDisplayModes",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

bool IsListAllDisplayModesEnabled() {
  return base::FeatureList::IsEnabled(kListAllDisplayModes);
}

}  // namespace features
}  // namespace display
