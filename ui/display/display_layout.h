// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_LAYOUT_H_
#define UI_DISPLAY_DISPLAY_LAYOUT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ui/display/display_export.h"

namespace gfx {
class Rect;
}

namespace display {
class Display;

// An identifier used to manage display layout in DisplayManager /
// DisplayLayoutStore.
using DisplayIdList = std::vector<int64_t>;

using Displays = std::vector<Display>;

// DisplayPlacement specifies where the display (D) is placed relative to
// parent (P) display. In the following example, D given by |display_id| is
// placed at the left side of P given by |parent_display_id|, with a negative
// offset and a top-left offset reference.
//
//        +      +--------+
// offset |      |        |
//        +      |   D    +--------+
//               |        |        |
//               +--------+   P    |
//                        |        |
//                        +--------+
//
struct DISPLAY_EXPORT DisplayPlacement {
  // The id of the display this placement will be applied to.
  int64_t display_id;

  // The parent display id to which the above display is placed.
  int64_t parent_display_id;

  // To which side the parent display the display is positioned.
  enum Position { TOP, RIGHT, BOTTOM, LEFT };
  Position position;

  // The offset of the position with respect to the parent.
  int offset;

  // Determines if the offset is relative to the TOP_LEFT or the BOTTOM_RIGHT.
  // Defaults to TOP_LEFT.
  enum OffsetReference { TOP_LEFT, BOTTOM_RIGHT };
  OffsetReference offset_reference;

  DisplayPlacement();
  DisplayPlacement(Position position, int offset);
  DisplayPlacement(Position position,
                   int offset,
                   OffsetReference offset_reference);
  DisplayPlacement(int64_t display_id,
                   int64_t parent_display_id,
                   Position position,
                   int offset,
                   OffsetReference offset_reference);

  DisplayPlacement(const DisplayPlacement&);
  DisplayPlacement& operator=(const DisplayPlacement&);

  bool operator==(const DisplayPlacement& other) const;
  bool operator!=(const DisplayPlacement& other) const;

  DisplayPlacement& Swap();

  std::string ToString() const;

  static std::string PositionToString(Position position);
  static bool StringToPosition(std::string_view string, Position* position);
};

class DISPLAY_EXPORT DisplayLayout final {
 public:
  DisplayLayout();

  DisplayLayout(const DisplayLayout&) = delete;
  DisplayLayout& operator=(const DisplayLayout&) = delete;

  ~DisplayLayout();

  // Applies the layout to the displays in |display_list|.
  // |updated_ids| (optional) contains the ids for displays whose bounds have
  // changed. |minimum_offset_overlap| represents the minimum required overlap
  // between displays. Any overlap between displays will be fixed, and the
  // display placement will be fixed.
  void ApplyToDisplayList(Displays* display_list,
                          std::vector<int64_t>* updated_ids,
                          int minimum_offset_overlap);

  // Validates the layout object.
  static bool Validate(const DisplayIdList& list, const DisplayLayout& layout);

  std::vector<DisplayPlacement> placement_list;

  // True if multi displays should default to unified mode.
  bool default_unified;

  // The id of the display used as a primary display.
  int64_t primary_id;

  std::unique_ptr<DisplayLayout> Copy() const;

  // Swaps the primary display so it is |new_primary_id|. This modifies the
  // display placements such so they are anchored on |new_primary_id|.
  void SwapPrimaryDisplay(int64_t new_primary_id);

  // Test if the |layout| has the same placement list. Other fields such
  // as mirrored, primary_id are ignored.
  bool HasSamePlacementList(const DisplayLayout& layout) const;

  // Removes the display placements created for `display_id_list`.
  void RemoveDisplayPlacements(const DisplayIdList& display_id_list);

  // Returns string representation of the layout for debugging/testing.
  std::string ToString() const;

  // Returns the DisplayPlacement entry matching |display_id| if it exists,
  // otherwise returns a DisplayPlacement with an invalid display id.
  DisplayPlacement FindPlacementById(int64_t display_id) const;

  // Creates a display::DisplayPlacement value for |rectangle| relative to the
  // |reference| rectangle.
  static DisplayPlacement CreatePlacementForRectangles(
      const gfx::Rect& reference,
      const gfx::Rect& rectangle);

 private:
  // Apply the display placement to |display_list|.
  // Returns true if the display bounds were updated.
  static bool ApplyDisplayPlacement(const DisplayPlacement& placement,
                                    Displays* display_list,
                                    int minimum_offset_overlap);
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_LAYOUT_H_
