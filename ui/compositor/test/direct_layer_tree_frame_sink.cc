// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/direct_layer_tree_frame_sink.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/tiles/image_decode_cache_utils.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/features.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "gpu/ipc/client/client_shared_image_interface.h"

namespace ui {

DirectLayerTreeFrameSink::DirectLayerTreeFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    viz::Display* display,
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<cc::RasterContextProviderWrapper>
        worker_context_provider_wrapper,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : LayerTreeFrameSink(std::move(context_provider),
                         std::move(worker_context_provider_wrapper),
                         std::move(compositor_task_runner),
                         gpu_memory_buffer_manager,
                         /*shared_image_interface=*/nullptr),
      frame_sink_id_(frame_sink_id),
      frame_sink_manager_(frame_sink_manager),
      display_(display) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

DirectLayerTreeFrameSink::~DirectLayerTreeFrameSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Reset `client_` in Display to avoid accessing `client_` after this is
  // destructed. This is to resolve the circular dependency between Display and
  // DirectLayerTreeFrameSink.
  display_->ResetDisplayClientForTesting(/*old_client=*/this);
}

bool DirectLayerTreeFrameSink::BindToClient(
    cc::LayerTreeFrameSinkClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!cc::LayerTreeFrameSink::BindToClient(client))
    return false;

  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, frame_sink_manager_, frame_sink_id_, /*is_root=*/true);
  begin_frame_source_ = std::make_unique<viz::ExternalBeginFrameSource>(this);
  client_->SetBeginFrameSource(begin_frame_source_.get());

  // Avoid initializing GL context here, as this should be sharing the
  // Display's context.
  display_->Initialize(this, frame_sink_manager_->surface_manager());

  support_->SetUpHitTest(display_);

  return true;
}

void DirectLayerTreeFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();

  // Unregister the SurfaceFactoryClient here instead of the dtor so that only
  // one client is alive for this namespace at any given time.
  support_.reset();

  cc::LayerTreeFrameSink::DetachFromClient();
}

void DirectLayerTreeFrameSink::SubmitCompositorFrame(
    viz::CompositorFrame frame,
    bool hit_test_data_changed) {
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK(frame.metadata.begin_frame_ack.frame_id.IsSequenceValid());

  if (frame.size_in_pixels() != last_swap_frame_size_ ||
      frame.device_scale_factor() != device_scale_factor_ ||
      !parent_local_surface_id_allocator_.HasValidLocalSurfaceId()) {
    parent_local_surface_id_allocator_.GenerateId();
    last_swap_frame_size_ = frame.size_in_pixels();
    device_scale_factor_ = frame.device_scale_factor();
    display_->SetLocalSurfaceId(
        parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        device_scale_factor_);
  }

  std::optional<viz::HitTestRegionList> hit_test_region_list =
      client_->BuildHitTestData();

  if (!hit_test_region_list) {
    last_hit_test_data_ = viz::HitTestRegionList();
  } else if (!hit_test_data_changed) {
    // Do not send duplicate hit-test data.
    if (viz::HitTestRegionList::IsEqual(*hit_test_region_list,
                                        last_hit_test_data_)) {
      DCHECK(!viz::HitTestRegionList::IsEqual(*hit_test_region_list,
                                              viz::HitTestRegionList()));
      hit_test_region_list = std::nullopt;
    } else {
      last_hit_test_data_ = *hit_test_region_list;
    }
  } else {
    last_hit_test_data_ = *hit_test_region_list;
  }

  support_->SubmitCompositorFrame(
      parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
      std::move(frame), std::move(hit_test_region_list));
}

void DirectLayerTreeFrameSink::DidNotProduceFrame(
    const viz::BeginFrameAck& ack,
    cc::FrameSkippedReason reason) {
  DCHECK(!ack.has_damage);
  DCHECK(ack.frame_id.IsSequenceValid());
  support_->DidNotProduceFrame(ack);
}

void DirectLayerTreeFrameSink::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const viz::SharedBitmapId& id) {
  bool ok = support_->DidAllocateSharedBitmap(std::move(region), id);
  DCHECK(ok);
}

void DirectLayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id) {
  support_->DidDeleteSharedBitmap(id);
}

void DirectLayerTreeFrameSink::DisplayOutputSurfaceLost() {
  is_lost_ = true;
  client_->DidLoseLayerTreeFrameSink();
}

void DirectLayerTreeFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    viz::AggregatedRenderPassList* render_passes) {
  if (support_->GetHitTestAggregator()) {
    support_->GetHitTestAggregator()->Aggregate(display_->CurrentSurfaceId());
  }
}

base::TimeDelta
DirectLayerTreeFrameSink::GetPreferredFrameIntervalForFrameSinkId(
    const viz::FrameSinkId& id,
    viz::mojom::CompositorFrameSinkType* type) {
  return frame_sink_manager_->GetPreferredFrameIntervalForFrameSinkId(id, type);
}

void DirectLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  // Submitting a CompositorFrame can synchronously draw and dispatch a frame
  // ack. PostTask to ensure the client is notified on a new stack frame.
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DirectLayerTreeFrameSink::DidReceiveCompositorFrameAckInternal,
          weak_factory_.GetWeakPtr(), std::move(resources)));
}

void DirectLayerTreeFrameSink::DidReceiveCompositorFrameAckInternal(
    std::vector<viz::ReturnedResource> resources) {
  client_->ReclaimResources(std::move(resources));
  client_->DidReceiveCompositorFrameAck();
}

void DirectLayerTreeFrameSink::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details,
    bool frame_ack,
    std::vector<viz::ReturnedResource> resources) {
  if (features::IsOnBeginFrameAcksEnabled()) {
    if (frame_ack) {
      DidReceiveCompositorFrameAck(std::move(resources));
    } else if (!resources.empty()) {
      ReclaimResources(std::move(resources));
    }
  }
  for (const auto& pair : timing_details)
    client_->DidPresentCompositorFrame(pair.first, pair.second);

  if (!needs_begin_frames_) {
    // OnBeginFrame() can be called just to deliver presentation feedback, so
    // report that we didn't use this BeginFrame.
    DidNotProduceFrame(viz::BeginFrameAck(args, false),
                       cc::FrameSkippedReason::kNoDamage);
    return;
  }

  begin_frame_source_->OnBeginFrame(args);
}

void DirectLayerTreeFrameSink::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  client_->ReclaimResources(std::move(resources));
}

void DirectLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {
  begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void DirectLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  needs_begin_frames_ = needs_begin_frames;
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

}  // namespace ui
