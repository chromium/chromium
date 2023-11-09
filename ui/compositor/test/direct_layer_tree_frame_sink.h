// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_DIRECT_LAYER_TREE_FRAME_SINK_H_
#define UI_COMPOSITOR_TEST_DIRECT_LAYER_TREE_FRAME_SINK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace viz {
class Display;
class FrameSinkManagerImpl;
}  // namespace viz

namespace ui {

// This class submits compositor frames to an in-process Display, with the
// client's frame being the root surface of the Display.
class DirectLayerTreeFrameSink : public cc::LayerTreeFrameSink,
                                 public viz::mojom::CompositorFrameSinkClient,
                                 public viz::ExternalBeginFrameSourceClient,
                                 public viz::DisplayClient {
 public:
  // |frame_sink_manager| and |display| must outlive this class.
  DirectLayerTreeFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      viz::FrameSinkManagerImpl* frame_sink_manager,
      viz::Display* display,
      scoped_refptr<viz::RasterContextProvider> context_provider,
      scoped_refptr<cc::RasterContextProviderWrapper>
          worker_context_provider_wrapper,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);

  DirectLayerTreeFrameSink(const DirectLayerTreeFrameSink& other) = delete;
  DirectLayerTreeFrameSink& operator=(const DirectLayerTreeFrameSink& other) =
      delete;

  ~DirectLayerTreeFrameSink() override;

  // cc::LayerTreeFrameSink implementation.
  bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                          cc::FrameSkippedReason reason) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

  // viz::DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      viz::AggregatedRenderPassList* render_passes) override;
  void DisplayDidDrawAndSwap() override {}
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override {}
  void DisplayAddChildWindowToBrowser(
      gpu::SurfaceHandle child_window) override {}
  void SetWideColorEnabled(bool enabled) override {}
  void SetPreferredFrameInterval(base::TimeDelta interval) override {}
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const viz::FrameSinkId& id,
      viz::mojom::CompositorFrameSinkType* type) override;

 private:
  // viz::mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resource) override;
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void OnBeginFramePausedChanged(bool paused) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override {}

  // viz::ExternalBeginFrameSourceClient implementation:
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void DidReceiveCompositorFrameAckInternal(
      std::vector<viz::ReturnedResource> resources);

  // This class is only meant to be used on a single thread.
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;

  bool needs_begin_frames_ = false;
  const viz::FrameSinkId frame_sink_id_;
  raw_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  raw_ptr<viz::Display> display_;
  gfx::Size last_swap_frame_size_;
  float device_scale_factor_ = 1.f;
  bool is_lost_ = false;
  std::unique_ptr<viz::ExternalBeginFrameSource> begin_frame_source_;

  viz::HitTestRegionList last_hit_test_data_;

  base::WeakPtrFactory<DirectLayerTreeFrameSink> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_DIRECT_LAYER_TREE_FRAME_SINK_H_
