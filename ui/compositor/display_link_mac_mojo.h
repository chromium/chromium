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
class HostFrameSinkManager;
}  // namespace viz

namespace ui {

class DisplayLinkMac;
class VSyncCallbackMac;
struct VSyncParamsMac;

// DisplayLinkMacMojo which creates VSyncThread and runs on it is responsible
// for:
// (1) Receiving MacOS CADisplayLink callbacks and forward the VSync
// parameters to Viz and GPU via IPC. (2) Responding to the start/stop VSync
// requests from Viz and GPU via IPC. (3) Observing the display change.
//
// After initialization, this VSync thread does not interact with the Browser
// main thread except for observing display/monitor changes. MacOS
// NSNotificationCenter always calls the callbacks on the App's main thread for
// display changes. So duplicating the display::Screen code to
// DisplayLinkMacMojo for observation is not more efficient.

class COMPOSITOR_EXPORT DisplayLinkMacMojo
    : public viz::mojom::ExternalBeginFrameControllerClient,
      public base::Thread,
      public display::DisplayObserver {
 public:
  explicit DisplayLinkMacMojo(
      viz::HostFrameSinkManager* host_frame_sink_manager);
  ~DisplayLinkMacMojo() override;

  static bool SupportsDisplayLinkMacInBrowser();

  // base::Thread implementation.
  void Init() override;
  void CleanUp() override;

  // viz::mojom::ExternalBeginFrameControllerClient implementation.
  void SetNeedsBeginFrame(bool needs_begin_frames) override {}
  void NeedsBeginFrameWithId(int64_t display_id, bool needs_begin_frames);
  void SetPreferredInterval(base::TimeDelta interval) override;

  // display::DisplayObserver implementation.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

 private:
  void ConnectVSyncIpc(viz::HostFrameSinkManager* host_frame_sink_manager);

  void OnDisplayLinkVSyncCallback(int64_t display_id, VSyncParamsMac params);

  void DisplayAddedOnVSyncThread(int64_t display_id);

  void DisplaysRemovedOnVSyncThread(std::vector<int64_t> display_ids);

  std::map<int64_t, scoped_refptr<DisplayLinkMac>> display_links_;
  std::map<int64_t, std::unique_ptr<VSyncCallbackMac>> vsync_callbacks_;

  SEQUENCE_CHECKER(vsync_thread_sequence_checker_);
  base::WeakPtrFactory<DisplayLinkMacMojo> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_DISPLAY_LINK_MAC_MOJO_H_
