// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_FRAME_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_FRAME_MANAGER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/common/wayland_presentation_info.h"

namespace ui {

class WaylandBufferHandle;
class WaylandConnection;
class WaylandWindow;
class WaylandSurface;
class WaylandSubsurface;

// Representation of a graphical frame update (DrawingFrame), stores the
// configuration information required to present this frame across Wayland. It
// also has information about when/whether resources of the frame has been
// presented and released.
struct WaylandFrame {
 public:
  // A frame originated from gpu process, and hence, requires acknowledgements.
  WaylandFrame(uint32_t frame_id,
               const gfx::FrameData& data,
               WaylandSurface* root_surface,
               wl::WaylandOverlayConfig root_config,
               base::circular_deque<
                   std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
                   subsurfaces_to_overlays = {});

  // A frame that does not require acknowledgements.
  WaylandFrame(WaylandSurface* root_surface,
               wl::WaylandOverlayConfig root_config,
               base::circular_deque<
                   std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
                   subsurfaces_to_overlays = {});

  WaylandFrame() = delete;
  WaylandFrame(const WaylandFrame&) = delete;
  WaylandFrame& operator=(const WaylandFrame&) = delete;
  ~WaylandFrame();

 private:
  friend class WaylandFrameManager;
  friend class WaylandFrameManagerTest;

  uint32_t frame_id;
  raw_ptr<WaylandSurface, DanglingUntriaged> root_surface;
  wl::WaylandOverlayConfig root_config;
  base::circular_deque<std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
      subsurfaces_to_overlays;

  base::flat_map<WaylandSurface*, raw_ptr<WaylandBufferHandle, CtnExperimental>>
      submitted_buffers;

  // An indicator that there are buffers destrotyed before frame playback. This
  // frame should be skipped.
  bool buffer_lost = false;

  // A Wayland callback, which is triggered once wl_buffer has been committed
  // and it is the right time to notify the GPU that it can start a new drawing
  // operation.
  wl::Object<wl_callback> wl_frame_callback;

  // Merged release fence fd. This is taken as the union of all release fences
  // for this frame.
  base::ScopedFD merged_release_fence_fd;
  // Whether this frame has had OnSubmission sent for it.
  bool submission_acked;

  // The wayland object identifying this feedback.
  wl::Object<struct wp_presentation_feedback> pending_feedback;
  // The actual presentation feedback. May be missing if the callback from the
  // Wayland server has not arrived yet.
  std::optional<gfx::PresentationFeedback> feedback = std::nullopt;
  // Whether this frame has had OnPresentation sent for it.
  bool presentation_acked;

  // The sequence ID for this frame. This is used to know when the proper
  // buffers associated with a configure arrive.
  [[maybe_unused]] int64_t seq = -1;

  // Trace ID for tracking submission of the current frame.
  int64_t trace_id = -1;
};

// This is the frame update manager that configures graphical window/surface
// state and submits buffer swaps to a window. Meanwhile it keeps track of the
// pending/submitted swaps in order to send the acknowledgements back to gpu
// process.
class WaylandFrameManager {
 public:
  WaylandFrameManager(WaylandWindow* window, WaylandConnection* connection);
  WaylandFrameManager(const WaylandFrameManager&) = delete;
  WaylandFrameManager& operator=(const WaylandFrameManager&) = delete;
  ~WaylandFrameManager();

  // WaylandWindow::CommitOverlays() calls this to put a pending frame in,
  // without making any actual Wayland protocol requests to the compositor that
  // will change the visual properties of the window.
  // A pending frame will be played back in PlayBackFrame() if the requirements
  // for submitting it are met.
  void RecordFrame(std::unique_ptr<WaylandFrame> frame);

  // Verifies if the first pending_frame can be played back. Requirements are:
  //   1) Previous frame's wl_frame_callback must be ack'ed;
  //   2) Window has been configured;
  //   3) All required wl_buffers are ready.
  // Does the playback if all requirements are met.
  void MaybeProcessPendingFrame();

  // Clears the state of the |frame_manager_| when the GPU channel is destroyed.
  void ClearStates();

  // Similar to ClearStates(), but does not clear submitted frames.
  void Hide();

  void SetVideoCapture();
  void ReleaseVideoCapture();

  void OnWindowActivationChanged();

  static base::TimeDelta GetPresentationFlushTimerDurationForTesting();

 private:
  friend class WaylandFrameManagerTest;

  void PlayBackFrame(std::unique_ptr<WaylandFrame> frame);
  void DiscardFrame(std::unique_ptr<WaylandFrame> frame);

  void EvaluateShouldSkipFrameCallbacks();

  // Configures |surface| but does not commit wl_surface states yet.
  // Returns whether or not changes require a commit to the wl_surface.
  bool ApplySurfaceConfigure(WaylandFrame* frame,
                             WaylandSurface* surface,
                             wl::WaylandOverlayConfig& config,
                             bool needs_opaque_region);

  void MaybeProcessSubmittedFrames();
  void ProcessOldSubmittedFrame(WaylandFrame* frame);

  // Gets presentation feedback information ready to be sent for submitted
  // frames. Also updates `presentation_acked` of corresponding frames to true.
  std::vector<wl::WaylandPresentationInfo> GetReadyPresentations();
  bool HaveReadyPresentations() const;

  // Clears submitted frames that are fully released and have already sent
  // presentation feedback info.
  void ClearProcessedSubmittedFrames();

  void OnExplicitBufferRelease(WaylandSurface* surface,
                               wl_buffer* wl_buffer,
                               base::ScopedFD fence);
  void OnWlBufferRelease(WaylandSurface* surface, wl_buffer* wl_buffer);

  // wl_callback_listener callbacks:
  static void OnFrameDone(void* data, wl_callback* callback, uint32_t time);

  void HandleFrameCallback(wl_callback* callback);

  // wp_presentation_feedback_listener callbacks:
  static void OnSyncOutput(
      void* data,
      struct wp_presentation_feedback* presentation_feedback,
      wl_output* output);
  static void OnPresented(
      void* data,
      struct wp_presentation_feedback* presentation_feedback,
      uint32_t tv_sec_hi,
      uint32_t tv_sec_lo,
      uint32_t tv_nsec,
      uint32_t refresh,
      uint32_t seq_hi,
      uint32_t seq_lo,
      uint32_t flags);
  static void OnDiscarded(
      void* data,
      struct wp_presentation_feedback* presentation_feedback);

  void HandlePresentationFeedback(
      struct wp_presentation_feedback* presentation_feedback,
      const gfx::PresentationFeedback& feedback,
      bool discarded = false);

  // Verifies the number of submitted frames and discards pending presentation
  // feedbacks if the number is too big.
  void VerifyNumberOfSubmittedFrames();

  // Verifies wl_buffers for the given |frame| exist. If they do not yet exist,
  // a callback to |MaybeProcessPendingFrame| is set and false is returned.
  // If the frame contains a buffer id for an invalid WaylandBufferHandle, the
  // |frame::buffer_lost| is set and false is returned. That means that the
  // frame must not be used for the further submission.
  bool EnsureWlBuffersExist(WaylandFrame& frame);

  // Immediately clears submitted_buffers in the 1st in-flight submitted_frame.
  // This unblocks the pipeline.
  // TODO(crbug.com/40237160): Remove related workaround once CrOS side fix
  // stablizes.
  void FreezeTimeout();

  void UpdatePresentationFlushTimer();
  void OnPresentationFlushTimerFired();

  const raw_ptr<WaylandWindow> window_;

  // When RecordFrame() is called, a Frame is pushed to |pending_frames_|. See
  // RecordFrame().
  base::circular_deque<std::unique_ptr<WaylandFrame>> pending_frames_;

  // After PlayBackFrame() is called, a Frame is pushed to |submitted_frames_|.
  // See MaybeProcessPendingFrame().
  base::circular_deque<std::unique_ptr<WaylandFrame>> submitted_frames_;

  // Non-owned pointer to the main connection.
  const raw_ptr<WaylandConnection> connection_;

  // Set when invalid frame data is sent and the gpu process must be terminated.
  std::string fatal_error_message_;

  uint32_t frames_in_flight_ = 0;
  base::OneShotTimer freeze_timeout_timer_;

  base::OneShotTimer presentation_flush_timer_;

  int video_capture_count_ = 0;

  // Indicates if fallback rendering should be used by not relying on frame
  // callbacks.
  bool should_skip_frame_callbacks_ = false;

  base::WeakPtrFactory<WaylandFrameManager> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_FRAME_MANAGER_H_
