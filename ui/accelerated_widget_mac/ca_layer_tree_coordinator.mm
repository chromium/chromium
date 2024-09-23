// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"

#import <AVFoundation/AVFoundation.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/gl/gl_context.h"
#endif

namespace ui {

CALayerTreeCoordinator::CALayerTreeCoordinator(
    bool allow_av_sample_buffer_display_layer,
    bool new_presentation_feedback_timestamps,
    BufferPresentedCallback buffer_presented_callback)
    : allow_remote_layers_(ui::RemoteLayerAPISupported()),
      allow_av_sample_buffer_display_layer_(
          allow_av_sample_buffer_display_layer),
      new_presentation_feedback_timestamps_(
          new_presentation_feedback_timestamps),
      buffer_presented_callback_(buffer_presented_callback) {
  if (allow_remote_layers_) {
    root_ca_layer_ = [[CALayer alloc] init];
#if BUILDFLAG(IS_MAC)
    // iOS' UIKit has default coordinate system where the origin is at the upper
    // left of the drawing area. In contrast, AppKit and Core Graphics that
    // macOS uses has its origin at the lower left of the drawing area. Thus, we
    // don't need to flip the coordinate system on iOS as it's already set the
    // way we want it to be.
    root_ca_layer_.geometryFlipped = YES;
#endif
    root_ca_layer_.opaque = YES;

    // Create the CAContext to send this to the GPU process, and the layer for
    // the context.
#if BUILDFLAG(IS_MAC)
    CGSConnectionID connection_id = CGSMainConnectionID();
    ca_context_ = [CAContext contextWithCGSConnection:connection_id
                                              options:@{}];
#else
    // Use a very large display ID to ensure that the context is never put
    // on-screen without being explicitly parented.
    ca_context_ = [CAContext remoteContextWithOptions:@{
      kCAContextIgnoresHitTest : @YES,
      kCAContextDisplayId : @10000
    }];
#endif
    ca_context_.layer = root_ca_layer_;
  }
}

CALayerTreeCoordinator::~CALayerTreeCoordinator() = default;

void CALayerTreeCoordinator::Resize(const gfx::Size& pixel_size,
                                    float scale_factor) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
}

void CALayerTreeCoordinator::SetCALayerErrorCode(
    gfx::CALayerResult ca_layer_error_code) {
  ca_layer_error_code_ = ca_layer_error_code;
}

CARendererLayerTree* CALayerTreeCoordinator::GetPendingCARendererLayerTree() {
  if (!unpresented_ca_renderer_layer_tree_) {
    CHECK_LE(presented_frames_.size(), presented_ca_layer_trees_max_length_);

    unpresented_ca_renderer_layer_tree_ = std::make_unique<CARendererLayerTree>(
        allow_av_sample_buffer_display_layer_, false);
  }
  return unpresented_ca_renderer_layer_tree_.get();
}

uint64_t CALayerTreeCoordinator::CreateBackpressureFence() {
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  if (current_context) {
    return current_context->BackpressureFenceCreate();
  }
  return 0;
}

void CALayerTreeCoordinator::ApplyBackpressure() {
  // No frame has been committed yet - this is the first frame being presented.
  if (presented_frames_.empty() || !presented_frames_.front()->has_committed) {
    return;
  }

  TRACE_EVENT0("gpu", "CALayerTreeCoordinator::ApplyBackpressure");

  // Apply back pressure to the previous frame.
  uint64_t frame_fence = presented_frames_.front()->backpressure_fence;

  // Waiting on the previous frame's fence (to maximize CPU and GPU execution
  // overlap).
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  if (current_context) {
    current_context->BackpressureFenceWait(frame_fence);
  }
}

void CALayerTreeCoordinator::Present(
    gl::Presenter::SwapCompletionCallback completion_callback,
    gl::Presenter::PresentationCallback presentation_callback) {
  presented_frames_.push(std::make_unique<PresentedFrame>(
      std::move(completion_callback), std::move(presentation_callback),
      CreateBackpressureFence(), ca_layer_error_code_,
      /*ready_timestamp=*/base::TimeTicks::Now(),
      std::move(unpresented_ca_renderer_layer_tree_)));
}

void CALayerTreeCoordinator::CommitPresentedFrameToCA(
    base::TimeDelta frame_interval,
    base::TimeTicks display_time) {
  // Update the CALayer hierarchy.
  ScopedCAActionDisabler disabler;

  // Remove the committed frame which is displayed on the screen from the
  // |presented_frames_| queue;
  std::unique_ptr<CARendererLayerTree> current_tree;
  if (!presented_frames_.empty() && presented_frames_.front()->has_committed) {
    current_tree.swap(presented_frames_.front()->layer_tree);

    presented_frames_.pop();
  }

  if (presented_frames_.empty()) {
    TRACE_EVENT0("gpu", "Blank frame: No overlays or CALayers");
    DLOG(WARNING) << "Blank frame: No overlays or CALayers";
    root_ca_layer_.sublayers = nil;
    return;
  }

  // Get the frame to be committed.
  auto* frame = presented_frames_.front().get();
  DCHECK(frame);

  if (frame->layer_tree) {
    frame->layer_tree->CommitScheduledCALayers(
        root_ca_layer_, std::move(current_tree), pixel_size_, scale_factor_);
  } else {
    root_ca_layer_.sublayers = nil;
  }
  frame->has_committed = true;

  // Populate the CA layer parameters to send to the browser.
  // Send the swap parameters to the browser.
  if (frame->completion_callback) {
    gfx::CALayerParams params;
    TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers", TRACE_EVENT_SCOPE_THREAD,
                         "GLImpl", static_cast<int>(gl::GetGLImplementation()),
                         "width", pixel_size_.width());
    if (allow_remote_layers_) {
      params.ca_context_id = [ca_context_ contextId];
    } else {
      IOSurfaceRef io_surface = frame->layer_tree->GetContentIOSurface();
      if (io_surface) {
        DCHECK(!allow_remote_layers_);
        params.io_surface_mach_port.reset(IOSurfaceCreateMachPort(io_surface));
      }
    }
    params.pixel_size = pixel_size_;
    params.scale_factor = scale_factor_;
    params.is_empty = false;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(frame->completion_callback),
                       gfx::SwapCompletionResult(
                           gfx::SwapResult::SWAP_ACK,
                           std::make_unique<gfx::CALayerParams>(params))));
  }

  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::Hertz(60),
                                     /*flags=*/0);
  feedback.ca_layer_error_code = frame->ca_layer_error_code;

#if BUILDFLAG(IS_MAC)
  if (new_presentation_feedback_timestamps_) {
    feedback.ready_timestamp = frame->ready_timestamp;
    feedback.latch_timestamp = base::TimeTicks::Now();
    feedback.interval = frame_interval;
    feedback.timestamp = display_time;

    // `update_vsync_params_callback` is not available in
    // SkiaOutputSurfaceImpl::BufferPresented(). Setting kVSync here will not
    // update vsync params.
    feedback.flags = gfx::PresentationFeedback::kHWCompletion |
                     gfx::PresentationFeedback::kVSync;
  }
#endif

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(buffer_presented_callback_,
                     std::move(frame->presentation_callback), feedback));
}

void CALayerTreeCoordinator::SetMaxCALayerTrees(int max_ca_layer_trees) {
  presented_ca_layer_trees_max_length_ = max_ca_layer_trees;
}

int CALayerTreeCoordinator::NumPendingSwaps() {
  int num = presented_frames_.size();
  if (num > 0 && presented_frames_.front()->has_committed) {
    num--;
  }
  return num;
}

PresentedFrame::PresentedFrame(
    gl::Presenter::SwapCompletionCallback completion_cb,
    gl::Presenter::PresentationCallback presentation_cb,
    uint64_t fence,
    gfx::CALayerResult error_code,
    base::TimeTicks ready_timestamp,
    std::unique_ptr<CARendererLayerTree> tree)
    : completion_callback(std::move(completion_cb)),
      presentation_callback(std::move(presentation_cb)),
      backpressure_fence(fence),
      ca_layer_error_code(error_code),
      ready_timestamp(ready_timestamp),
      layer_tree(std::move(tree)),
      has_committed(false) {}

PresentedFrame::~PresentedFrame() = default;

}  // namespace ui
