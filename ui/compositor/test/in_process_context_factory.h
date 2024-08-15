// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "cc/test/test_task_graph_runner.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/surfaces/subtree_capture_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom.h"
#include "ui/compositor/compositor.h"

namespace cc {
class RasterContextProviderWrapper;
}

namespace viz {
class HostFrameSinkManager;
class TestInProcessContextProvider;
}

namespace ui {

class InProcessContextFactory : public ContextFactory {
 public:
  // Both |host_frame_sink_manager| and |frame_sink_manager| must outlive the
  // ContextFactory.
  // TODO(crbug.com/40489946): |frame_sink_manager| should go away and we should
  // use the LayerTreeFrameSink from the HostFrameSinkManager.
  // The default for |output_to_window| will create an OutputSurface that does
  // not display anything. Set to true if you want to see results on the screen.
  InProcessContextFactory(viz::HostFrameSinkManager* host_frame_sink_manager,
                          viz::FrameSinkManagerImpl* frame_sink_manager,
                          bool output_to_window = false);

  InProcessContextFactory(const InProcessContextFactory&) = delete;
  InProcessContextFactory& operator=(const InProcessContextFactory&) = delete;

  ~InProcessContextFactory() override;

  viz::FrameSinkManagerImpl* GetFrameSinkManager() {
    return frame_sink_manager_;
  }

  // Setting a higher refresh rate will spend less time waiting for BeginFrame;
  // while setting a lower refresh rate will reduce the workload per unit of
  // time, which could be useful, e.g., when using mock time and fast forwarding
  // by a long duration.
  //
  // Takes effect for the next CreateLayerTreeFrameSink() call.
  void SetRefreshRateForTests(double refresh_rate);

  // ContextFactory implementation.
  void CreateLayerTreeFrameSink(base::WeakPtr<Compositor> compositor) override;

  scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() override;

  void RemoveCompositor(Compositor* compositor) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  viz::FrameSinkId AllocateFrameSinkId() override;
  viz::SubtreeCaptureId AllocateSubtreeCaptureId() override;
  viz::HostFrameSinkManager* GetHostFrameSinkManager() override;

  SkM44 GetOutputColorMatrix(Compositor* compositor) const;
  gfx::DisplayColorSpaces GetDisplayColorSpaces(Compositor* compositor) const;
  base::TimeTicks GetDisplayVSyncTimeBase(Compositor* compositor) const;
  base::TimeDelta GetDisplayVSyncTimeInterval(Compositor* compositor) const;
  std::optional<base::TimeDelta> GetMaxVSyncInterval(
      Compositor* compositor) const;
  display::VariableRefreshRateState GetVrrState(Compositor* compositor) const;
  void ResetDisplayOutputParameters(Compositor* compositor);

 private:
  class PerCompositorData;

  PerCompositorData* CreatePerCompositorData(Compositor* compositor);

  scoped_refptr<viz::TestInProcessContextProvider> shared_main_thread_contexts_;
  scoped_refptr<cc::RasterContextProviderWrapper>
      shared_worker_context_provider_wrapper_;
  viz::TestSharedBitmapManager shared_bitmap_manager_;
  gpu::SharedImageManager shared_image_manager_;
  gpu::SyncPointManager sync_point_manager_;
  gpu::Scheduler gpu_scheduler_{&sync_point_manager_};
  gpu::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  cc::TestTaskGraphRunner task_graph_runner_;
  viz::FrameSinkIdAllocator frame_sink_id_allocator_;
  viz::SubtreeCaptureIdAllocator subtree_capture_id_allocator_;
  bool output_to_window_ = false;
  bool disable_vsync_ = false;
  double refresh_rate_ = 60.0;
  const raw_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  const raw_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;

  viz::RendererSettings renderer_settings_;
  viz::DebugRendererSettings debug_settings_;
  using PerCompositorDataMap =
      std::unordered_map<Compositor*, std::unique_ptr<PerCompositorData>>;
  PerCompositorDataMap per_compositor_data_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_
