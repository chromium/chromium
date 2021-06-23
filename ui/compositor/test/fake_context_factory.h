// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_

#include "cc/test/test_task_graph_runner.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "ui/compositor/compositor.h"

namespace cc {
class FakeLayerTreeFrameSink;
class TestTaskGraphRunner;
class TestGpuMemoryBufferManager;
}

namespace viz {
class CompositorFrame;
class ContextProvider;
}

namespace ui {

class FakeContextFactory : public ui::ContextFactory {
 public:
  FakeContextFactory();
  ~FakeContextFactory() override;

  const viz::CompositorFrame& GetLastCompositorFrame() const;

  // ui::ContextFactory:
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<viz::ContextProvider> SharedMainThreadContextProvider()
      override;
  scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;

 protected:
  const viz::RendererSettings& renderer_settings() const {
    return renderer_settings_;
  }

 private:
  cc::FakeLayerTreeFrameSink* frame_sink_ = nullptr;
  cc::TestTaskGraphRunner task_graph_runner_;
  viz::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  viz::RendererSettings renderer_settings_;

  DISALLOW_COPY_AND_ASSIGN(FakeContextFactory);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_
