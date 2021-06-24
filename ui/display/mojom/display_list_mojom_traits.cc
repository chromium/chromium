// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_list_mojom_traits.h"

#include "ui/display/mojom/display.mojom.h"

namespace mojo {

// static
bool StructTraits<display::mojom::DisplayListDataView, display::DisplayList>::
    Read(display::mojom::DisplayListDataView data, display::DisplayList* out) {
  std::vector<display::Display> displays;
  if (!data.ReadDisplays(&displays))
    return false;
  *out = display::DisplayList(displays, data.primary_id(), data.current_id());
  return out->IsValidOrEmpty();
}

}  // namespace mojo
