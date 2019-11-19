// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_

#include <stdint.h>
#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "cc/test/test_task_graph_runner.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "components/viz/test/test_image_factory.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom.h"
#include "ui/compositor/compositor.h"

namespace viz {
class HostFrameSinkManager;
}

namespace ui {
class InProcessContextProvider;

class InProcessContextFactory : public ContextFactory,
                                public ContextFactoryPrivate {
 public:
  // Both |host_frame_sink_manager| and |frame_sink_manager| must outlive the
  // ContextFactory. The constructor without |use_skia_renderer| will use
  // SkiaRenderer if the feature is enabled.
  // TODO(crbug.com/657959): |frame_sink_manager| should go away and we should
  // use the LayerTreeFrameSink from the HostFrameSinkManager.
  InProcessContextFactory(viz::HostFrameSinkManager* host_frame_sink_manager,
                          viz::FrameSinkManagerImpl* frame_sink_manager);
  InProcessContextFactory(viz::HostFrameSinkManager* host_frame_sink_manager,
                          viz::FrameSinkManagerImpl* frame_sink_manager,
                          bool use_skia_renderer);
  ~InProcessContextFactory() override;

  viz::FrameSinkManagerImpl* GetFrameSinkManager() {
    return frame_sink_manager_;
  }

  // If true (the default) an OutputSurface is created that does not display
  // anything. Set to false if you want to see results on the screen.
  void set_use_test_surface(bool use_test_surface) {
    use_test_surface_ = use_test_surface;
  }

  // This is used to call SendOnLostSharedContext() on all clients, to ensure
  // they stop using the SharedMainThreadContextProvider.
  void SendOnLostSharedContext();

  // Set refresh rate will be set to 200 to spend less time waiting for
  // BeginFrame when used for tests.
  void SetUseFastRefreshRateForTests();

  // ContextFactory implementation.
  void CreateLayerTreeFrameSink(base::WeakPtr<Compositor> compositor) override;

  std::unique_ptr<Reflector> CreateReflector(Compositor* mirrored_compositor,
                                             Layer* mirroring_layer) override;
  void RemoveReflector(Reflector* reflector) override;

  scoped_refptr<viz::ContextProvider> SharedMainThreadContextProvider()
      override;
  scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() override;

  void RemoveCompositor(Compositor* compositor) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  viz::FrameSinkId AllocateFrameSinkId() override;
  viz::HostFrameSinkManager* GetHostFrameSinkManager() override;
  void SetDisplayVisible(ui::Compositor* compositor, bool visible) override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;
  void DisableSwapUntilResize(ui::Compositor* compositor) override;
  void SetDisplayColorMatrix(ui::Compositor* compositor,
                             const SkMatrix44& matrix) override;
  void SetDisplayColorSpace(ui::Compositor* compositor,
                            const gfx::ColorSpace& output_color_space,
                            float sdr_white_level) override;
  void SetDisplayVSyncParameters(ui::Compositor* compositor,
                                 base::TimeTicks timebase,
                                 base::TimeDelta interval) override;
  void IssueExternalBeginFrame(
      ui::Compositor* compositor,
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback) override {}
  void SetOutputIsSecure(ui::Compositor* compositor, bool secure) override {}
  void AddVSyncParameterObserver(
      ui::Compositor* compositor,
      mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer)
      override {}
  void SetDisplayTransformHint(Compositor* compositor,
                               gfx::OverlayTransform transform) override {}
  void AddObserver(ContextFactoryObserver* observer) override;
  void RemoveObserver(ContextFactoryObserver* observer) override;
  bool SyncTokensRequiredForDisplayCompositor() override;

  SkMatrix44 GetOutputColorMatrix(Compositor* compositor) const;
  gfx::ColorSpace GetDisplayColorSpace(ui::Compositor* compositor) const;
  float GetSDRWhiteLevel(ui::Compositor* compositor) const;
  base::TimeTicks GetDisplayVSyncTimeBase(ui::Compositor* compositor) const;
  base::TimeDelta GetDisplayVSyncTimeInterval(ui::Compositor* compositor) const;
  void ResetDisplayOutputParameters(ui::Compositor* compositor);

 private:
  struct PerCompositorData;

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor);

  scoped_refptr<InProcessContextProvider> shared_main_thread_contexts_;
  scoped_refptr<InProcessContextProvider> shared_worker_context_provider_;
  viz::TestSharedBitmapManager shared_bitmap_manager_;
  viz::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  viz::TestImageFactory image_factory_;
  cc::TestTaskGraphRunner task_graph_runner_;
  viz::FrameSinkIdAllocator frame_sink_id_allocator_;
  bool use_test_surface_;
  bool disable_vsync_ = false;
  double refresh_rate_ = 60.0;
  viz::HostFrameSinkManager* const host_frame_sink_manager_;
  viz::FrameSinkManagerImpl* const frame_sink_manager_;
  base::ObserverList<ContextFactoryObserver>::Unchecked observer_list_;

  viz::RendererSettings renderer_settings_;
  using PerCompositorDataMap =
      std::unordered_map<ui::Compositor*, std::unique_ptr<PerCompositorData>>;
  PerCompositorDataMap per_compositor_data_;

  DISALLOW_COPY_AND_ASSIGN(InProcessContextFactory);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_
