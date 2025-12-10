// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/display_link_mac_mojo.h"

#include <utility>

#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/mac/ca_display_link_mac.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ui {

// static
bool DisplayLinkMacMojo::SupportsDisplayLinkMacInBrowser() {
  // CADisplayLink is available only on MacOS 14.0+.
  if (!@available(macos 14.0, *)) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      display::features::kCADisplayLinkInBrowser);
}

DisplayLinkMacMojo::DisplayLinkMacMojo(
    viz::HostFrameSinkManager* host_frame_sink_manager)
    : base::Thread("VSyncThread") {
  DETACH_FROM_SEQUENCE(vsync_thread_sequence_checker_);

  // Start the VSync thread.
  base::Thread::Options thread_options;
  // CoreAnimation CADisplayLink only works with MessagePumpType::NS_RUNLOOP.
  thread_options.message_pump_type = base::MessagePumpType::NS_RUNLOOP;
  thread_options.thread_type = base::ThreadType::kDisplayCritical;
  StartWithOptions(base::Thread::Options(std::move(thread_options)));

  // To ensure VSyncThread task_runner() is valid, call ConnectVSyncIpc() after
  // calling StartWithOptions().
  ConnectVSyncIpc(host_frame_sink_manager);

  // Display AddObserver can only be called on the browser main thread.
  DCHECK(display::Screen::HasScreen());
  display::Screen::Get()->AddObserver(this);
}

DisplayLinkMacMojo::~DisplayLinkMacMojo() {
  if (display::Screen::HasScreen()) {
    display::Screen::Get()->RemoveObserver(this);
  }

  // Stop the VSync thread.
  Stop();
}

void DisplayLinkMacMojo::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);

  // Create CADisplayLink for all displays.
  const std::vector<display::Display>& displays =
      display::Screen::Get()->GetAllDisplays();
  for (const auto& display : displays) {
    DisplayAddedOnVSyncThread(display.id());
  }
}

void DisplayLinkMacMojo::CleanUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);

  // These are created on the VSync thread and must be destroyed on the same
  // thread.
  vsync_callbacks_.clear();
  display_links_.clear();
}

// Called on the browser main thread
void DisplayLinkMacMojo::ConnectVSyncIpc(
    viz::HostFrameSinkManager* host_frame_sink_manager) {
  CHECK(host_frame_sink_manager);
}

// Called on the VSync thread directly from IPC.
void DisplayLinkMacMojo::NeedsBeginFrameWithId(int64_t display_id,
                                               bool needs_begin_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);

  auto found = display_links_.find(display_id);
  if (found == display_links_.end()) {
    DLOG(ERROR) << "DisplayLinkMacMojo::NeedsBeginFrameWithId() Display id "
                << display_id << " not found! Skip needs_begin_frames().";
    return;
  }

  DisplayLinkMac* display_link_mac = found->second.get();
  VSyncCallbackMac* callback = vsync_callbacks_[display_id].get();

  if (needs_begin_frames && !callback) {
    std::unique_ptr<VSyncCallbackMac> vsync_callback =
        display_link_mac->RegisterCallback(
            base::BindRepeating(&DisplayLinkMacMojo::OnDisplayLinkVSyncCallback,
                                base::Unretained(this), display_id));

    if (!vsync_callback) {
      DLOG(ERROR) << "CADisplayLink RegisterCallback failed!";
      DisplaysRemovedOnVSyncThread({display_id});

      // TODO: Notify Viz of this failure.
      return;
    }

    vsync_callbacks_[display_id] = std::move(vsync_callback);
  } else if (!needs_begin_frames && callback) {
    vsync_callbacks_[display_id] = nullptr;
  }
}

void DisplayLinkMacMojo::SetPreferredInterval(base::TimeDelta interval) {}

void DisplayLinkMacMojo::OnDisplayLinkVSyncCallback(int64_t display_id,
                                                    VSyncParamsMac params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);
  // With the delay from OnDisplaysRemoved() to DisplaysRemovedOnVSyncThread()
  // or just the delay in CoreAnimation CADisplayLink, we might receive a VSync
  // callback after the display is removed in a rare case, But this is fine. We
  // can still forward this extra VSync to Viz/GPU because `VSyncCallbackMac`
  // uses weak_ptr. If the frame_sink or BeginFrameSource for this display is
  // destroyed, it's handled.
}

void DisplayLinkMacMojo::OnDisplayAdded(const display::Display& new_display) {
  int64_t display_id = new_display.id();
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DisplayLinkMacMojo::DisplayAddedOnVSyncThread,
                                base::Unretained(this), display_id));
}

void DisplayLinkMacMojo::DisplayAddedOnVSyncThread(int64_t display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);

  if (auto it = display_links_.find(display_id); it != display_links_.end()) {
    // This display has been added.
    return;
  }

  scoped_refptr<DisplayLinkMac> display_link_mac =
      CADisplayLinkMac::GetForDisplay(display_id);
  if (!display_link_mac) {
    return;
  }

  auto result = display_links_.insert(
      std::make_pair(display_id, std::move(display_link_mac)));
  bool inserted = result.second;
  DCHECK(inserted);

  vsync_callbacks_.insert(std::make_pair(display_id, nullptr));
}

void DisplayLinkMacMojo::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  std::vector<int64_t> display_ids;
  for (auto& removed_display : removed_displays) {
    display_ids.push_back(removed_display.id());
  }

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DisplayLinkMacMojo::DisplaysRemovedOnVSyncThread,
                     base::Unretained(this), std::move(display_ids)));
}

void DisplayLinkMacMojo::DisplaysRemovedOnVSyncThread(
    std::vector<int64_t> display_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);

  for (auto display_id : display_ids) {
    if (auto it = display_links_.find(display_id); it == display_links_.end()) {
      // This display has been removed.
      continue;
    }

    vsync_callbacks_.erase(display_id);
    display_links_.erase(display_id);
  }
}

}  // namespace ui
