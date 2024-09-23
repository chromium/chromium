// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/util/display_manager_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"

namespace display {

DisplayLayoutStore::DisplayLayoutStore()
    : default_display_placement_(DisplayPlacement::RIGHT, 0) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSecondaryDisplayLayout)) {
    std::string value =
        command_line->GetSwitchValueASCII(switches::kSecondaryDisplayLayout);
    char layout;
    int offset = 0;
    if (sscanf(value.c_str(), "%c,%d", &layout, &offset) == 2) {
      if (layout == 't')
        default_display_placement_.position = DisplayPlacement::TOP;
      else if (layout == 'b')
        default_display_placement_.position = DisplayPlacement::BOTTOM;
      else if (layout == 'r')
        default_display_placement_.position = DisplayPlacement::RIGHT;
      else if (layout == 'l')
        default_display_placement_.position = DisplayPlacement::LEFT;
      default_display_placement_.offset = offset;
    }
  }
}

DisplayLayoutStore::~DisplayLayoutStore() {}

void DisplayLayoutStore::SetDefaultDisplayPlacement(
    const DisplayPlacement& placement) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kSecondaryDisplayLayout))
    default_display_placement_ = placement;
}

void DisplayLayoutStore::RegisterLayoutForDisplayIdList(
    const DisplayIdList& list,
    std::unique_ptr<DisplayLayout> layout) {
  // m50/51 dev/beta channel may have bad layout data saved in local state.
  // TODO(oshima): Consider removing this after m53.
  if (list.size() == 2 && layout->placement_list.size() > 1)
    return;

  // Do not overwrite the valid data with old invalid date.
  if (layouts_.count(list) && !CompareDisplayIds(list[0], list[1]))
    return;

  // Old data may not have the display_id/parent_display_id.
  // Guess these values based on the saved primary_id.
  if (layout->placement_list.size() >= 1 &&
      layout->placement_list[0].display_id == kInvalidDisplayId) {
    if (layout->primary_id == list[1]) {
      layout->placement_list[0].display_id = list[0];
      layout->placement_list[0].parent_display_id = list[1];
    } else {
      layout->placement_list[0].display_id = list[1];
      layout->placement_list[0].parent_display_id = list[0];
    }
  }

  if (!DisplayLayout::Validate(list, *layout.get())) {
    NOTREACHED_IN_MIGRATION()
        << "Attempting to register an invalid layout: ids="
        << DisplayIdListToString(list) << ", layout=" << layout->ToString();
    // We never allow to register an invalid layout, instead, we revert back to
    // a default layout.
    CreateDefaultDisplayLayout(list);
    return;
  }

  layouts_[list] = std::move(layout);
}

const DisplayLayout& DisplayLayoutStore::GetRegisteredDisplayLayout(
    const DisplayIdList& list) {
  return GetOrCreateRegisteredDisplayLayoutInternal(list, /*create=*/false);
}

void DisplayLayoutStore::UpdateDefaultUnified(const DisplayIdList& list,
                                              bool default_unified) {
  DCHECK(layouts_.find(list) != layouts_.end());
  if (layouts_.find(list) == layouts_.end())
    GetOrCreateRegisteredDisplayLayoutInternal(list,
                                               /*create_if_not_exist=*/false);
  layouts_[list]->default_unified = default_unified;
}

const DisplayLayout& DisplayLayoutStore::GetOrCreateRegisteredDisplayLayout(
    const DisplayIdList& list) {
  return GetOrCreateRegisteredDisplayLayoutInternal(list, /*create=*/true);
}

DisplayLayout* DisplayLayoutStore::CreateDefaultDisplayLayout(
    const DisplayIdList& list) {
  std::unique_ptr<DisplayLayout> layout(new DisplayLayout);
  // The first display is the primary by default.
  layout->primary_id = list[0];
  layout->placement_list.clear();
  for (size_t i = 0; i < list.size() - 1; i++) {
    DisplayPlacement placement(default_display_placement_);
    placement.display_id = list[i + 1];
    placement.parent_display_id = list[i];
    layout->placement_list.push_back(placement);
  }
  layouts_[list] = std::move(layout);
  auto iter = layouts_.find(list);
  return iter->second.get();
}

const DisplayLayout&
DisplayLayoutStore::GetOrCreateRegisteredDisplayLayoutInternal(
    const DisplayIdList& list,
    bool create_if_not_exist) {
  DCHECK_GT(list.size(), 1u);
  DCHECK(IsDisplayIdListSorted(list));

  const auto iter = layouts_.find(list);
  DCHECK(create_if_not_exist || iter != layouts_.end());

  const DisplayLayout* layout = iter != layouts_.end()
                                    ? iter->second.get()
                                    : CreateDefaultDisplayLayout(list);

  DCHECK(DisplayLayout::Validate(list, *layout)) << layout->ToString();
  DCHECK_NE(layout->primary_id, kInvalidDisplayId);
  return *layout;
}

}  // namespace display
