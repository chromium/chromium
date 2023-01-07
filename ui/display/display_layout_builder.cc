// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_layout_builder.h"

#include <algorithm>

#include "ui/display/display.h"
#include "ui/display/util/display_util.h"

namespace display {

DisplayLayoutBuilder::DisplayLayoutBuilder(const DisplayLayout& layout)
    : layout_(layout.Copy()) {}

DisplayLayoutBuilder::DisplayLayoutBuilder(int64_t primary_id)
    : layout_(new DisplayLayout) {
  layout_->primary_id = primary_id;
}

DisplayLayoutBuilder::~DisplayLayoutBuilder() {}

DisplayLayoutBuilder& DisplayLayoutBuilder::SetDefaultUnified(
    bool default_unified) {
  layout_->default_unified = default_unified;
  return *this;
}

DisplayLayoutBuilder& DisplayLayoutBuilder::ClearPlacements() {
  layout_->placement_list.clear();
  return *this;
}

DisplayLayoutBuilder& DisplayLayoutBuilder::AddDisplayPlacement(
    int64_t display_id,
    int64_t parent_display_id,
    DisplayPlacement::Position position,
    int offset) {
  DisplayPlacement placement;
  placement.position = position;
  placement.offset = offset;
  placement.display_id = display_id;
  placement.parent_display_id = parent_display_id;
  AddDisplayPlacement(placement);
  return *this;
}

DisplayLayoutBuilder& DisplayLayoutBuilder::AddDisplayPlacement(
    const DisplayPlacement& placement) {
  layout_->placement_list.push_back(placement);
  return *this;
}

DisplayLayoutBuilder& DisplayLayoutBuilder::SetSecondaryPlacement(
    int64_t secondary_id,
    DisplayPlacement::Position position,
    int offset) {
  layout_->placement_list.clear();
  AddDisplayPlacement(secondary_id, layout_->primary_id, position, offset);
  return *this;
}

std::unique_ptr<DisplayLayout> DisplayLayoutBuilder::Build() {
  std::sort(layout_->placement_list.begin(), layout_->placement_list.end(),
            [](const DisplayPlacement& a, const DisplayPlacement& b) {
              return CompareDisplayIds(a.display_id, b.display_id);
            });
  return std::move(layout_);
}

}  // namespace display
