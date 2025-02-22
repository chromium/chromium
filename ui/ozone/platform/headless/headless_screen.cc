// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_screen.h"

#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/headless/screen_info/headless_screen_info.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_finder.h"
#include "ui/display/util/display_util.h"
#include "ui/ozone/public/ozone_switches.h"

using headless::HeadlessScreenInfo;

namespace ui {

namespace {

// By default headless screen has 1x1 size and 1.0 scale factor. Headless
// screen size can be overridden using --ozone-override-screen-size switch.
//
// More complex headless screen configuration (including multiple screens)
// can be specified using the --screen-info command line switch.
// See //components/headless/screen_info/headless_screen_info.h for details.

// Ozone/headless display defaults.
constexpr int64_t kHeadlessDisplayIdBase = 1;
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

gfx::Rect GetHeadlessDisplayBounds() {
  gfx::Rect bounds(kHeadlessDisplaySize);

  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
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

std::vector<HeadlessScreenInfo> GetScreenInfo() {
  std::vector<HeadlessScreenInfo> screen_info;

  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  if (command_line.HasSwitch(switches::kScreenInfo)) {
    const std::string switch_value =
        command_line.GetSwitchValueASCII(switches::kScreenInfo);
    auto screen_info_or_error = HeadlessScreenInfo::FromString(switch_value);
    CHECK(screen_info_or_error.has_value()) << screen_info_or_error.error();
    screen_info = screen_info_or_error.value();
  } else {
    screen_info.push_back(
        HeadlessScreenInfo({.bounds = GetHeadlessDisplayBounds(),
                            .device_pixel_ratio = kHeadlessDisplayScale}));
  }
  return screen_info;
}

}  // namespace

HeadlessScreen::HeadlessScreen() {
  std::vector<HeadlessScreenInfo> screen_info = GetScreenInfo();

  base::flat_set<int64_t> internal_display_ids;
  display::DisplayList::Type type = display::DisplayList::Type::PRIMARY;
  for (const auto& it : screen_info) {
    static int64_t synthesized_display_id = kHeadlessDisplayIdBase;
    display::Display display(synthesized_display_id++);
    display.set_label(it.label);
    display.set_color_depth(it.color_depth);
    display.SetScaleAndBounds(it.device_pixel_ratio, it.bounds);

    if (!it.work_area_insets.IsEmpty()) {
      display.UpdateWorkAreaFromInsets(it.work_area_insets);
    }

    if (it.rotation) {
      CHECK(display::Display::IsValidRotation(it.rotation));
      display.SetRotationAsDegree(it.rotation);
    }

    if (it.is_internal) {
      internal_display_ids.insert(display.id());
    }

    is_natural_landscape_map_.insert({display.id(), display.is_landscape()});

    display_list_.AddDisplay(display, type);

    type = display::DisplayList::Type::NOT_PRIMARY;
  }

  display::SetInternalDisplayIds(std::move(internal_display_ids));
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

}  // namespace ui
