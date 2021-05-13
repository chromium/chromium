// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_list.h"

#include "base/memory/ptr_util.h"
#include "ui/display/display_observer.h"

namespace display {

DisplayList::DisplayList() = default;

DisplayList::~DisplayList() = default;

void DisplayList::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void DisplayList::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

DisplayList::Displays::const_iterator DisplayList::FindDisplayById(
    int64_t id) const {
  for (auto iter = displays_.begin(); iter != displays_.end(); ++iter) {
    if (iter->id() == id)
      return iter;
  }
  return displays_.end();
}

DisplayList::Displays::const_iterator DisplayList::GetPrimaryDisplayIterator()
    const {
  return primary_display_index_ == -1
             ? displays_.end()
             : displays_.begin() + primary_display_index_;
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
  if (type == Type::PRIMARY &&
      static_cast<int>(iter - displays_.begin()) !=
          static_cast<int>(GetPrimaryDisplayIterator() - displays_.begin())) {
    primary_display_index_ = static_cast<int>(iter - displays_.begin());
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
  return changed_values;
}

void DisplayList::AddDisplay(const Display& display, Type type) {
  DCHECK(displays_.end() == FindDisplayByIdInternal(display.id()));
  displays_.push_back(display);
  if (type == Type::PRIMARY)
    primary_display_index_ = static_cast<int>(displays_.size()) - 1;
  for (DisplayObserver& observer : observers_)
    observer.OnDisplayAdded(display);
}

void DisplayList::RemoveDisplay(int64_t id) {
  auto iter = FindDisplayByIdInternal(id);
  DCHECK(displays_.end() != iter);
  if (primary_display_index_ == static_cast<int>(iter - displays_.begin())) {
    // The primary display can only be removed if it is the last display.
    // Users must choose a new primary before removing an old primary display.
    DCHECK_EQ(1u, displays_.size());
    primary_display_index_ = -1;
  } else if (primary_display_index_ >
             static_cast<int>(iter - displays_.begin())) {
    primary_display_index_--;
  }
  const Display display = *iter;
  displays_.erase(iter);
  for (DisplayObserver& observer : observers_)
    observer.OnDisplayRemoved(display);
}

DisplayList::Type DisplayList::GetTypeByDisplayId(int64_t display_id) const {
  if (primary_display_index_ == -1)
    return Type::NOT_PRIMARY;
  return (displays_[primary_display_index_].id() == display_id
              ? Type::PRIMARY
              : Type::NOT_PRIMARY);
}

DisplayList::Displays::iterator DisplayList::FindDisplayByIdInternal(
    int64_t id) {
  for (auto iter = displays_.begin(); iter != displays_.end(); ++iter) {
    if (iter->id() == id)
      return iter;
  }
  return displays_.end();
}

}  // namespace display
