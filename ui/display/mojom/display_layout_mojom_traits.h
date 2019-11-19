// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_LAYOUT_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_LAYOUT_MOJOM_TRAITS_H_

#include <memory>
#include <vector>

#include "ui/display/display_layout.h"
#include "ui/display/mojom/display_layout.mojom.h"

namespace mojo {

template <>
struct EnumTraits<display::mojom::Position,
                  display::DisplayPlacement::Position> {
  static display::mojom::Position ToMojom(
      display::DisplayPlacement::Position type);
  static bool FromMojom(display::mojom::Position type,
                        display::DisplayPlacement::Position* output);
};

template <>
struct EnumTraits<display::mojom::OffsetReference,
                  display::DisplayPlacement::OffsetReference> {
  static display::mojom::OffsetReference ToMojom(
      display::DisplayPlacement::OffsetReference type);
  static bool FromMojom(display::mojom::OffsetReference type,
                        display::DisplayPlacement::OffsetReference* output);
};

template <>
struct StructTraits<display::mojom::DisplayPlacementDataView,
                    display::DisplayPlacement> {
  static int64_t display_id(const display::DisplayPlacement& placement) {
    return placement.display_id;
  }

  static int64_t parent_display_id(const display::DisplayPlacement& placement) {
    return placement.parent_display_id;
  }

  static display::DisplayPlacement::Position position(
      const display::DisplayPlacement& placement) {
    return placement.position;
  }

  static int offset(const display::DisplayPlacement& placement) {
    return placement.offset;
  }

  static display::DisplayPlacement::OffsetReference offset_reference(
      const display::DisplayPlacement& placement) {
    return placement.offset_reference;
  }

  static bool Read(display::mojom::DisplayPlacementDataView data,
                   display::DisplayPlacement* out);
};

template <>
struct StructTraits<display::mojom::DisplayLayoutDataView,
                    std::unique_ptr<display::DisplayLayout>> {
  static bool default_unified(
      const std::unique_ptr<display::DisplayLayout>& layout) {
    return layout->default_unified;
  }

  static int64_t primary_display_id(
      const std::unique_ptr<display::DisplayLayout>& layout) {
    return layout->primary_id;
  }

  static const std::vector<display::DisplayPlacement>& placement_list(
      const std::unique_ptr<display::DisplayLayout>& layout) {
    return layout->placement_list;
  }

  static bool Read(display::mojom::DisplayLayoutDataView data,
                   std::unique_ptr<display::DisplayLayout>* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_MOJOM_TRAITS_H_
