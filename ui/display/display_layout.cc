// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_layout.h"

#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/display/display.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
namespace {

// DisplayPlacement Positions
const char kTop[] = "top";
const char kRight[] = "right";
const char kBottom[] = "bottom";
const char kLeft[] = "left";
const char kUnknown[] = "unknown";

// The maximum value for 'offset' in DisplayLayout in case of outliers.  Need
// to change this value in case to support even larger displays.
const int kMaxValidOffset = 10000;

bool ComparePlacements(const DisplayPlacement& d1, const DisplayPlacement& d2) {
  return CompareDisplayIds(d1.display_id, d2.display_id);
}

// Extracts the displays IDs list from the displays list.
DisplayIdList DisplayListToDisplayIdList(const Displays& displays) {
  DisplayIdList list;
  for (const auto& display : displays)
    list.emplace_back(display.id());

  return list;
}

// Returns nullptr if display with |id| is not found.
Display* FindDisplayById(Displays* display_list, int64_t id) {
  auto iter = base::ranges::find(*display_list, id, &Display::id);
  return iter == display_list->end() ? nullptr : &(*iter);
}

// Returns the tree depth of the display with ID |display_id| from the tree root
// (i.e. from the primary display).
int GetDisplayTreeDepth(
    int64_t display_id,
    int64_t primary_id,
    const std::map<int64_t, int64_t>& display_to_parent_ids_map) {
  int64_t current_id = display_id;
  int depth = 0;
  const int kMaxDepth = 100;  // Avoid layouts with cycles.
  while (current_id != primary_id && depth < kMaxDepth) {
    ++depth;
    auto iter = display_to_parent_ids_map.find(current_id);
    if (iter == display_to_parent_ids_map.end())
      return kMaxDepth;  // Let detached displays go to the end.

    current_id = iter->second;
  }

  return depth;
}

// Returns true if the child and parent displays are sharing a border that
// matches the child's relative position to its parent.
bool AreDisplaysTouching(const Display& child_display,
                         const Display& parent_display,
                         DisplayPlacement::Position child_position) {
  const gfx::Rect& a_bounds = child_display.bounds();
  const gfx::Rect& b_bounds = parent_display.bounds();

  if (child_position == DisplayPlacement::TOP ||
      child_position == DisplayPlacement::BOTTOM) {
    const int rb = std::min(a_bounds.bottom(), b_bounds.bottom());
    const int ry = std::max(a_bounds.y(), b_bounds.y());
    return rb == ry;
  }

  const int rx = std::max(a_bounds.x(), b_bounds.x());
  const int rr = std::min(a_bounds.right(), b_bounds.right());
  return rr == rx;
}

// After the layout has been applied to the |display_list| and any possible
// overlaps have been fixed, this function is called to update the offsets in
// the |placement_list|, and make sure the placement list is sorted by display
// IDs.
void UpdatePlacementList(Displays* display_list,
                         std::vector<DisplayPlacement>* placement_list) {
  std::sort(placement_list->begin(), placement_list->end(), ComparePlacements);

  for (DisplayPlacement& placement : *placement_list) {
    const Display* child_display =
        FindDisplayById(display_list, placement.display_id);
    const Display* parent_display =
        FindDisplayById(display_list, placement.parent_display_id);

    if (!child_display || !parent_display)
      continue;

    const gfx::Rect& child_bounds = child_display->bounds();
    const gfx::Rect& parent_bounds = parent_display->bounds();

    if (placement.position == DisplayPlacement::TOP ||
        placement.position == DisplayPlacement::BOTTOM) {
      placement.offset = child_bounds.x() - parent_bounds.x();
    } else {
      placement.offset = child_bounds.y() - parent_bounds.y();
    }
  }
}

// Reparents |target_display| to |last_intersecting_source_display| if it's not
// touching with its current parent. It also handles the case if
// |target_display| is detached, it then reparents it to the last intersecting
// display.
void MaybeReparentTargetDisplay(
    int last_offset_x,
    int last_offset_y,
    const Display* last_intersecting_source_display,
    const Display* target_display,
    std::map<int64_t, int64_t>* display_to_parent_ids_map,
    Displays* display_list,
    std::vector<DisplayPlacement>* placement_list) {
  // A de-intersection was performed.
  // The offset target display may have moved such that it no longer touches
  // its parent. Reparent if necessary.
  DisplayPlacement* target_display_placement = nullptr;
  auto iter = display_to_parent_ids_map->find(target_display->id());
  if (iter != display_to_parent_ids_map->end()) {
    const int64_t parent_display_id = iter->second;
    if (parent_display_id == last_intersecting_source_display->id()) {
      // It was just de-intersected with the source display in such a way that
      // they're touching, and the source display is its parent. So no need to
      // do any reparenting.
      return;
    }

    Display* parent_display = FindDisplayById(display_list, parent_display_id);
    DCHECK(parent_display);

    auto target_display_placement_itr = base::ranges::find(
        *placement_list, target_display->id(), &DisplayPlacement::display_id);
    CHECK(target_display_placement_itr != placement_list->end(),
          base::NotFatalUntil::M130);
    target_display_placement = &(*target_display_placement_itr);
    if (AreDisplaysTouching(*target_display, *parent_display,
                            target_display_placement->position)) {
      return;
    }
  } else {
    // It's a detached display with no parent. Add a new placement for it.
    DisplayPlacement new_placement;
    new_placement.display_id = target_display->id();
    placement_list->emplace_back(new_placement);
    target_display_placement = &placement_list->back();
  }

  DCHECK(target_display_placement);

  // Reparent the target to source and update the position. No need to
  // update the offset here as it will be done later when UpdateOffsets()
  // is called.
  target_display_placement->parent_display_id =
      last_intersecting_source_display->id();
  // Update the map.
  (*display_to_parent_ids_map)[target_display->id()] =
      last_intersecting_source_display->id();

  if (last_offset_x) {
    target_display_placement->position =
        last_offset_x > 0 ? DisplayPlacement::RIGHT : DisplayPlacement::LEFT;
  } else {
    target_display_placement->position =
        last_offset_y > 0 ? DisplayPlacement::BOTTOM : DisplayPlacement::TOP;
  }
}

// Offsets |display| by the provided |x| and |y| values.
void OffsetDisplay(Display* display, int x, int y) {
  gfx::Point new_origin = display->bounds().origin();
  new_origin.Offset(x, y);
  gfx::Insets insets = display->GetWorkAreaInsets();
  display->set_bounds(gfx::Rect(new_origin, display->bounds().size()));
  display->UpdateWorkAreaFromInsets(insets);
}

// Calculates the amount of offset along the X or Y axes for the target display
// with |target_bounds| to de-intersect with the source display with
// |source_bounds|.
// These functions assume both displays already intersect.
int CalculateOffsetX(const gfx::Rect& source_bounds,
                     const gfx::Rect& target_bounds) {
  if (target_bounds.x() >= 0) {
    // Target display moves along the +ve X direction.
    return source_bounds.right() - target_bounds.x();
  }

  // Target display moves along the -ve X direction.
  return -(target_bounds.right() - source_bounds.x());
}
int CalculateOffsetY(const gfx::Rect& source_bounds,
                     const gfx::Rect& target_bounds) {
  if (target_bounds.y() >= 0) {
    // Target display moves along the +ve Y direction.
    return source_bounds.bottom() - target_bounds.y();
  }

  // Target display moves along the -ve Y direction.
  return -(target_bounds.bottom() - source_bounds.y());
}

// Fixes any overlapping displays and reparents displays if necessary.
void DeIntersectDisplays(int64_t primary_id,
                         Displays* display_list,
                         std::vector<DisplayPlacement>* placement_list,
                         std::set<int64_t>* updated_displays) {
  std::map<int64_t, int64_t> display_to_parent_ids_map;
  for (const DisplayPlacement& placement : *placement_list) {
    display_to_parent_ids_map.insert(
        std::make_pair(placement.display_id, placement.parent_display_id));
  }

  std::vector<Display*> sorted_displays;
  for (Display& display : *display_list)
    sorted_displays.push_back(&display);

  // Sort the displays first by their depth in the display hierarchy tree, and
  // then by distance of their top left points from the origin. This way we
  // process the displays starting at the root (the primary display), in the
  // order of their descendence spanning out from the primary display.
  std::sort(sorted_displays.begin(), sorted_displays.end(), [&](Display* d1,
                                                                Display* d2) {
    const int d1_depth =
        GetDisplayTreeDepth(d1->id(), primary_id, display_to_parent_ids_map);
    const int d2_depth =
        GetDisplayTreeDepth(d2->id(), primary_id, display_to_parent_ids_map);

    if (d1_depth != d2_depth)
      return d1_depth < d2_depth;

    const int64_t d1_distance = d1->bounds().OffsetFromOrigin().LengthSquared();
    const int64_t d2_distance = d2->bounds().OffsetFromOrigin().LengthSquared();

    if (d1_distance != d2_distance)
      return d1_distance < d2_distance;

    return d1->id() < d2->id();
  });
  // This must result in the primary display being at the front of the list.
  DCHECK_EQ(sorted_displays.front()->id(), primary_id);

  for (size_t i = 1; i < sorted_displays.size(); ++i) {
    Display* target_display = sorted_displays[i];
    const Display* last_intersecting_source_display = nullptr;
    int last_offset_x = 0;
    int last_offset_y = 0;
    for (size_t j = 0; j < i; ++j) {
      const Display* source_display = sorted_displays[j];
      const gfx::Rect source_bounds = source_display->bounds();
      const gfx::Rect target_bounds = target_display->bounds();

      gfx::Rect intersection = source_bounds;
      intersection.Intersect(target_bounds);

      if (intersection.IsEmpty())
        continue;

      // Calculate offsets along both X and Y axes such that either can remove
      // the overlap, but choose and apply the smaller offset. This way we have
      // more predictable results.
      int offset_x = 0;
      int offset_y = 0;
      if (intersection.width())
        offset_x = CalculateOffsetX(source_bounds, target_bounds);
      if (intersection.height())
        offset_y = CalculateOffsetY(source_bounds, target_bounds);

      if (offset_x == 0 && offset_y == 0)
        continue;

      // Choose the smaller offset.
      if (std::abs(offset_x) <= std::abs(offset_y))
        offset_y = 0;
      else
        offset_x = 0;

      OffsetDisplay(target_display, offset_x, offset_y);
      updated_displays->insert(target_display->id());

      // The most recent performed de-intersection data.
      last_intersecting_source_display = source_display;
      last_offset_x = offset_x;
      last_offset_y = offset_y;
    }

    if (!last_intersecting_source_display)
      continue;

    MaybeReparentTargetDisplay(last_offset_x, last_offset_y,
                               last_intersecting_source_display, target_display,
                               &display_to_parent_ids_map, display_list,
                               placement_list);
  }

  // New placements might have been added and offsets might have changed and we
  // must update them.
  UpdatePlacementList(display_list, placement_list);
}

// Checks if the given point is over the radius vector described by its end
// point |vector|. The point is over a vector if it's on its positive (left)
// side. The method sees a point on the same line as the vector as being over
// the vector.
bool IsPointOverRadiusVector(const gfx::Point& point,
                             const gfx::Point& vector) {
  // |point| is left of |vector| if its radius vector's scalar product with a
  // vector orthogonal (and facing the positive side) to |vector| is positive.
  //
  // An orthogonal vector of (a, b) is (b, -a), as the scalar product of these
  // two is 0.
  // So, (x, y) is over (a, b) if x * b + y * (-a) >= 0, which is equivalent to
  // x * b >= y * a.
  return static_cast<int64_t>(point.x()) * static_cast<int64_t>(vector.y()) >=
         static_cast<int64_t>(point.y()) * static_cast<int64_t>(vector.x());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisplayPlacement

DisplayPlacement::DisplayPlacement()
    : DisplayPlacement(kInvalidDisplayId,
                       kInvalidDisplayId,
                       DisplayPlacement::RIGHT,
                       0,
                       DisplayPlacement::TOP_LEFT) {}

DisplayPlacement::DisplayPlacement(Position position, int offset)
    : DisplayPlacement(kInvalidDisplayId,
                       kInvalidDisplayId,
                       position,
                       offset,
                       DisplayPlacement::TOP_LEFT) {}

DisplayPlacement::DisplayPlacement(Position position,
                                   int offset,
                                   OffsetReference offset_reference)
    : DisplayPlacement(kInvalidDisplayId,
                       kInvalidDisplayId,
                       position,
                       offset,
                       offset_reference) {}

DisplayPlacement::DisplayPlacement(int64_t display_id,
                                   int64_t parent_display_id,
                                   Position position,
                                   int offset,
                                   OffsetReference offset_reference)
    : display_id(display_id),
      parent_display_id(parent_display_id),
      position(position),
      offset(offset),
      offset_reference(offset_reference) {
  DCHECK_LE(TOP, position);
  DCHECK_GE(LEFT, position);
  // Set the default value to |position| in case position is invalid.  DCHECKs
  // above doesn't stop in Release builds.
  if (TOP > position || LEFT < position)
    this->position = RIGHT;

  DCHECK_GE(kMaxValidOffset, abs(offset));
}

DisplayPlacement::DisplayPlacement(const DisplayPlacement&) = default;

DisplayPlacement& DisplayPlacement::operator=(const DisplayPlacement&) =
    default;

bool DisplayPlacement::operator==(const DisplayPlacement& other) const {
  return display_id == other.display_id &&
         parent_display_id == other.parent_display_id &&
         position == other.position &&
         offset == other.offset &&
         offset_reference == other.offset_reference;
}

bool DisplayPlacement::operator!=(const DisplayPlacement& other) const {
  return !operator==(other);
}

DisplayPlacement& DisplayPlacement::Swap() {
  switch (position) {
    case TOP:
      position = BOTTOM;
      break;
    case BOTTOM:
      position = TOP;
      break;
    case RIGHT:
      position = LEFT;
      break;
    case LEFT:
      position = RIGHT;
      break;
  }
  offset = -offset;
  std::swap(display_id, parent_display_id);
  return *this;
}

std::string DisplayPlacement::ToString() const {
  std::stringstream s;
  if (display_id != kInvalidDisplayId)
    s << "id=" << display_id << ", ";
  if (parent_display_id != kInvalidDisplayId)
    s << "parent=" << parent_display_id << ", ";
  s << PositionToString(position) << ", ";
  s << offset;
  return s.str();
}

// static
std::string DisplayPlacement::PositionToString(Position position) {
  switch (position) {
    case TOP:
      return kTop;
    case RIGHT:
      return kRight;
    case BOTTOM:
      return kBottom;
    case LEFT:
      return kLeft;
  }
  return kUnknown;
}

// static
bool DisplayPlacement::StringToPosition(std::string_view string,
                                        Position* position) {
  if (string == kTop) {
    *position = TOP;
    return true;
  }

  if (string == kRight) {
    *position = RIGHT;
    return true;
  }

  if (string == kBottom) {
    *position = BOTTOM;
    return true;
  }

  if (string == kLeft) {
    *position = LEFT;
    return true;
  }

  LOG(ERROR) << "Invalid position value:" << string;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayLayout

DisplayLayout::DisplayLayout()
    : default_unified(true), primary_id(kInvalidDisplayId) {}

DisplayLayout::~DisplayLayout() {}

void DisplayLayout::ApplyToDisplayList(Displays* display_list,
                                       std::vector<int64_t>* updated_ids,
                                       int minimum_offset_overlap) {
  if (placement_list.empty())
    return;

  if (!DisplayLayout::Validate(DisplayListToDisplayIdList(*display_list),
                               *this)) {
    // Prevent invalid and non-relevant display layouts.
    return;
  }

  // Layout from primary, then dependent displays.
  std::set<int64_t> parents;
  std::set<int64_t> updated_displays;
  parents.insert(primary_id);
  while (parents.size()) {
    int64_t parent_id = *parents.begin();
    parents.erase(parent_id);
    for (const DisplayPlacement& placement : placement_list) {
      if (placement.parent_display_id == parent_id) {
        if (ApplyDisplayPlacement(placement, display_list,
                                  minimum_offset_overlap)) {
          updated_displays.insert(placement.display_id);
        }
        parents.insert(placement.display_id);
      }
    }
  }

  // Now that all the placements have been applied, we must detect and fix any
  // overlapping displays.
  DeIntersectDisplays(primary_id, display_list, &placement_list,
                      &updated_displays);

  if (updated_ids) {
    updated_ids->insert(updated_ids->begin(), updated_displays.begin(),
                        updated_displays.end());
  }
}

// static
bool DisplayLayout::Validate(const DisplayIdList& list,
                             const DisplayLayout& layout) {
  // The primary display should be in the list.
  if (!base::Contains(list, layout.primary_id)) {
    DISPLAY_LOG(ERROR) << "The primary id: " << layout.primary_id
                       << " is not in the id list.";
    return false;
  }

  // Unified mode, or mirror mode switched from unified mode,
  // may not have the placement yet.
  if (layout.placement_list.size() == 0u)
    return true;

  bool has_primary_as_parent = false;
  // The placement list must be sorted by the first 8 bits of the display IDs.
  int64_t prev_id = std::numeric_limits<int8_t>::min();
  for (const auto& placement : layout.placement_list) {
    // Placements are sorted by display_id.
    if (prev_id >= (placement.display_id & 0xFF)) {
      DISPLAY_LOG(ERROR) << "PlacementList must be sorted by first 8 bits of"
                         << " display_id ";
      return false;
    }
    prev_id = (placement.display_id & 0xFF);
    if (placement.display_id == kInvalidDisplayId) {
      DISPLAY_LOG(ERROR) << "display_id is not initialized";
      return false;
    }
    if (placement.parent_display_id == kInvalidDisplayId) {
      DISPLAY_LOG(ERROR) << "display_parent_id is not initialized";
      return false;
    }
    if (placement.display_id == placement.parent_display_id) {
      DISPLAY_LOG(ERROR) << "display_id must not be same as parent_display_id";
      return false;
    }
    if (!base::Contains(list, placement.display_id)) {
      DISPLAY_LOG(ERROR) << "display_id is not in the id list:"
                         << placement.ToString();
      return false;
    }

    if (!base::Contains(list, placement.parent_display_id)) {
      DISPLAY_LOG(ERROR) << "parent_display_id is not in the id list:"
                         << placement.ToString();
      return false;
    }
    has_primary_as_parent |= layout.primary_id == placement.parent_display_id;
  }
  if (!has_primary_as_parent)
    DISPLAY_LOG(ERROR)
        << "At least, one placement must have the primary as a parent.";
  return has_primary_as_parent;
}

std::unique_ptr<DisplayLayout> DisplayLayout::Copy() const {
  std::unique_ptr<DisplayLayout> copy(new DisplayLayout);
  for (const auto& placement : placement_list)
    copy->placement_list.push_back(placement);
  copy->default_unified = default_unified;
  copy->primary_id = primary_id;
  return copy;
}

void DisplayLayout::SwapPrimaryDisplay(int64_t new_primary_id) {
  if (primary_id == new_primary_id)
    return;

  // Build a map of the *original* |display_id| for each placement.
  std::unordered_map<int64_t, DisplayPlacement*> id_to_placement;
  for (auto& placement : placement_list)
    id_to_placement[placement.display_id] = &placement;

  // Swap placements so that |new_primary_id| is the display that placements are
  // anchored on and set |primary_id|.
  int64_t swap_display_id = new_primary_id;
  while (swap_display_id != primary_id) {
    DisplayPlacement* placement = id_to_placement.at(swap_display_id);
    swap_display_id = placement->parent_display_id;
    placement->Swap();
  }
  std::sort(placement_list.begin(), placement_list.end(), ComparePlacements);
  primary_id = new_primary_id;
}

bool DisplayLayout::HasSamePlacementList(const DisplayLayout& layout) const {
  return placement_list == layout.placement_list;
}

void DisplayLayout::RemoveDisplayPlacements(const DisplayIdList& list) {
  std::erase_if(placement_list, [&list](const DisplayPlacement& placement) {
    return base::Contains(list, placement.display_id);
  });
  for (DisplayPlacement& placement : placement_list) {
    if (base::Contains(list, placement.parent_display_id))
      placement.parent_display_id = primary_id;
  }
}

std::string DisplayLayout::ToString() const {
  std::stringstream s;
  s << "primary=" << primary_id;
  if (default_unified)
    s << ", default_unified";
  bool added = false;
  for (const auto& placement : placement_list) {
    s << (added ? "),(" : " [(");
    s << placement.ToString();
    added = true;
  }
  if (added)
    s << ")]";
  return s.str();
}

DisplayPlacement DisplayLayout::FindPlacementById(int64_t display_id) const {
  const auto iter = base::ranges::find(placement_list, display_id,
                                       &DisplayPlacement::display_id);
  return (iter == placement_list.end()) ? DisplayPlacement()
                                        : DisplayPlacement(*iter);
}

// Creates a display::DisplayPlacement value for |rectangle| relative to the
// |reference| rectangle.
// The layout consists of two values:
//   - position: Whether the rectangle is positioned left, right, over or under
//     the reference.
//   - offset: The rectangle's offset from the reference origin along the axis
//     opposite the position direction (if the rectangle is left or right along
//     y-axis, otherwise along x-axis).
// The rectangle's position is calculated by dividing the space in areas defined
// by the |reference|'s diagonals and finding the area |rectangle|'s center
// point belongs. If the |rectangle| in the calculated layout does not share a
// part of the bounds with the |reference|, the |rectangle| position in set to
// the more suitable neighboring position (e.g. if |rectangle| is completely
// over the |reference| top bound, it will be set to TOP) and the layout is
// recalculated with the new position. This is to handle the case where the
// rectangle shares an edge with the reference, but it's center is not in the
// same area as the reference's edge, e.g.
//
// +---------------------+
// |                     |
// | REFERENCE           |
// |                     |
// |                     |
// +---------------------+
//                 +-------------------------------------------------+
//                 | RECTANGLE               x                       |
//                 +-------------------------------------------------+
//
// The rectangle shares an edge with the reference's bottom edge, but its
// center point is in the left area.

// static
DisplayPlacement DisplayLayout::CreatePlacementForRectangles(
    const gfx::Rect& reference,
    const gfx::Rect& rectangle) {
  // Translate coordinate system so origin is in the reference's top left point
  // (so the reference's down-diagonal vector starts in the (0, 0)) and scale it
  // up by two (to avoid division when calculating the rectangle's center
  // point).
  gfx::Point center(2 * (rectangle.x() - reference.x()) + rectangle.width(),
                    2 * (rectangle.y() - reference.y()) + rectangle.height());
  gfx::Point down_diag(2 * reference.width(), 2 * reference.height());

  bool is_top_right = IsPointOverRadiusVector(center, down_diag);

  // Translate the coordinate system again, so the bottom right point of the
  // reference is origin (so the reference's up-diagonal starts at (0, 0)).
  // Note that the coordinate system is scaled by 2.
  center.Offset(0, -2 * reference.height());
  // Choose the vector orientation so the points on the diagonal are considered
  // to be left.
  gfx::Point up_diag(-2 * reference.width(), 2 * reference.height());

  bool is_bottom_right = IsPointOverRadiusVector(center, up_diag);

  DisplayPlacement::Position position;
  if (is_top_right) {
    position =
        is_bottom_right ? DisplayPlacement::RIGHT : DisplayPlacement::TOP;
  } else {
    position =
        is_bottom_right ? DisplayPlacement::BOTTOM : DisplayPlacement::LEFT;
  }

  // If the rectangle with the calculated position would not have common side
  // with the reference, try to position it so it shares another edge with the
  // reference.
  if (is_top_right == is_bottom_right) {
    if (rectangle.y() > reference.bottom()) {
      // The rectangle is left or right, but completely under the reference.
      position = DisplayPlacement::BOTTOM;
    } else if (rectangle.bottom() < reference.y()) {
      // The rectangle is left or right, but completely over the reference.
      position = DisplayPlacement::TOP;
    }
  } else {
    if (rectangle.x() > reference.right()) {
      // The rectangle is over or under, but completely right of the reference.
      position = DisplayPlacement::RIGHT;
    } else if (rectangle.right() < reference.x()) {
      // The rectangle is over or under, but completely left of the reference.
      position = DisplayPlacement::LEFT;
    }
  }
  int offset = (position == DisplayPlacement::LEFT ||
                position == DisplayPlacement::RIGHT)
                   ? rectangle.y()
                   : rectangle.x();
  return DisplayPlacement(position, offset);
}

// static
bool DisplayLayout::ApplyDisplayPlacement(const DisplayPlacement& placement,
                                          Displays* display_list,
                                          int minimum_offset_overlap) {
  const Display& parent_display =
      *FindDisplayById(display_list, placement.parent_display_id);
  DCHECK(parent_display.is_valid());
  Display* target_display = FindDisplayById(display_list, placement.display_id);
  gfx::Rect old_bounds(target_display->bounds());
  DCHECK(target_display);

  const gfx::Rect& parent_bounds = parent_display.bounds();
  const gfx::Rect& target_bounds = target_display->bounds();
  gfx::Point new_target_origin = parent_bounds.origin();

  DisplayPlacement::Position position = placement.position;

  // Ignore the offset in case the target display doesn't share edges with
  // the parent display.
  int offset = placement.offset;
  if (position == DisplayPlacement::TOP ||
      position == DisplayPlacement::BOTTOM) {
    if (placement.offset_reference == DisplayPlacement::BOTTOM_RIGHT)
      offset = parent_bounds.width() - offset - target_bounds.width();

    offset = std::min(offset, parent_bounds.width() - minimum_offset_overlap);
    offset = std::max(offset, -target_bounds.width() + minimum_offset_overlap);
  } else {
    if (placement.offset_reference == DisplayPlacement::BOTTOM_RIGHT)
      offset = parent_bounds.height() - offset - target_bounds.height();

    offset = std::min(offset, parent_bounds.height() - minimum_offset_overlap);
    offset = std::max(offset, -target_bounds.height() + minimum_offset_overlap);
  }
  switch (position) {
    case DisplayPlacement::TOP:
      new_target_origin.Offset(offset, -target_bounds.height());
      break;
    case DisplayPlacement::RIGHT:
      new_target_origin.Offset(parent_bounds.width(), offset);
      break;
    case DisplayPlacement::BOTTOM:
      new_target_origin.Offset(offset, parent_bounds.height());
      break;
    case DisplayPlacement::LEFT:
      new_target_origin.Offset(-target_bounds.width(), offset);
      break;
  }

  gfx::Insets insets = target_display->GetWorkAreaInsets();
  target_display->set_bounds(
      gfx::Rect(new_target_origin, target_bounds.size()));
  target_display->UpdateWorkAreaFromInsets(insets);

  return old_bounds != target_display->bounds();
}

}  // namespace display
