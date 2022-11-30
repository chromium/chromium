// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_SCREEN_BASE_H_
#define UI_DISPLAY_SCREEN_BASE_H_

#include <vector>

#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/display/display_list.h"
#include "ui/display/screen.h"

namespace display {

// Simple screen implementation with a display list.
class DISPLAY_EXPORT ScreenBase : public Screen {
 public:
  ScreenBase();

  ScreenBase(const ScreenBase&) = delete;
  ScreenBase& operator=(const ScreenBase&) = delete;

  ~ScreenBase() override;

  DisplayList& display_list() { return display_list_; }
  const DisplayList& display_list() const { return display_list_; }

  // Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore) override;
  Display GetPrimaryDisplay() const override;
  Display GetDisplayNearestWindow(gfx::NativeWindow window) const override;
  Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  int GetNumDisplays() const override;
  const std::vector<Display>& GetAllDisplays() const override;
  Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  void AddObserver(DisplayObserver* observer) override;
  void RemoveObserver(DisplayObserver* observer) override;
  void SetPanelRotationForTesting(int64_t display_id,
                                  Display::Rotation rotation) override;

  bool HasDisplayObservers() const;

 protected:
  // Invoked when a display changed in some way, including being added.
  // If |is_primary| is true, |changed_display| is the primary display.
  void ProcessDisplayChanged(const Display& changed_display, bool is_primary);

 private:
  DisplayList display_list_;
};

}  // namespace display

#endif  // UI_DISPLAY_SCREEN_BASE_H_
