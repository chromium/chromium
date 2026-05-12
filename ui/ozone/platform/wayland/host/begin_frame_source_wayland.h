// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_BEGIN_FRAME_SOURCE_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_BEGIN_FRAME_SOURCE_WAYLAND_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/platform_window/extensions/begin_frame_source_extension.h"

namespace ui {

class PlatformWindow;
class WaylandFrameManager;

class WaylandFrameTimingObserver : public base::CheckedObserver {
 public:
  virtual void OnFrameCallback(base::TimeTicks frame_time) = 0;
  virtual void OnPresentationFeedback(
      const gfx::PresentationFeedback& feedback) = 0;
};

// Drives begin frames from Wayland frame callbacks (wl_frame_callback) and
// uses presentation feedback (wp_presentation_feedback) for accurate timing.
class BeginFrameSourceWayland : public BeginFrameSourceExtension,
                                public WaylandFrameTimingObserver {
 public:
  BeginFrameSourceWayland(PlatformWindow* window,
                          WaylandFrameManager* frame_manager);
  ~BeginFrameSourceWayland() override;

  BeginFrameSourceWayland(const BeginFrameSourceWayland&) = delete;
  BeginFrameSourceWayland& operator=(const BeginFrameSourceWayland&) = delete;

  // BeginFrameSourceExtension implementation.
  void SetDelegate(Delegate* delegate) override;
  void SetNeedsBeginFrame(bool needs) override;
  void SetPreferredInterval(base::TimeDelta interval) override;

  // WaylandFrameTimingObserver implementation.
  void OnFrameCallback(base::TimeTicks callback_time) override;
  void OnPresentationFeedback(
      const gfx::PresentationFeedback& feedback) override;

 private:
  void MaybeIssueBeginFrame();
  void OnBeginFrameAck(bool has_damage);
  void StartFrameCallbackTimer();
  void OnFrameCallbackTimeout();

  const raw_ptr<WaylandFrameManager> frame_manager_;
  const raw_ptr<PlatformWindow> window_;

  raw_ptr<Delegate> delegate_ = nullptr;

  // Default vsync interval, matches viz::BeginFrameArgs::DefaultInterval().
  static constexpr base::TimeDelta kDefaultInterval = base::Microseconds(16666);

  // Current vsync interval, updated from presentation feedback.
  base::TimeDelta vsync_interval_ = kDefaultInterval;

  // Last known frame presentation time if provided by the compositor,
  // used to align frame_time and deadline to the display's timing.
  base::TimeTicks last_presentation_time_;

  // Whether the delegate wants us to produce begin frames.
  bool needs_begin_frame_ = false;

  // Only one frame can be in flight at a time.
  bool frame_in_flight_ = false;

  // True if Wayland is ready for a frame (we got a callback) or we
  // want to issue one manually.
  bool ready_to_issue_begin_frame_ = false;

  base::OneShotTimer frame_callback_timeout_timer_;

  base::WeakPtrFactory<BeginFrameSourceWayland> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_BEGIN_FRAME_SOURCE_WAYLAND_H_
