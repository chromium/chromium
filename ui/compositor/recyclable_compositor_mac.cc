// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/recyclable_compositor_mac.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/features.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/types/display_constants.h"

namespace ui {

namespace {

// Returns a task runner for creating a ui::Compositor. This allows compositor
// tasks to be funneled through ui::WindowResizeHelper's task runner to allow
// resize operations to coordinate with frames provided by the GPU process.
scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner() {
  // If the WindowResizeHelper's pumpable task runner is set, it means the GPU
  // process is directing messages there, and the compositor can synchronize
  // with it. Otherwise, just use the UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ui::WindowResizeHelperMac::Get()->task_runner();
  return task_runner ? task_runner
                     : base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RecyclableCompositorMac

RecyclableCompositorMac::RecyclableCompositorMac(
    ui::ContextFactory* context_factory)
    : accelerated_widget_mac_(new ui::AcceleratedWidgetMac()),
      compositor_(context_factory->AllocateFrameSinkId(),
                  context_factory,
                  GetCompositorTaskRunner(),
                  ui::IsPixelCanvasRecordingEnabled()) {
  compositor_.SetAcceleratedWidget(
      accelerated_widget_mac_->accelerated_widget());
  Suspend();
  compositor_.AddObserver(this);
}

RecyclableCompositorMac::~RecyclableCompositorMac() {
  compositor_.RemoveObserver(this);
}

void RecyclableCompositorMac::Suspend() {
  // Requests a compositor lock without a timeout.
  compositor_suspended_lock_ =
      compositor_.GetCompositorLock(nullptr, base::TimeDelta());
}

void RecyclableCompositorMac::Unsuspend() {
  compositor_suspended_lock_ = nullptr;
}

void RecyclableCompositorMac::UpdateSurface(
    const gfx::Size& size_pixels,
    float scale_factor,
    const gfx::DisplayColorSpaces& display_color_spaces,
    int64_t display_id) {
  if (size_pixels != size_pixels_ || scale_factor != scale_factor_) {
    size_pixels_ = size_pixels;
    scale_factor_ = scale_factor;
    local_surface_id_allocator_.GenerateId();
    viz::LocalSurfaceId local_surface_id =
        local_surface_id_allocator_.GetCurrentLocalSurfaceId();
    compositor()->SetScaleAndSize(scale_factor_, size_pixels_,
                                  local_surface_id);
  }
  compositor()->SetDisplayColorSpaces(display_color_spaces);
  compositor()->SetVSyncDisplayID(display_id);
}

void RecyclableCompositorMac::InvalidateSurface() {
  size_pixels_ = gfx::Size();
  scale_factor_ = 1.f;
  local_surface_id_allocator_.Invalidate();
  compositor()->SetScaleAndSize(
      scale_factor_, size_pixels_,
      local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  compositor()->SetDisplayColorSpaces(gfx::DisplayColorSpaces());
  compositor()->SetVSyncDisplayID(display::kInvalidDisplayId);
}

void RecyclableCompositorMac::OnCompositingDidCommit(
    ui::Compositor* compositor_that_did_commit) {
  DCHECK_EQ(compositor_that_did_commit, compositor());
  accelerated_widget_mac_->SetSuspended(false);
}

}  // namespace ui
