// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/linux/test/virtual_display_util_linux.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/randr_output_manager.h"

namespace {

// Appends a new screen with `resolution` and `dpi` to the specified desktop
// `layout`. Arranges horizontally left to right.
void AppendScreen(x11::RandRMonitorLayout& layout,
                  const gfx::Size& resolution,
                  const gfx::Vector2d& dpi) {
  // Find the rightmost screen layout.
  const x11::RandRMonitorConfig* rightmost_layout = nullptr;
  for (const auto& screen : layout.configs) {
    if (rightmost_layout == nullptr ||
        screen.rect().right() > rightmost_layout->rect().right()) {
      rightmost_layout = &screen;
    }
  }
  layout.configs.emplace_back(
      std::nullopt,
      gfx::Rect(rightmost_layout->rect().right() + 1,
                rightmost_layout->rect().y(), resolution.width(),
                resolution.height()),
      dpi);
}
}  // namespace

namespace display::test {

VirtualDisplayUtilLinux::VirtualDisplayUtilLinux(Screen* screen)
    : screen_(screen),
      randr_output_manager_(std::make_unique<x11::RandROutputManager>(
          /*output_name_prefix=*/"VDU_")),
      initial_layout_(randr_output_manager_->GetLayout()),
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
  // Check if XRandR is available with a sufficient number of connected outputs.
  // Skip base::nix::GetSessionType(...), which may return kTty instead of kX11
  // in SSH sessions with virtualized X11 environments.

  constexpr auto kConnected = static_cast<x11::RandR::RandRConnection>(0);
  constexpr auto kDisabled = static_cast<x11::RandR::Crtc>(0);
  x11::Connection* x11_connection = x11::Connection::Get();
  if (!x11_connection) {
    LOG(ERROR) << "X11 is not present.";
    return false;
  }
  x11::RandR& xrandr = x11_connection->randr();
  if (!xrandr.present()) {
    LOG(ERROR) << "XRandR is not present.";
    return false;
  }
  x11::Response<x11::RandR::GetScreenResourcesCurrentReply> screen_resources =
      xrandr.GetScreenResourcesCurrent({x11_connection->default_screen().root})
          .Sync();
  if (!screen_resources.reply) {
    LOG(ERROR) << "GetScreenResourcesCurrent failed.";
    return false;
  }
  int connected_and_disabled_outputs = 0;
  for (const auto& output : screen_resources.reply->outputs) {
    std::unique_ptr<x11::RandR::GetOutputInfoReply> output_reply =
        xrandr.GetOutputInfo(output, screen_resources.reply->config_timestamp)
            .Sync()
            .reply;
    if (output_reply && output_reply->connection == kConnected &&
        output_reply->crtc == kDisabled) {
      connected_and_disabled_outputs++;
    }
  }
  return connected_and_disabled_outputs >= kMaxDisplays;
}

int64_t VirtualDisplayUtilLinux::AddDisplay(
    const DisplayParams& display_params) {
  if (current_layout_.configs.size() - initial_layout_.configs.size() >
      kMaxDisplays) {
    LOG(ERROR) << "Cannot exceed " << kMaxDisplays << " virtual displays.";
    return kInvalidDisplayId;
  }
  CHECK(!current_layout_.configs.empty());
  last_requested_layout_ = current_layout_;
  AppendScreen(last_requested_layout_, display_params.resolution,
               display_params.dpi);
  randr_output_manager_->SetLayout(last_requested_layout_);
  size_t initial_detected_displays = detected_added_display_ids_.size();
  StartWaiting();
  CHECK_EQ(detected_added_display_ids_.size(), initial_detected_displays + 1u)
      << "Did not detect exactly one new display.";
  // Reconcile the added resizer display ID to the detected display::Display id.
  int64_t new_display_id = detected_added_display_ids_.back();
  x11::RandRMonitorLayout prev_layout = current_layout_;
  current_layout_ = randr_output_manager_->GetLayout();
  for (const auto& layout : current_layout_.configs) {
    auto was_added =
        std::find_if(prev_layout.configs.begin(), prev_layout.configs.end(),
                     [&](const x11::RandRMonitorConfig& prev) {
                       return prev.rect() == layout.rect();
                     });
    if (was_added == prev_layout.configs.end()) {
      display_id_to_randr_id_[new_display_id] = *layout.id();
    }
  }
  return new_display_id;
}

void VirtualDisplayUtilLinux::RemoveDisplay(int64_t display_id) {
  if (!display_id_to_randr_id_.contains(display_id)) {
    LOG(ERROR) << "Invalid display_id. Missing mapping for " << display_id
               << " to randr ID.";
    return;
  }
  last_requested_layout_ = current_layout_;
  x11::RandRMonitorConfig::ScreenId randr_id =
      display_id_to_randr_id_[display_id];
  std::erase_if(last_requested_layout_.configs,
                [&](const x11::RandRMonitorConfig& layout) {
                  return layout.id() == randr_id;
                });
  randr_output_manager_->SetLayout(last_requested_layout_);
  StartWaiting();
}

void VirtualDisplayUtilLinux::ResetDisplays() {
  last_requested_layout_ = initial_layout_;
  randr_output_manager_->SetLayout(last_requested_layout_);
  StartWaiting();
  current_layout_ = randr_output_manager_->GetLayout();
}

void VirtualDisplayUtilLinux::OnDisplayAdded(
    const display::Display& new_display) {
  detected_added_display_ids_.push_back(new_display.id());
  OnDisplayAddedOrRemoved(new_display.id());
}

void VirtualDisplayUtilLinux::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  for (const auto& display : removed_displays) {
    base::EraseIf(
        display_id_to_randr_id_,
        [&](std::pair<DisplayId, x11::RandRMonitorConfig::ScreenId>& pair) {
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
  // minus initial layout) is equal to the number of detected virtual displays.
  return last_requested_layout_.configs.size() -
             initial_layout_.configs.size() ==
         detected_added_display_ids_.size();
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
std::unique_ptr<VirtualDisplayUtil> VirtualDisplayUtil::TryCreate(
    Screen* screen) {
  if (!VirtualDisplayUtilLinux::IsAPIAvailable()) {
    return nullptr;
  }
  return std::make_unique<VirtualDisplayUtilLinux>(screen);
}

}  // namespace display::test
