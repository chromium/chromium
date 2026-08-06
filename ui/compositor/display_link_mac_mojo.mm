// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/display_link_mac_mojo.h"

#include <utility>

#include "base/task/bind_post_task.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/mac/ca_display_link_mac.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ui {

DisplayLinkMacMojo::DisplayLinkMacMojo(
    viz::HostFrameSinkManager* host_frame_sink_manager)
    : base::Thread("VSyncThread") {
  DETACH_FROM_SEQUENCE(vsync_thread_sequence_checker_);

  // Start the VSync thread.
  base::Thread::Options thread_options;
  // CoreAnimation CADisplayLink only works with MessagePumpType::NS_RUNLOOP.
  thread_options.message_pump_type = base::MessagePumpType::NS_RUNLOOP;
  thread_options.thread_type = base::ThreadType::kPresentation;
  StartWithOptions(base::Thread::Options(std::move(thread_options)));

  // To ensure VSyncThread task_runner() is valid, StartWithOptions() must be
  // called before connecting the VSync IPC and adding the display observer,
  // which are done on the browser main thread.
  ConnectVSyncIpcAndAddDisplayObserver(host_frame_sink_manager);
}

DisplayLinkMacMojo::~DisplayLinkMacMojo() {
  display_observer_.reset();

  // Stop the VSync thread.
  Stop();
}

void DisplayLinkMacMojo::InitDisplaysOnVSyncThread(
    std::vector<int64_t> display_ids,
    mojo::Remote<viz::mojom::ExternalBeginFrameController>
        external_begin_frame_controller,
    std::unique_ptr<
        mojo::Receiver<viz::mojom::ExternalBeginFrameControllerClient>>
        client_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);
  external_begin_frame_controller_ = std::move(external_begin_frame_controller);
  client_receiver_ = std::move(client_receiver);

  // Create CADisplayLink for all displays.
  for (int64_t display_id : display_ids) {
    DisplayAddedOnVSyncThread(display_id);
  }
}

void DisplayLinkMacMojo::CleanUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);
  ResetStateOnVSyncThread();
}

void DisplayLinkMacMojo::ResetStateOnVSyncThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);

  // Created on the VSync thread and must be destroyed on the same thread.
  vsync_callbacks_.clear();
  display_links_.clear();

  // The remote and receiver are connected to IPC to issue and receive mojom
  // interface calls on the VSync thread. These dtors are required to run on the
  // same thread.
  external_begin_frame_controller_.reset();
  client_receiver_.reset();
}

// Called on the browser main thread.
// Handles GPU process loss, even if multiple losses occur in rapid succession.
// Resource destruction and recreation are serialized across both threads to
// avoid races.
void DisplayLinkMacMojo::OnGpuProcessLost(
    viz::HostFrameSinkManager* host_frame_sink_manager) {
  pending_gpu_lost_count_++;

  display_observer_.reset();

  // Ensure the WeakPtr are created and bound to the sequence on CrBrowserMain.
  // The dereferencing (checking if the pointer is still valid) and invalidation
  // happens on the Main thread only. A WeakPtr is passed to the VSync thread
  // only to be used as an argument for the recovery task
  // (ContinueGpuProcessLostOnMainThread).
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DisplayLinkMacMojo::OnGpuProcessLostOnVSyncThread,
                     base::Unretained(this), host_frame_sink_manager,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     weak_ptr_factory_.GetWeakPtr()));
}

// Called on the browser main thread.
void DisplayLinkMacMojo::ContinueGpuProcessLostOnMainThread(
    viz::HostFrameSinkManager* host_frame_sink_manager) {
  pending_gpu_lost_count_--;
  if (pending_gpu_lost_count_ > 0) {
    return;
  }

  ConnectVSyncIpcAndAddDisplayObserver(host_frame_sink_manager);
}

void DisplayLinkMacMojo::OnGpuProcessLostOnVSyncThread(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    base::WeakPtr<DisplayLinkMacMojo> weak_ptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);

  ResetStateOnVSyncThread();

  // After destroying the old objects on the VSync thread, return to the main
  // thread to continue the recovery process. We use a WeakPtr because recovery
  // should only proceed if this instance has not been destroyed.
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DisplayLinkMacMojo::ContinueGpuProcessLostOnMainThread,
                     weak_ptr, host_frame_sink_manager));
}

// Called on the browser main thread
void DisplayLinkMacMojo::ConnectVSyncIpcAndAddDisplayObserver(
    viz::HostFrameSinkManager* host_frame_sink_manager) {
  CHECK(host_frame_sink_manager);

  // Params to connect with Viz
  auto params = viz::mojom::CompositorDisplayLinkParams::New();

  // Set up ExternalBeginFrameController for DisplayLinkMacMojo to
  // issue VSync call via IPC to Viz when receiving a CADisplayLink callback.
  mojo::Remote<viz::mojom::ExternalBeginFrameController>
      external_begin_frame_controller;
  // Move pending receiver to params.
  params->external_begin_frame_controller =
      external_begin_frame_controller.BindNewPipeAndPassReceiver(task_runner());

  // Set up ExternalBeginFrameControllerClient for Viz to call
  // NeedsBeginFrameWithId() via IPC to request DisplayLinkMacMojo for VSync
  // start/stop.
  mojo::PendingRemote<viz::mojom::ExternalBeginFrameControllerClient>
      remote_client;
  mojo::PendingReceiver<viz::mojom::ExternalBeginFrameControllerClient>
      pending_client_receiver = remote_client.InitWithNewPipeAndPassReceiver();

  auto client_receiver = std::make_unique<
      mojo::Receiver<viz::mojom::ExternalBeginFrameControllerClient>>(this);
  client_receiver->Bind(std::move(pending_client_receiver), task_runner());

  // Move pending remote to params.
  params->external_begin_frame_controller_client = std::move(remote_client);

  host_frame_sink_manager->CreateCompositorDisplayLink(std::move(params));

  // Display AddObserver can only be called on the browser main thread.
  // Only add a observer after IPC is connected.
  display_observer_.emplace(this);

  std::vector<int64_t> display_ids;
  // Screen GetAllDisplays() should be called on the main thread.
  const std::vector<display::Display>& displays =
      display::Screen::Get()->GetAllDisplays();
  for (const display::Display& display : displays) {
    display_ids.push_back(display.id());
  }

  // Since |external_begin_frame_controller_| and |client_receiver_| must be
  // accessed exclusively on the VSync thread sequence, move the remote to the
  // VSync thread and assign it there to maintain sequence safety. Now start
  // getting DisplayLinks for all displays on the VSync thread.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DisplayLinkMacMojo::InitDisplaysOnVSyncThread,
                                base::Unretained(this), std::move(display_ids),
                                std::move(external_begin_frame_controller),
                                std::move(client_receiver)));
}

// Called on the VSync thread directly from IPC.
void DisplayLinkMacMojo::NeedsBeginFrameWithId(int64_t display_id,
                                               bool needs_begin_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);

  auto found = display_links_.find(display_id);
  if (found == display_links_.end()) {
    DLOG(WARNING) << "DisplayLinkMacMojo::NeedsBeginFrameWithId() Display id "
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

      // Notify the GPU process of the DisplayLink failure.
      external_begin_frame_controller_->SetSupportedDisplayLinkId(
          display_id, /*supported=*/false);
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

  // |external_begin_frame_controller_| becomes unbound when it was deleted and
  // is not yet created in OnGpuProcessLost().
  if (!external_begin_frame_controller_.is_bound()) {
    return;
  }

  // With the delay from OnDisplaysRemoved() to DisplaysRemovedOnVSyncThread()
  // or just the delay in CoreAnimation CADisplayLink, we might receive a VSync
  // callback after the display is removed in a rare case, But this is fine. We
  // can still forward this extra VSync to Viz/GPU because `VSyncCallbackMac`
  // uses weak_ptr. If the frame_sink or BeginFrameSource for this display is
  // destroyed, it's handled.
  viz::CADisplayLinkParams viz_params(display_id, params.callback_timebase,
                                      params.display_timebase,
                                      params.callback_interval);
  viz_params.ipc_begin_timestamp = base::TimeTicks::Now();
  external_begin_frame_controller_->IssueExternalVSync(viz_params);
}

void DisplayLinkMacMojo::OnDisplayAdded(const display::Display& new_display) {
  int64_t display_id = new_display.id();
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DisplayLinkMacMojo::DisplayAddedOnVSyncThread,
                                base::Unretained(this), display_id));
}

void DisplayLinkMacMojo::DisplayAddedOnVSyncThread(int64_t vsync_display_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(vsync_thread_sequence_checker_);
  DCHECK(external_begin_frame_controller_);

  if (auto it = display_links_.find(vsync_display_id);
      it != display_links_.end()) {
    // This display has been added.
    return;
  }

  if (!base::IsValueInRangeForNumericType<CGDirectDisplayID>(
          vsync_display_id)) {
    return;
  }

  CGDirectDisplayID display_id =
      static_cast<CGDirectDisplayID>(vsync_display_id);
  scoped_refptr<DisplayLinkMac> display_link_mac =
      CADisplayLinkMac::GetForDisplay(display_id, /*in_gpu_process=*/false);
  if (!display_link_mac) {
    // The display ID was never added to the GPU, so there is no need to call
    // SetSupportedDisplayLinkId(false) to remove an existing supported ID from
    // the GPU.
    return;
  }

  auto result = display_links_.insert(
      std::make_pair(display_id, std::move(display_link_mac)));
  bool inserted = result.second;
  DCHECK(inserted);

  vsync_callbacks_.insert(std::make_pair(display_id, nullptr));

  // Notify the GPU process of the new DisplayLink.
  external_begin_frame_controller_->SetSupportedDisplayLinkId(
      display_id, /*supported=*/true);
}

void DisplayLinkMacMojo::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  std::vector<int64_t> display_ids;
  for (const display::Display& removed_display : removed_displays) {
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
  DCHECK(external_begin_frame_controller_);

  for (auto display_id : display_ids) {
    if (auto it = display_links_.find(display_id); it == display_links_.end()) {
      // This display has been removed.
      continue;
    }

    vsync_callbacks_.erase(display_id);
    display_links_.erase(display_id);

    // Notify the GPU process of the DisplayLink failure.
    external_begin_frame_controller_->SetSupportedDisplayLinkId(
        display_id, /*supported=*/false);
  }
}

}  // namespace ui
