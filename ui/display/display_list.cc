// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_list.h"

#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "ui/display/display_observer.h"

namespace display {

DisplayList::DisplayList() = default;

DisplayList::~DisplayList() = default;

DisplayList::DisplayList(const Displays& displays,
                         int64_t primary_id,
                         int64_t current_id)
    : displays_(displays), primary_id_(primary_id), current_id_(current_id) {
#if defined(OS_FUCHSIA)
  // TODO(crbug.com/1207996): Resolve ScenicScreen's lack of primary display.
  if (!displays_.empty() && primary_id_ == kInvalidDisplayId)
    primary_id_ = displays_[0].id();
#endif  // OS_FUCHSIA
  DCHECK(observers_.empty());
  DCHECK(IsValidOrEmpty());
}

DisplayList::DisplayList(const DisplayList& other)
    : displays_(other.displays_),
      primary_id_(other.primary_id_),
      current_id_(other.current_id_) {
  DCHECK(other.observers_.empty());
  DCHECK(observers_.empty());
  DCHECK(IsValidOrEmpty());
}

DisplayList& DisplayList::operator=(const DisplayList& other) {
  displays_ = other.displays_;
  primary_id_ = other.primary_id_;
  current_id_ = other.current_id_;
  DCHECK(other.observers_.empty());
  DCHECK(observers_.empty());
  DCHECK(IsValidOrEmpty());
  return *this;
}

bool DisplayList::operator==(const DisplayList& other) const {
  return displays_ == other.displays_ && primary_id_ == other.primary_id_ &&
         current_id_ == other.current_id_;
}

void DisplayList::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void DisplayList::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

DisplayList::Displays::const_iterator DisplayList::FindDisplayById(
    int64_t id) const {
  return base::ranges::find(displays_, id, &Display::id);
}

DisplayList::Displays::const_iterator DisplayList::GetPrimaryDisplayIterator()
    const {
  return FindDisplayById(primary_id_);
}

const Display& DisplayList::GetPrimaryDisplay() const {
  Displays::const_iterator primary_iter = GetPrimaryDisplayIterator();
  CHECK(primary_iter != displays_.end());
  return *primary_iter;
}

const Display& DisplayList::GetCurrentDisplay() const {
  Displays::const_iterator current_iter = FindDisplayById(current_id_);
  CHECK(current_iter != displays_.end());
  return *current_iter;
}

void DisplayList::AddOrUpdateDisplay(const Display& display, Type type) {
  if (FindDisplayById(display.id()) == displays_.end())
    AddDisplay(display, type);
  else
    UpdateDisplay(display, type);
}

uint32_t DisplayList::UpdateDisplay(const Display& display) {
  return UpdateDisplay(display, GetTypeByDisplayId(display.id()));
}

uint32_t DisplayList::UpdateDisplay(const Display& display, Type type) {
  auto iter = FindDisplayByIdInternal(display.id());
  DCHECK(iter != displays_.end());

  Display* local_display = &(*iter);
  uint32_t changed_values = 0;
  // TODO(crbug.com/1207996): Guard against removal of the primary designation.
  // DCHECK(type == Type::PRIMARY || local_display->id() != primary_id_);
  if (type == Type::PRIMARY && local_display->id() != primary_id_) {
    primary_id_ = local_display->id();
    // ash::DisplayManager only notifies for the Display gaining primary, not
    // the one losing it.
    changed_values |= DisplayObserver::DISPLAY_METRIC_PRIMARY;
  }
  if (local_display->bounds() != display.bounds()) {
    local_display->set_bounds(display.bounds());
    changed_values |= DisplayObserver::DISPLAY_METRIC_BOUNDS;
  }
  if (local_display->work_area() != display.work_area()) {
    local_display->set_work_area(display.work_area());
    changed_values |= DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  }
  if (local_display->rotation() != display.rotation()) {
    local_display->set_rotation(display.rotation());
    changed_values |= DisplayObserver::DISPLAY_METRIC_ROTATION;
  }
  if (local_display->panel_rotation() != display.panel_rotation()) {
    local_display->set_panel_rotation(display.panel_rotation());
    changed_values |= DisplayObserver::DISPLAY_METRIC_ROTATION;
  }
  if (local_display->device_scale_factor() != display.device_scale_factor()) {
    local_display->set_device_scale_factor(display.device_scale_factor());
    changed_values |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  }
  if (local_display->color_spaces() != display.color_spaces() ||
      local_display->depth_per_component() != display.depth_per_component() ||
      local_display->color_depth() != display.color_depth()) {
    local_display->set_color_spaces(display.color_spaces());
    local_display->set_depth_per_component(display.depth_per_component());
    local_display->set_color_depth(display.color_depth());
    changed_values |= DisplayObserver::DISPLAY_METRIC_COLOR_SPACE;
  }
  if (local_display->GetSizeInPixel() != display.GetSizeInPixel()) {
    local_display->set_size_in_pixels(display.GetSizeInPixel());
  }
  for (DisplayObserver& observer : observers_)
    observer.OnDisplayMetricsChanged(*local_display, changed_values);
  DCHECK(IsValidOrEmpty());
  return changed_values;
}

void DisplayList::AddDisplay(const Display& display, Type type) {
  DCHECK(displays_.end() == FindDisplayById(display.id()));
  displays_.push_back(display);
  // The first display added should always be designated as the primary display.
  // Displays added later can take on the primary designation as appropriate.
  if (type == Type::PRIMARY || displays_.size() == 1)
    primary_id_ = display.id();
  for (DisplayObserver& observer : observers_)
    observer.OnDisplayAdded(display);
  DCHECK(IsValidOrEmpty());
}

void DisplayList::RemoveDisplay(int64_t id) {
  auto iter = FindDisplayByIdInternal(id);
  DCHECK(displays_.end() != iter);
  if (id == primary_id_) {
    // The primary display can only be removed if it is the last display.
    // Users must choose a new primary before removing an old primary display.
    DCHECK_EQ(1u, displays_.size());
    primary_id_ = kInvalidDisplayId;
  }
  if (id == current_id_) {
    // The current display can only be removed if it is the last display.
    // Users must choose a new current before removing an old current display.
    DCHECK_EQ(1u, displays_.size());
    current_id_ = kInvalidDisplayId;
  }
  const Display display = *iter;
  displays_.erase(iter);
  for (DisplayObserver& observer : observers_) {
    observer.OnDisplayRemoved(display);
    observer.OnDidRemoveDisplays();
  }
  DCHECK(IsValidOrEmpty());
}

bool DisplayList::IsValidOrEmpty() const {
  // The primary and current ids must be invalid when `displays_` is empty.
  if (displays_.empty())
    return primary_id_ == kInvalidDisplayId && current_id_ == kInvalidDisplayId;

  // Ensure ids are unique. 96% of clients have a display count <= 3, 98% <= 4,
  // with a max count of 16 seen on Windows. With these low counts we can use
  // a brute force search.
  for (auto outer = displays_.begin(); outer != displays_.end(); ++outer) {
    for (auto inner = outer + 1; inner != displays_.end(); ++inner) {
      if (inner->id() == outer->id()) {
        return false;
      }
    }
  }

  // The primary id must correspond to a `displays_` entry.
  if (GetPrimaryDisplayIterator() == displays_.end())
    return false;

  // The current id may be invalid, or must correspond to a `displays_` entry.
  if (current_id_ != kInvalidDisplayId &&
      FindDisplayById(current_id_) == displays_.end()) {
    return false;
  }

  return true;
}

bool DisplayList::IsValidAndHasPrimaryAndCurrentDisplays() const {
  return IsValidOrEmpty() && GetPrimaryDisplayIterator() != displays_.end() &&
         FindDisplayById(current_id_) != displays_.end();
}

DisplayList::Type DisplayList::GetTypeByDisplayId(int64_t display_id) const {
  return display_id == primary_id_ ? Type::PRIMARY : Type::NOT_PRIMARY;
}

DisplayList::Displays::iterator DisplayList::FindDisplayByIdInternal(
    int64_t id) {
  return base::ranges::find(displays_, id, &Display::id);
}

}  // namespace display
