// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_COMPUTE_ATTRIBUTES_H_
#define UI_ACCESSIBILITY_PLATFORM_COMPUTE_ATTRIBUTES_H_

#include <cstddef>

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

class AXPlatformNodeDelegate;

// Compute the attribute value instead of returning the "raw" attribute value
// for those attributes that have computation methods.
COMPONENT_EXPORT(AX_PLATFORM)
absl::optional<int32_t> ComputeAttribute(
    const ui::AXPlatformNodeDelegate* delegate,
    ax::mojom::IntAttribute attribute);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_COMPUTE_ATTRIBUTES_H_
