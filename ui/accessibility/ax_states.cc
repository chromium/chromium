// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_states.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

std::string ToString(AXStates states) {
  std::string result;
  for (uint32_t i = static_cast<uint32_t>(ax::mojom::State::kNone) + 1;
       i <= static_cast<uint32_t>(ax::mojom::State::kMaxValue); ++i) {
    if (HasState(states, static_cast<ax::mojom::State>(i))) {
      base::StrAppend(&result, {" ", base::ToUpperASCII(ToString(
                                         static_cast<ax::mojom::State>(i)))});
    }
  }
  return result;
}

}  // namespace ui
