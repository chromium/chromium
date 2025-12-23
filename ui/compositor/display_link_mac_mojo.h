// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_DISPLAY_LINK_MAC_MOJO_H_
#define UI_COMPOSITOR_DISPLAY_LINK_MAC_MOJO_H_

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "ui/compositor/compositor_export.h"
#include "ui/display/display_observer.h"

namespace viz {
namespace mojom {
class ExternalBeginFrameController;
}  // namespace mojom
class HostFrameSinkManager;
}  // namespace viz

namespace ui {

class DisplayLinkMac;
class VSyncCallbackMac;
struct VSyncParamsMac;

// To use CoreAnimation CADisplaylink, the app creates a display link object
// with a VSync callback and adds it to the Runloop. When the screen's contents
// need to update, the system calls the callback on the thread (RunLoop) the app
// registered for. Somehow there are issues with CADisplayLink when it's running
// in the GPU process, such as no response from CoreAnimation CADisplayLink
// after power resume. The solution here is to create CADisplayLink in the
// browser thread, forward the VSync signals, and receive control requests
// to/from the GPU process via IPC.
//
// To avoid increasing Browser main thread CPU usage, which can be significant
// for 120Hz animations across multiple windows, DisplayLinkMacMojo offloads all
// IPC and Display Link tasks to a dedicated "VSyncThread" within the browser
// process. Because CADisplayLink requires either the application's main thread
// or a thread using MessagePumpType::NS_RUNLOOP, it is incompatible with the
// Chrome IO thread.
//
// The responsibilities of DisplayLinkMacMojo include:
// 1. Creating a "VSyncThread" within the browser process.
// 2. Receiving MacOS CADisplayLink callbacks and using IPC to forward VSync
// parameters to GPU and Viz.
// 3. Handling VSync start/stop requests from GPU and Viz.
// 4. Monitoring display changes.
//
// DisplayLinkMacMojo is owned by VizProcessTransportFactory, and
// is initialized on CrBrowserMain. All VSync and IPC operations occur on
// VSyncThread, with the exception of Ctor, Dtor, observing display/monitor
// changes, and GpuProcessLost. MacOS NSNotificationCenter always runs the
// callbacks on the App's main thread for display changes. So duplicating the
// display::Screen code in DisplayLinkMacMojo for observation will not be more
// efficient.

class COMPOSITOR_EXPORT DisplayLinkMacMojo
    : public viz::mojom::ExternalBeginFrameControllerClient,
      public base::Thread,
      public display::DisplayObserver {
 public:
  explicit DisplayLinkMacMojo(
      viz::HostFrameSinkManager* host_frame_sink_manager);
  ~DisplayLinkMacMojo() override;

  // base::Thread implementation.
  void CleanUp() override;

  // viz::mojom::ExternalBeginFrameControllerClient implementation.
  void SetNeedsBeginFrame(bool needs_begin_frames) override {}
  void NeedsBeginFrameWithId(int64_t display_id,
                             bool needs_begin_frames) override;
  void SetPreferredInterval(base::TimeDelta interval) override;

  // display::DisplayObserver implementation.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

  void OnGpuProcessLost(viz::HostFrameSinkManager* host_frame_sink_manager);

 private:
  // VSyncIpc is connected when DisplayLinkMacMojo is created, and it's
  // reconnected after the GPU process is lost.
  void ConnectVSyncIpc(viz::HostFrameSinkManager* host_frame_sink_manager);

  void InitDisplaysOnVSyncThread();

  void OnDisplayLinkVSyncCallback(int64_t display_id, VSyncParamsMac params);

  void DisplayAddedOnVSyncThread(int64_t display_id);

  void DisplaysRemovedOnVSyncThread(std::vector<int64_t> display_ids);

  std::map<int64_t, scoped_refptr<DisplayLinkMac>> display_links_;
  std::map<int64_t, std::unique_ptr<VSyncCallbackMac>> vsync_callbacks_;

  // We bind the Remote for the VSync thread so we can issue Interface method
  // calls to the connected Receiver in Viz directly from the VSync thread where
  // CADisplayLink callback is received. |external_begin_frame_controller_|
  // should be destroyed on the same VSync thread or a CHECK will be triggered
  // for not CALLED_ON_VALID_SEQUENCE.
  mojo::Remote<viz::mojom::ExternalBeginFrameController>
      external_begin_frame_controller_;

  std::unique_ptr<
      mojo::Receiver<viz::mojom::ExternalBeginFrameControllerClient>>
      client_receiver_;

  SEQUENCE_CHECKER(vsync_thread_sequence_checker_);
  base::WeakPtrFactory<DisplayLinkMacMojo> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_DISPLAY_LINK_MAC_MOJO_H_
