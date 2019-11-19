// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_context_factory.h"

#include "cc/test/test_layer_tree_frame_sink.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/test_context_provider.h"

namespace aura {
namespace test {
namespace {

class FrameSinkClient : public cc::TestLayerTreeFrameSinkClient {
 public:
  explicit FrameSinkClient(
      scoped_refptr<viz::ContextProvider> display_context_provider)
      : display_context_provider_(std::move(display_context_provider)) {}

  // cc::TestLayerTreeFrameSinkClient:
  std::unique_ptr<viz::SkiaOutputSurface> CreateDisplaySkiaOutputSurface()
      override {
    return viz::FakeSkiaOutputSurface::Create3d();
  }

  std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<viz::ContextProvider> compositor_context_provider)
      override {
    return viz::FakeOutputSurface::Create3d(
        std::move(display_context_provider_));
  }
  void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) override {}
  void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) override {}
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              viz::RenderPassList* render_passes) override {}
  void DisplayDidDrawAndSwap() override {}

 private:
  scoped_refptr<viz::ContextProvider> display_context_provider_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkClient);
};

}  // namespace

AuraTestContextFactory::AuraTestContextFactory() = default;

AuraTestContextFactory::~AuraTestContextFactory() = default;

void AuraTestContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::Create();
  std::unique_ptr<FrameSinkClient> frame_sink_client =
      std::make_unique<FrameSinkClient>(context_provider);
  constexpr bool synchronous_composite = false;
  constexpr bool disable_display_vsync = false;
  const double refresh_rate = 200.0;
  auto frame_sink = std::make_unique<cc::TestLayerTreeFrameSink>(
      context_provider, viz::TestContextProvider::CreateWorker(),
      GetGpuMemoryBufferManager(), renderer_settings(),
      base::ThreadTaskRunnerHandle::Get().get(), synchronous_composite,
      disable_display_vsync, refresh_rate);
  frame_sink->SetClient(frame_sink_client.get());
  compositor->SetLayerTreeFrameSink(std::move(frame_sink));
  frame_sink_clients_.insert(std::move(frame_sink_client));
}

}  // namespace test
}  // namespace aura
