// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_layout_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

display::mojom::Position
EnumTraits<display::mojom::Position, display::DisplayPlacement::Position>::
    ToMojom(display::DisplayPlacement::Position rotation) {
  switch (rotation) {
    case display::DisplayPlacement::TOP:
      return display::mojom::Position::TOP;
    case display::DisplayPlacement::RIGHT:
      return display::mojom::Position::RIGHT;
    case display::DisplayPlacement::BOTTOM:
      return display::mojom::Position::BOTTOM;
    case display::DisplayPlacement::LEFT:
      return display::mojom::Position::LEFT;
  }
  NOTREACHED();
}

display::DisplayPlacement::Position
EnumTraits<display::mojom::Position, display::DisplayPlacement::Position>::
    FromMojom(display::mojom::Position rotation) {
  switch (rotation) {
    case display::mojom::Position::TOP:
      return display::DisplayPlacement::TOP;
    case display::mojom::Position::RIGHT:
      return display::DisplayPlacement::RIGHT;
    case display::mojom::Position::BOTTOM:
      return display::DisplayPlacement::BOTTOM;
    case display::mojom::Position::LEFT:
      return display::DisplayPlacement::LEFT;
  }
  NOTREACHED();
}

display::mojom::OffsetReference
EnumTraits<display::mojom::OffsetReference,
           display::DisplayPlacement::OffsetReference>::
    ToMojom(display::DisplayPlacement::OffsetReference rotation) {
  switch (rotation) {
    case display::DisplayPlacement::TOP_LEFT:
      return display::mojom::OffsetReference::TOP_LEFT;
    case display::DisplayPlacement::BOTTOM_RIGHT:
      return display::mojom::OffsetReference::BOTTOM_RIGHT;
  }
  NOTREACHED();
}

display::DisplayPlacement::OffsetReference
EnumTraits<display::mojom::OffsetReference,
           display::DisplayPlacement::OffsetReference>::
    FromMojom(display::mojom::OffsetReference rotation) {
  switch (rotation) {
    case display::mojom::OffsetReference::TOP_LEFT:
      return display::DisplayPlacement::TOP_LEFT;
    case display::mojom::OffsetReference::BOTTOM_RIGHT:
      return display::DisplayPlacement::BOTTOM_RIGHT;
  }
  NOTREACHED();
}

bool StructTraits<display::mojom::DisplayPlacementDataView,
                  display::DisplayPlacement>::
    Read(display::mojom::DisplayPlacementDataView data,
         display::DisplayPlacement* out) {
  out->display_id = data.display_id();
  out->parent_display_id = data.parent_display_id();
  out->offset = data.offset();

  if (!data.ReadPosition(&out->position))
    return false;

  if (!data.ReadOffsetReference(&out->offset_reference))
    return false;

  return true;
}

bool StructTraits<display::mojom::DisplayLayoutDataView,
                  std::unique_ptr<display::DisplayLayout>>::
    Read(display::mojom::DisplayLayoutDataView data,
         std::unique_ptr<display::DisplayLayout>* out) {
  auto display_layout = std::make_unique<display::DisplayLayout>();

  if (!data.ReadPlacementList(&display_layout->placement_list))
    return false;

  display_layout->default_unified = data.default_unified();
  display_layout->primary_id = data.primary_display_id();

  *out = std::move(display_layout);

  return true;
}

}  // namespace mojo
