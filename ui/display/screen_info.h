// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_SCREEN_INFO_H_
#define UI_DISPLAY_SCREEN_INFO_H_

#include <optional>
#include <string>

#include "ui/display/display_export.h"
#include "ui/display/mojom/screen_orientation.mojom-shared.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"

namespace display {

// This structure roughly parallels display::Display. It may be desirable to
// deprecate derived counterparts of ui/display types; see crbug.com/1208469.
struct DISPLAY_EXPORT ScreenInfo {
  // Device scale factor. Specifies the ratio between physical and logical
  // pixels.
  float device_scale_factor = 1.f;

  // The color spaces used by output display for various content types.
  gfx::DisplayColorSpaces display_color_spaces;

  // The screen depth in bits per pixel.
  int depth = 0;

  // The bits per colour component. This assumes that the colours are balanced
  // equally.
  int depth_per_component = 0;

  // This can be true for black and white printers
  bool is_monochrome = false;

  // This is set from the rcMonitor member of MONITORINFOEX, to whit:
  //   "A RECT structure that specifies the display monitor rectangle,
  //   expressed in virtual-screen coordinates. Note that if the monitor
  //   is not the primary display monitor, some of the rectangle's
  //   coordinates may be negative values."
  gfx::Rect rect;

  // This is set from the rcWork member of MONITORINFOEX, to whit:
  //   "A RECT structure that specifies the work area rectangle of the
  //   display monitor that can be used by applications, expressed in
  //   virtual-screen coordinates. Windows uses this rectangle to
  //   maximize an application on the monitor. The rest of the area in
  //   rcMonitor contains system windows such as the task bar and side
  //   bars. Note that if the monitor is not the primary display monitor,
  //   some of the rectangle's coordinates may be negative values".
  gfx::Rect available_rect;

  // This is the orientation 'type' or 'name', as in landscape-primary or
  // portrait-secondary for examples.
  // See ui/display/mojom/screen_orientation.mojom for the full list.
  mojom::ScreenOrientation orientation_type =
      mojom::ScreenOrientation::kUndefined;

  // This is the orientation angle of the displayed content in degrees.
  // It is the opposite of the physical rotation.
  // TODO(crbug.com/41387359): we should use an enum rather than a number here.
  uint16_t orientation_angle = 0;

  // Whether this Screen is part of a multi-screen extended visual workspace.
  bool is_extended = false;

  // Whether this screen is designated as the 'primary' screen by the OS
  // (otherwise it is a 'secondary' screen).
  bool is_primary = false;

  // Whether this screen is an 'internal' panel built into the device, like a
  // laptop display (otherwise it is 'external', like a wired monitor).
  bool is_internal = false;

  // A user-friendly label for the screen, determined by the platform.
  std::string label;

  // Not web-exposed; the display::Display::id(), for internal tracking only.
  int64_t display_id = kDefaultDisplayId;

  // Expose this constant to Blink.
  static constexpr int64_t kInvalidDisplayId = display::kInvalidDisplayId;

  ScreenInfo();
  ScreenInfo(const ScreenInfo& other);
  ~ScreenInfo();
  ScreenInfo& operator=(const ScreenInfo& other);
  bool operator==(const ScreenInfo& other) const;
  bool operator!=(const ScreenInfo& other) const;

  // Returns a string representation of the screen.
  std::string ToString() const;
};

}  // namespace display

#endif  // UI_DISPLAY_SCREEN_INFO_H_
