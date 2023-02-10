// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_mode_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXModeDataView, ui::AXMode>::Read(
    ax::mojom::AXModeDataView data,
    ui::AXMode* out) {
  // This is a bit field. Check that flags fall within accepted bounds.
  if (data.flags() >= ui::AXMode::kLastModeFlag * 2) {
    return false;
  }

  out->flags_ = data.flags();

  // This is a bit field. Check that flags fall within accepted bounds.
  if (data.experimental_flags() >= ui::AXMode::kLastModeFlag * 2) {
    return false;
  }

  out->experimental_flags_ = data.experimental_flags();
  return true;
}

}  // namespace mojo
