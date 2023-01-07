// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_FAKE_DISPLAY_CONTROLLER_H_
#define UI_DISPLAY_TYPES_FAKE_DISPLAY_CONTROLLER_H_

#include <stdint.h>

#include <memory>

#include "ui/display/types/display_types_export.h"

namespace gfx {
class Size;
}

namespace display {

class DisplaySnapshot;

// Controls the fake display state. Provided by the NativeDisplayDelegate if
// it is intended for use off device where there are no physical displays and
// we need to fake the display state.
class DISPLAY_TYPES_EXPORT FakeDisplayController {
 public:
  // Adds a fake display with the specified size, returns the display id or
  // |kInvalidDisplayId| if it fails.
  virtual int64_t AddDisplay(const gfx::Size& display_size) = 0;

  // Adds |display| to the list of displays and returns true if successful. Will
  // fail if an existing display has the same id as |display|.
  virtual bool AddDisplay(std::unique_ptr<DisplaySnapshot> display) = 0;

  // Removes a fake display with specified id, returns true if successful.
  virtual bool RemoveDisplay(int64_t display_id) = 0;

 protected:
  virtual ~FakeDisplayController() {}
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_FAKE_DISPLAY_CONTROLLER_H_
