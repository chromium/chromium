// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_MOJOM_AX_LOCATION_AND_SCROLL_UPDATES_MOJOM_TRAITS_H_
#define UI_ACCESSIBILITY_MOJOM_AX_LOCATION_AND_SCROLL_UPDATES_MOJOM_TRAITS_H_

#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/mojom/ax_location_and_scroll_updates.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXLocationChangeDataView, ui::AXLocationChange> {
  static int32_t id(const ui::AXLocationChange& p) { return p.id; }
  static const ui::AXRelativeBounds& new_location(
      const ui::AXLocationChange& p) {
    return p.new_location;
  }
  static bool Read(ax::mojom::AXLocationChangeDataView data,
                   ui::AXLocationChange* out);
};

template <>
struct StructTraits<ax::mojom::AXScrollChangeDataView, ui::AXScrollChange> {
  static int32_t id(const ui::AXScrollChange& p) { return p.id; }
  static int32_t scroll_x(const ui::AXScrollChange& p) { return p.scroll_x; }
  static int32_t scroll_y(const ui::AXScrollChange& p) { return p.scroll_y; }
  static bool Read(ax::mojom::AXScrollChangeDataView data,
                   ui::AXScrollChange* out);
};

template <>
struct StructTraits<ax::mojom::AXLocationAndScrollUpdatesDataView,
                    ui::AXLocationAndScrollUpdates> {
  static const std::vector<ui::AXLocationChange>& location_changes(
      const ui::AXLocationAndScrollUpdates& p) {
    return p.location_changes;
  }
  static const std::vector<ui::AXScrollChange>& scroll_changes(
      const ui::AXLocationAndScrollUpdates& p) {
    return p.scroll_changes;
  }
  static bool Read(ax::mojom::AXLocationAndScrollUpdatesDataView data,
                   ui::AXLocationAndScrollUpdates* out);
};

}  // namespace mojo

#endif  // UI_ACCESSIBILITY_MOJOM_AX_LOCATION_AND_SCROLL_UPDATES_MOJOM_TRAITS_H_
