// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_util.h"

#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/icc_profile.h"

namespace display {

// static
void DisplayUtil::DisplayToScreenInfo(ScreenInfo* screen_info,
                                      const Display& display) {
  screen_info->rect = display.bounds();
  // TODO(husky): Remove any Android system controls from availableRect.
  screen_info->available_rect = display.work_area();
  screen_info->device_scale_factor = display.device_scale_factor();
  screen_info->display_color_spaces = display.GetColorSpaces();
  screen_info->depth = display.color_depth();
  screen_info->depth_per_component = display.depth_per_component();
  screen_info->is_monochrome = display.is_monochrome();

  // TODO(crbug.com/41478398): Expose panel orientation via a proper web
  // API instead of window.screen.orientation.angle.
  screen_info->orientation_angle = display.PanelRotationAsDegree();
#if defined(USE_AURA)
  // The Display rotation and the ScreenInfo orientation are not the same
  // angle. The former is the physical display rotation while the later is the
  // rotation required by the content to be shown properly on the screen, in
  // other words, relative to the physical display.
  // Spec: https://w3c.github.io/screen-orientation/#dom-screenorientation-angle
  // TODO(ccameron): Should this apply to macOS? Should this be reconciled at a
  // higher level (say, in conversion to ScreenInfo)?
  if (screen_info->orientation_angle == 90)
    screen_info->orientation_angle = 270;
  else if (screen_info->orientation_angle == 270)
    screen_info->orientation_angle = 90;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  screen_info->orientation_type = GetOrientationTypeForMobile(display);
#else
  screen_info->orientation_type = GetOrientationTypeForDesktop(display);
#endif

  // TODO(crbug.com/1194700 and crbug.com/1182855): Use cross-process screen
  // info caches, not local-process info, for child frames and Mac's shim.
  auto* screen = Screen::GetScreen();
  // Some tests are run with no Screen initialized.
  screen_info->is_extended = screen && screen->GetNumDisplays() > 1;
  screen_info->is_primary =
      screen && (screen->GetPrimaryDisplay().id() == display.id());
  screen_info->is_internal = display.IsInternal();
  screen_info->display_id = display.id();
  screen_info->label = display.label();
}

// static
void DisplayUtil::GetDefaultScreenInfo(ScreenInfo* screen_info) {
  return GetNativeViewScreenInfo(screen_info, gfx::NativeView());
}

// static
void DisplayUtil::GetNativeViewScreenInfo(ScreenInfo* screen_info,
                                          gfx::NativeView native_view) {
  // Some tests are run with no Screen initialized.
  Screen* screen = Screen::GetScreen();
  if (!screen) {
    *screen_info = ScreenInfo();
    return;
  }
  Display display = native_view ? screen->GetDisplayNearestView(native_view)
                                : screen->GetPrimaryDisplay();
  DisplayToScreenInfo(screen_info, display);
}

// static
mojom::ScreenOrientation DisplayUtil::GetOrientationTypeForMobile(
    const Display& display) {
  int angle = display.PanelRotationAsDegree();
  const gfx::Rect& bounds = display.bounds();

  // Whether the device's natural orientation is portrait.
  bool natural_portrait = false;
  if (angle == 0 || angle == 180)  // The device is in its natural orientation.
    natural_portrait = bounds.height() >= bounds.width();
  else
    natural_portrait = bounds.height() <= bounds.width();

  switch (angle) {
    case 0:
      return natural_portrait ? mojom::ScreenOrientation::kPortraitPrimary
                              : mojom::ScreenOrientation::kLandscapePrimary;
    case 90:
      return natural_portrait ? mojom::ScreenOrientation::kLandscapePrimary
                              : mojom::ScreenOrientation::kPortraitSecondary;
    case 180:
      return natural_portrait ? mojom::ScreenOrientation::kPortraitSecondary
                              : mojom::ScreenOrientation::kLandscapeSecondary;
    case 270:
      return natural_portrait ? mojom::ScreenOrientation::kLandscapeSecondary
                              : mojom::ScreenOrientation::kPortraitPrimary;
    default:
      NOTREACHED_IN_MIGRATION();
      return mojom::ScreenOrientation::kPortraitPrimary;
  }
}

// static
mojom::ScreenOrientation DisplayUtil::GetOrientationTypeForDesktop(
    const Display& display) {
  static int primary_landscape_angle = -1;
  static int primary_portrait_angle = -1;

  int angle = display.PanelRotationAsDegree();
  const gfx::Rect& bounds = display.bounds();
  bool is_portrait = bounds.height() >= bounds.width();

  if (is_portrait && primary_portrait_angle == -1)
    primary_portrait_angle = angle;

  if (!is_portrait && primary_landscape_angle == -1)
    primary_landscape_angle = angle;

  if (is_portrait) {
    return primary_portrait_angle == angle
               ? mojom::ScreenOrientation::kPortraitPrimary
               : mojom::ScreenOrientation::kPortraitSecondary;
  }

  return primary_landscape_angle == angle
             ? mojom::ScreenOrientation::kLandscapePrimary
             : mojom::ScreenOrientation::kLandscapeSecondary;
}

// static
uint32_t DisplayUtil::GetAudioFormats() {
  // Audio passthrough is only supported with a single display. If multiple
  // displays are attached, audio passthrough will not be enabled.
  Screen* screen = Screen::GetScreen();
  if (screen) {
    auto display = screen->GetAllDisplays();
    if (display.size() == 1) {
      return display.front().audio_formats();
    }
  }
  return 0;
}

}  // namespace display
