// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_screen.h"

#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "ui/display/tablet_state.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {
// Ozone/headless display defaults.
constexpr int64_t kHeadlessDisplayId = 1;
constexpr float kHeadlessDisplayScale = 1.0f;
constexpr gfx::Size kHeadlessDisplaySize(1, 1);

// Parse comma-separated screen width and height.
bool ParseScreenSize(const std::string& screen_size, int* width, int* height) {
  std::vector<std::string_view> width_and_height = base::SplitStringPiece(
      screen_size, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (width_and_height.size() != 2)
    return false;

  if (!base::StringToInt(width_and_height[0], width) ||
      !base::StringToInt(width_and_height[1], height)) {
    return false;
  }

  return true;
}

gfx::Rect GetDisplayBounds() {
  gfx::Rect bounds(kHeadlessDisplaySize);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kOzoneOverrideScreenSize)) {
    int width, height;
    std::string screen_size =
        command_line.GetSwitchValueASCII(switches::kOzoneOverrideScreenSize);
    if (ParseScreenSize(screen_size, &width, &height)) {
      bounds.set_size(gfx::Size(width, height));
    }
  }

  return bounds;
}

}  // namespace

HeadlessScreen::HeadlessScreen() {
  display::Display display(kHeadlessDisplayId);
  display.SetScaleAndBounds(kHeadlessDisplayScale, GetDisplayBounds());
  display_list_.AddDisplay(display, display::DisplayList::Type::PRIMARY);
}

HeadlessScreen::~HeadlessScreen() = default;

const std::vector<display::Display>& HeadlessScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display HeadlessScreen::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  CHECK(iter != display_list_.displays().end(), base::NotFatalUntil::M130);
  return *iter;
}

display::Display HeadlessScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return GetPrimaryDisplay();
}

gfx::Point HeadlessScreen::GetCursorScreenPoint() const {
  return gfx::Point();
}

gfx::AcceleratedWidget HeadlessScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  return gfx::kNullAcceleratedWidget;
}

display::Display HeadlessScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return GetPrimaryDisplay();
}

display::Display HeadlessScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return GetPrimaryDisplay();
}

void HeadlessScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void HeadlessScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
display::TabletState HeadlessScreen::GetTabletState() const {
  return display::TabletState::kInClamshellMode;
}
#endif

}  // namespace ui
