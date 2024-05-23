// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/linux/test/virtual_display_util_linux.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "remoting/host/desktop_geometry.h"
#include "remoting/host/x11_desktop_resizer.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace {

// Appends a new screen with `resolution` to the specified desktop
// `layout`. Arranges horizontally left to right.
void AppendScreen(remoting::DesktopLayoutSet& layout,
                  const remoting::DesktopResolution& resolution) {
  // Find the rightmost screen layout.
  const remoting::DesktopLayout* rightmost_layout = nullptr;
  for (const auto& screen : layout.layouts) {
    if (rightmost_layout == nullptr ||
        screen.rect().right() > rightmost_layout->rect().right()) {
      rightmost_layout = &screen;
    }
  }
  layout.layouts.emplace_back(
      std::nullopt,
      gfx::Rect(rightmost_layout->rect().right() + 1,
                rightmost_layout->position_y(), resolution.dimensions().width(),
                resolution.dimensions().height()),
      resolution.dpi());
}
}  // namespace

namespace display::test {

struct DisplayParams {
  explicit DisplayParams(remoting::DesktopResolution resolution)
      : resolution(resolution) {}
  remoting::DesktopResolution resolution;
};

VirtualDisplayUtilLinux::VirtualDisplayUtilLinux(Screen* screen)
    : screen_(screen),
      desktop_resizer_(std::make_unique<remoting::X11DesktopResizer>()),
      initial_layout_(desktop_resizer_->GetLayout()),
      current_layout_(initial_layout_) {
  CHECK(screen_);
  screen_->AddObserver(this);
}
VirtualDisplayUtilLinux::~VirtualDisplayUtilLinux() {
  ResetDisplays();
  screen_->RemoveObserver(this);
}

// static
bool VirtualDisplayUtilLinux::IsAPIAvailable() {
  // Wayland is currently unimplemented. Note that this detection uses
  // XDG_SESSION_TYPE environment variable. When running in a pure SSH / virtual
  // X server, it may be necessary to manually set XDG_SESSION_TYPE=x11 in the
  // command.
  static base::nix::SessionType session_type =
      base::nix::GetSessionType(*base::Environment::Create());
  return session_type == base::nix::SessionType::kX11;
}

int64_t VirtualDisplayUtilLinux::AddDisplay(
    uint8_t id,
    const DisplayParams& display_params) {
  if (requested_ids_to_display_ids_.contains(id) ||
      std::find(requested_ids_.begin(), requested_ids_.end(), id) !=
          requested_ids_.end()) {
    LOG(ERROR) << "Virtual display with id " << id
               << " already exists or requested.";
    return kInvalidDisplayId;
  }
  if (current_layout_.layouts.size() - initial_layout_.layouts.size() >
      kMaxDisplays) {
    LOG(ERROR) << "Cannot exceed " << kMaxDisplays << " virtual displays.";
    return kInvalidDisplayId;
  }
  CHECK(!current_layout_.layouts.empty());
  last_requested_layout_ = current_layout_;
  AppendScreen(last_requested_layout_, display_params.resolution);
  requested_ids_.push_back(id);
  desktop_resizer_->SetVideoLayout(last_requested_layout_);
  detected_added_display_ids_.clear();
  StartWaiting();
  CHECK_EQ(detected_added_display_ids_.size(), 1u)
      << "Did not detect exactly one new display.";
  // Reconcile the added resizer display ID to the detected display::Display id.
  int64_t new_display_id = detected_added_display_ids_.front();
  detected_added_display_ids_.pop_front();
  remoting::DesktopLayoutSet prev_layout = current_layout_;
  current_layout_ = desktop_resizer_->GetLayout();
  for (const auto& layout : current_layout_.layouts) {
    auto was_added =
        std::find_if(prev_layout.layouts.begin(), prev_layout.layouts.end(),
                     [&](const remoting::DesktopLayout& prev) {
                       return prev.rect() == layout.rect();
                     });
    if (was_added == prev_layout.layouts.end()) {
      display_id_to_resizer_id_[new_display_id] = *layout.screen_id();
    }
  }
  return new_display_id;
}
void VirtualDisplayUtilLinux::RemoveDisplay(int64_t display_id) {
  if (!display_id_to_resizer_id_.contains(display_id)) {
    LOG(ERROR) << "Invalid display_id. Missing mapping for " << display_id
               << " to resizer ID.";
    return;
  }
  last_requested_layout_ = current_layout_;
  std::erase_if(last_requested_layout_.layouts,
                [&](const remoting::DesktopLayout& layout) {
                  return layout.screen_id() ==
                         display_id_to_resizer_id_[display_id];
                });
  desktop_resizer_->SetVideoLayout(last_requested_layout_);
  StartWaiting();
}
void VirtualDisplayUtilLinux::ResetDisplays() {
  last_requested_layout_ = initial_layout_;
  desktop_resizer_->SetVideoLayout(last_requested_layout_);
  StartWaiting();
  current_layout_ = desktop_resizer_->GetLayout();
}

void VirtualDisplayUtilLinux::OnDisplayAdded(
    const display::Display& new_display) {
  // TODO(crbug.com/40257169): Support adding multiple displays at a time, or
  // ignoring external display configuration changes.
  CHECK_EQ(requested_ids_.size(), 1u)
      << "An extra display was detected that was either not requested by this "
         "controller, or multiple displays were requested concurrently. This "
         "is not supported.";
  detected_added_display_ids_.push_back(new_display.id());
  uint8_t requested_id = requested_ids_.front();
  requested_ids_.pop_front();
  requested_ids_to_display_ids_[requested_id] = new_display.id();
  OnDisplayAddedOrRemoved(new_display.id());
}

void VirtualDisplayUtilLinux::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  for (const auto& display : removed_displays) {
    base::EraseIf(requested_ids_to_display_ids_,
                  [&](std::pair<uint8_t, int64_t>& pair) {
                    return pair.second == display.id();
                  });
    base::EraseIf(display_id_to_resizer_id_,
                  [&](std::pair<DisplayId, ResizerDisplayId>& pair) {
                    return pair.first == display.id();
                  });
    base::EraseIf(detected_added_display_ids_,
                  [&](DisplayId& id) { return id == display.id(); });
    OnDisplayAddedOrRemoved(display.id());
  }
}

void VirtualDisplayUtilLinux::OnDisplayAddedOrRemoved(int64_t id) {
  if (!RequestedLayoutIsSet()) {
    return;
  }
  StopWaiting();
}

bool VirtualDisplayUtilLinux::RequestedLayoutIsSet() {
  // Checks that the number of virtual displays (delta of last requested layout
  // minus initial layout) is equal to the number of requested displays.
  return last_requested_layout_.layouts.size() -
             initial_layout_.layouts.size() ==
         requested_ids_to_display_ids_.size();
}

void VirtualDisplayUtilLinux::StartWaiting() {
  CHECK(!run_loop_);
  if (RequestedLayoutIsSet()) {
    return;
  }
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}
void VirtualDisplayUtilLinux::StopWaiting() {
  CHECK(run_loop_);
  run_loop_->Quit();
}

// static
const DisplayParams VirtualDisplayUtilLinux::k1920x1080 = DisplayParams(
    remoting::DesktopResolution(gfx::Size(1920, 1080), gfx::Vector2d(96, 96)));
const DisplayParams VirtualDisplayUtilLinux::k1024x768 = DisplayParams(
    remoting::DesktopResolution(gfx::Size(1024, 768), gfx::Vector2d(96, 96)));

// VirtualDisplayUtil definitions:
const DisplayParams VirtualDisplayUtil::k1920x1080 =
    VirtualDisplayUtilLinux::k1920x1080;
const DisplayParams VirtualDisplayUtil::k1024x768 =
    VirtualDisplayUtilLinux::k1024x768;

// static
std::unique_ptr<VirtualDisplayUtil> VirtualDisplayUtil::TryCreate(
    Screen* screen) {
  if (!VirtualDisplayUtilLinux::IsAPIAvailable()) {
    return nullptr;
  }
  return std::make_unique<VirtualDisplayUtilLinux>(screen);
}

}  // namespace display::test
