// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/screen_mac_headless.h"

#import <AppKit/AppKit.h>

#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "components/headless/screen_info/headless_screen_info.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/switches.h"

namespace display {

namespace {

std::vector<headless::HeadlessScreenInfo> GetHeadlessScreenInfos() {
  std::vector<headless::HeadlessScreenInfo> screen_infos;

  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());

  if (command_line.HasSwitch(switches::kScreenInfo)) {
    const std::string switch_value =
        command_line.GetSwitchValueASCII(switches::kScreenInfo);
    base::expected<std::vector<headless::HeadlessScreenInfo>, std::string>
        screen_infos_or_error =
            headless::HeadlessScreenInfo::FromString(switch_value);
    CHECK(screen_infos_or_error.has_value()) << screen_infos_or_error.error();
    screen_infos = screen_infos_or_error.value();
  } else {
    screen_infos.push_back(headless::HeadlessScreenInfo());
  }

  return screen_infos;
}

}  // namespace

ScreenMacHeadless::ScreenMacHeadless() {
  CreateDisplayList();

  display::HeadlessScreenManager::Get()->SetDelegate(this);
}

ScreenMacHeadless::~ScreenMacHeadless() {
  display::HeadlessScreenManager::Get()->SetDelegate(nullptr);
}

void ScreenMacHeadless::CreateDisplayList() {
  std::vector<headless::HeadlessScreenInfo> screen_infos =
      GetHeadlessScreenInfos();
  CHECK(!screen_infos.empty());

  bool is_primary = true;
  base::flat_set<int64_t> internal_display_ids;
  for (const headless::HeadlessScreenInfo& it : screen_infos) {
    Display display(HeadlessScreenManager::GetNewDisplayId());
    display.set_label(it.label);
    display.set_color_depth(it.color_depth);

    display::HeadlessScreenManager::SetDisplayGeometry(
        display, it.bounds, it.work_area_insets, it.device_pixel_ratio);

    if (it.rotation) {
      CHECK(Display::IsValidRotation(it.rotation));
      display.SetRotationAsDegree(it.rotation);
    }

    if (it.is_internal) {
      internal_display_ids.insert(display.id());
    }

    ProcessDisplayChanged(display, is_primary);
    is_primary = false;
  }

  SetInternalDisplayIds(internal_display_ids);
}

gfx::Point ScreenMacHeadless::GetCursorScreenPoint() {
  return gfx::Point();
}

bool ScreenMacHeadless::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow ScreenMacHeadless::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  return gfx::NativeWindow();
}

gfx::NativeWindow ScreenMacHeadless::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  return gfx::NativeWindow();
}

Display ScreenMacHeadless::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  if (window && GetNumDisplays() > 1) {
    if (NSWindow* ns_window = window.GetNativeNSWindow()) {
      const gfx::Rect bounds = gfx::ScreenRectFromNSRect([ns_window frame]);
      return GetDisplayMatching(bounds);
    }
  }
  return GetPrimaryDisplay();
}

bool ScreenMacHeadless::IsHeadless() const {
  return true;
}

int64_t ScreenMacHeadless::AddDisplay(const Display& display) {
  Display new_display(display);
  new_display.set_id(HeadlessScreenManager::GetNewDisplayId());

  bool is_primary = display_list().displays().empty();
  display_list().AddDisplay(new_display, is_primary
                                             ? DisplayList::Type::PRIMARY
                                             : DisplayList::Type::NOT_PRIMARY);
  return new_display.id();
}

void ScreenMacHeadless::RemoveDisplay(int64_t display_id) {
  display_list().RemoveDisplay(display_id);
  display::RemoveInternalDisplayId(display_id);
}

}  // namespace display
