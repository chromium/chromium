// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/screen_infos_mojom_traits.h"

#include "ui/display/mojom/screen_info_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"

namespace mojo {

// static
bool StructTraits<display::mojom::ScreenInfosDataView, display::ScreenInfos>::
    Read(display::mojom::ScreenInfosDataView data, display::ScreenInfos* out) {
  if (!data.ReadScreenInfos(&out->screen_infos) ||
      !data.ReadSystemCursorSize(&out->system_cursor_size))
    return false;
  out->current_display_id = data.current_display_id();

  // Ensure ScreenInfo ids are unique and contain the `current_display_id`.
  base::flat_set<int64_t> display_ids;
  display_ids.reserve(out->screen_infos.size());
  for (const auto& screen_info : out->screen_infos) {
    if (!display_ids.insert(screen_info.display_id).second)
      return false;
  }
  return display_ids.find(out->current_display_id) != display_ids.end();
}

}  // namespace mojo
