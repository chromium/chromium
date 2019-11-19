// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_DISPLAY_MANAGER_TEST_API_H_
#define UI_DISPLAY_TEST_DISPLAY_MANAGER_TEST_API_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/display/display_layout.h"
#include "ui/display/types/display_constants.h"

namespace gfx {
class Size;
}

namespace display {
class DisplayManager;
class ManagedDisplayInfo;

namespace test {

class DISPLAY_EXPORT DisplayManagerTestApi {
 public:
  explicit DisplayManagerTestApi(DisplayManager* display_manager);
  virtual ~DisplayManagerTestApi();

  void set_maximum_display(size_t maximum_display_num) {
    maximum_support_display_ = maximum_display_num;
  }
  void ResetMaximumDisplay();

  // Update the display configuration as given in |display_specs|. The format of
  // |display_spec| is a list of comma separated spec for each displays. Please
  // refer to the comment in |display::ManagedDisplayInfo::CreateFromSpec| for
  // the format of the display spec.
  void UpdateDisplay(const std::string& display_specs);

  // Set the 1st display as an internal display and returns the display Id for
  // the internal display.
  int64_t SetFirstDisplayAsInternalDisplay();

  // Don't update the display when the root window's size was changed.
  void DisableChangeDisplayUponHostResize();

  // Gets the internal ManagedDisplayInfo for a specific display id.
  const ManagedDisplayInfo& GetInternalManagedDisplayInfo(int64_t display_id);

  // Sets the touch support for |display_id|.
  void SetTouchSupport(int64_t display_id, Display::TouchSupport touch_support);

 private:
  friend class ScopedSetInternalDisplayId;
  // Sets the display id for internal display and
  // update the display mode list if necessary.
  void SetInternalDisplayId(int64_t id);

  // Indicate the maximum number of displays that chrome device can support.
  static size_t maximum_support_display_;

  DisplayManager* display_manager_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(DisplayManagerTestApi);
};

class DISPLAY_EXPORT ScopedSetInternalDisplayId {
 public:
  ScopedSetInternalDisplayId(DisplayManager* test_api, int64_t id);
  ~ScopedSetInternalDisplayId();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSetInternalDisplayId);
};

// Sets the display mode that matches the |resolution| for |display_id|.
DISPLAY_EXPORT bool SetDisplayResolution(DisplayManager* display_manager,
                                         int64_t display_id,
                                         const gfx::Size& resolution);

// Creates the dislpay layout from position and offset for the current
// display list. If you simply want to create a new layout that is
// independent of current displays, use DisplayLayoutBuilder or simply
// create a new DisplayLayout and set display id fields (primary, ids
// in placement) manually.
DISPLAY_EXPORT std::unique_ptr<DisplayLayout> CreateDisplayLayout(
    DisplayManager* display_manager,
    DisplayPlacement::Position position,
    int offset);

// Creates the DisplayIdList from ints.
DISPLAY_EXPORT DisplayIdList CreateDisplayIdList2(int64_t id1, int64_t id2);
DISPLAY_EXPORT DisplayIdList CreateDisplayIdListN(size_t count, ...);

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_TEST_DISPLAY_MANAGER_TEST_API_H_
