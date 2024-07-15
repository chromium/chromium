// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_list.h"

#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
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
  return base::ranges::find(displays_, id, &Display::id);
}

DisplayList::Displays::const_iterator DisplayList::GetPrimaryDisplayIterator()
    const {
  return base::ranges::find(displays_, primary_id_, &Display::id);
}

void DisplayList::AddOrUpdateDisplay(const Display& display, Type type) {
  if (FindDisplayById(display.id()) == displays_.end())
    AddDisplay(display, type);
  else
    UpdateDisplay(display, type);
  DCHECK(IsValid());
}

uint32_t DisplayList::UpdateDisplay(const Display& display) {
  return UpdateDisplay(
      display, display.id() == primary_id_ ? Type::PRIMARY : Type::NOT_PRIMARY);
}

uint32_t DisplayList::UpdateDisplay(const Display& display, Type type) {
  auto iter = FindDisplayByIdInternal(display.id());
  CHECK(iter != displays_.end(), base::NotFatalUntil::M130);

  Display* local_display = &(*iter);
  uint32_t changed_values = 0;

  // For now, unsetting the primary does nothing. Setting a new primary is the
  // only way to modify it, so that there is always exactly one primary.
  // TODO(enne): it would be nice to enforce setting the new primary first
  // before unsetting the old one but Wayland handles primary setting based on
  // messages from Wayland itself which may be delivered in any order.
  // See: WaylandScreenTest.MultipleOutputsAddedAndRemoved
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
  if (local_display->GetColorSpaces() != display.GetColorSpaces() ||
      local_display->depth_per_component() != display.depth_per_component() ||
      local_display->color_depth() != display.color_depth()) {
    local_display->SetColorSpaces(display.GetColorSpaces());
    local_display->set_depth_per_component(display.depth_per_component());
    local_display->set_color_depth(display.color_depth());
    changed_values |= DisplayObserver::DISPLAY_METRIC_COLOR_SPACE;
  }
  if (local_display->label() != display.label()) {
    local_display->set_label(display.label());
    changed_values |= DisplayObserver::DISPLAY_METRIC_LABEL;
  }
  if (local_display->GetSizeInPixel() != display.GetSizeInPixel()) {
    local_display->set_size_in_pixels(display.GetSizeInPixel());
  }
  if (local_display->native_origin() != display.native_origin()) {
    local_display->set_native_origin(display.native_origin());
  }
  for (DisplayObserver& observer : observers_)
    observer.OnDisplayMetricsChanged(*local_display, changed_values);
  DCHECK(IsValid());
  return changed_values;
}

void DisplayList::AddDisplay(const Display& display, Type type) {
  DCHECK(displays_.end() == FindDisplayById(display.id()));
  DCHECK_NE(display.id(), kInvalidDisplayId);
  // The first display must be primary.
  DCHECK(type == Type::PRIMARY || !displays_.empty());
  displays_.push_back(display);
  if (type == Type::PRIMARY)
    primary_id_ = display.id();
  for (DisplayObserver& observer : observers_)
    observer.OnDisplayAdded(display);
  DCHECK(IsValid());
}

void DisplayList::RemoveDisplay(int64_t id) {
  auto iter = FindDisplayByIdInternal(id);
  CHECK(displays_.end() != iter, base::NotFatalUntil::M130);
  if (id == primary_id_) {
    // The primary display can only be removed if it is the last display.
    // Users must choose a new primary before removing an old primary display.
    DCHECK_EQ(1u, displays_.size());
    primary_id_ = kInvalidDisplayId;
  }
  const Display display = *iter;
  displays_.erase(iter);
  for (DisplayObserver& observer : observers_) {
    observer.OnDisplaysRemoved({display});
  }
  DCHECK(IsValid());
}

bool DisplayList::IsValid() const {
  // The primary id must be invalid when `displays_` is empty.
  if (displays_.empty())
    return primary_id_ == kInvalidDisplayId;

  // The primary id must exist if there is at least one display.
  if (primary_id_ == kInvalidDisplayId)
    return false;

  // Ensure ids are unique and valid. 96% of clients have a display count <= 3,
  // 98% <= 4, with a max count of 16 seen on Windows. With these low counts we
  // can use a brute force search.
  for (auto outer = displays_.begin(); outer != displays_.end(); ++outer) {
    if (outer->id() == kInvalidDisplayId)
      return false;
    for (auto inner = outer + 1; inner != displays_.end(); ++inner) {
      if (inner->id() == outer->id()) {
        return false;
      }
    }
  }

  // The primary id must correspond to a `displays_` entry.
  if (GetPrimaryDisplayIterator() == displays_.end())
    return false;

  return true;
}

DisplayList::Displays::iterator DisplayList::FindDisplayByIdInternal(
    int64_t id) {
  return base::ranges::find(displays_, id, &Display::id);
}

}  // namespace display
