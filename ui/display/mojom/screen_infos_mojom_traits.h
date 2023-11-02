// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_SCREEN_INFOS_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_SCREEN_INFOS_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "ui/display/mojom/screen_infos.mojom-shared.h"
#include "ui/display/screen_infos.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(DISPLAY_SHARED_MOJOM_TRAITS)
    StructTraits<display::mojom::ScreenInfosDataView, display::ScreenInfos> {
  static const std::vector<display::ScreenInfo>& screen_infos(
      const display::ScreenInfos& r) {
    // Ensure ScreenInfo ids are unique and contain `current_display_id`.
    base::flat_set<int64_t> display_ids;
    display_ids.reserve(r.screen_infos.size());
    for (const display::ScreenInfo& screen_info : r.screen_infos)
      CHECK(display_ids.insert(screen_info.display_id).second);
    CHECK(display_ids.find(r.current_display_id) != display_ids.end());
    return r.screen_infos;
  }

  static int64_t current_display_id(const display::ScreenInfos& r) {
    // One `r.screen_infos` item must have a matching `display_id`; see above.
    return r.current_display_id;
  }

  static const gfx::Size& system_cursor_size(const display::ScreenInfos& r) {
    return r.system_cursor_size;
  }

  static bool Read(display::mojom::ScreenInfosDataView r,
                   display::ScreenInfos* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_SCREEN_INFOS_MOJOM_TRAITS_H_
