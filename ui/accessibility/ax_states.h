// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_STATES_H_
#define UI_ACCESSIBILITY_AX_STATES_H_

#include <stdint.h>

#include <string>

#include "base/types/strong_alias.h"
#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

// A field of state bits at positions corresponding to ax::mojom::State values.
using AXStates = base::StrongAlias<class AXStateTag, uint32_t>;

constexpr bool HasState(AXStates states, ax::mojom::State state) {
  return (states.value() & (1U << static_cast<int32_t>(state))) != 0;
}

constexpr void AddState(AXStates& states, ax::mojom::State state) {
  states.value() |= (1U << static_cast<int32_t>(state));
}

constexpr void RemoveState(AXStates& states, ax::mojom::State state) {
  states.value() &= ~(1U << static_cast<int32_t>(state));
}

AX_BASE_EXPORT std::string ToString(AXStates states);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_STATES_H_
