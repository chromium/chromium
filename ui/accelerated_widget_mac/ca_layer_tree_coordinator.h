// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_COORDINATOR_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_COORDINATOR_H_

#include <queue>

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/presenter.h"

@class CAContext;
@class CALayer;

namespace ui {
using BufferPresentedCallback =
    base::RepeatingCallback<void(gl::Presenter::PresentationCallback callback,
                                 const gfx::PresentationFeedback& feedback)>;

struct PresentedFrame {
  PresentedFrame(gl::Presenter::SwapCompletionCallback completion_cb,
                 gl::Presenter::PresentationCallback presentation_cb,
                 uint64_t fence,
                 gfx::CALayerResult error_code,
                 base::TimeTicks ready_timestamp,
                 std::unique_ptr<CARendererLayerTree> layer_tree);
  ~PresentedFrame();

  // The swap completion and presentation callbacks are made when
  // the layers in `tree` are committed to CoreAnimation.
  gl::Presenter::SwapCompletionCallback completion_callback;
  gl::Presenter::PresentationCallback presentation_callback;

  // A fence inserted when Present was called. Future frames will
  // wait on this fence to ensure that our GPU work does not starve
  // CoreAnimation.
  uint64_t backpressure_fence = 0;

  gfx::CALayerResult ca_layer_error_code = gfx::kCALayerSuccess;

  // The timetick when the GPU has finished completing all the drawing commands
  base::TimeTicks ready_timestamp;

  // The tree structure and, for the currently displaying tree, the
  // actually CALayers.
  std::unique_ptr<CARendererLayerTree> layer_tree;

  bool has_committed = false;
};

// A structure that holds the tree of CALayers to display composited content.
// The CALayer tree may consist of a GLRendererLayerTree if the OpenGL renderer
// is being used, or a CARendererLayerTree if the CoreAnimation renderer is
// being used.
//
// This is instantiated in the GPU process and sent to the browser process via
// the cross-process CoreAnimation API.
class ACCELERATED_WIDGET_MAC_EXPORT CALayerTreeCoordinator {
 public:
  CALayerTreeCoordinator(bool allow_av_sample_buffer_display_layer,
                         bool new_presentation_feedback_timestamps,
                         BufferPresentedCallback buffer_preseneted_callback);

  CALayerTreeCoordinator(const CALayerTreeCoordinator&) = delete;
  CALayerTreeCoordinator& operator=(const CALayerTreeCoordinator&) = delete;

  ~CALayerTreeCoordinator();

  // Set the composited frame's size.
  void Resize(const gfx::Size& pixel_size, float scale_factor);

  // Set the CALayer overlay error for the frame that is going to be presented.
  void SetCALayerErrorCode(gfx::CALayerResult ca_layer_error_code);

  // The CARendererLayerTree for the pending frame. This is used to construct
  // the CALayer tree for the CoreAnimation renderer.
  CARendererLayerTree* GetPendingCARendererLayerTree();

  void Present(gl::Presenter::SwapCompletionCallback completion_callback,
               gl::Presenter::PresentationCallback presentation_callback);

  //  Do a GL fence for flush to apply back-pressure on the committed frame.
  void ApplyBackpressure();

  // Commit the presented frame's OpenGL backbuffer or CALayer tree to be
  // attached to the root CALayer.
  void CommitPresentedFrameToCA(base::TimeDelta frame_interval,
                                base::TimeTicks display_time);

  void SetMaxCALayerTrees(int cap_max_ca_layer_trees);

  int NumPendingSwaps();

 private:
  uint64_t CreateBackpressureFence();

  const bool allow_remote_layers_ = true;
  const bool allow_av_sample_buffer_display_layer_ = true;
  const bool new_presentation_feedback_timestamps_;
  gfx::Size pixel_size_;
  float scale_factor_ = 1;
  gfx::CALayerResult ca_layer_error_code_ = gfx::kCALayerSuccess;

  // The max number of CARendererLayerTree allowed at the same time. It includes
  // both the current tree and the pending trees.
  size_t presented_ca_layer_trees_max_length_ = 2;

  CAContext* __strong ca_context_;

  // The root CALayer to display the current frame. This does not change
  // over the lifetime of the object.
  CALayer* __strong root_ca_layer_;

  BufferPresentedCallback buffer_presented_callback_;

  // The frame that is currently under construction. It has had planes
  // scheduled, but has not had Present() called yet. When Present() is called,
  // it will be moved to |presented_frames_|.
  std::unique_ptr<CARendererLayerTree> unpresented_ca_renderer_layer_tree_;

  // The frames has been presented.
  std::queue<std::unique_ptr<PresentedFrame>> presented_frames_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_COORDINATOR_H_
