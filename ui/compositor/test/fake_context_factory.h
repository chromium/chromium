// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "cc/test/test_task_graph_runner.h"
#include "components/viz/common/display/renderer_settings.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "ui/compositor/compositor.h"

namespace cc {
class FakeLayerTreeFrameSink;
class TestTaskGraphRunner;
}

namespace viz {
class CompositorFrame;
class TestGpuMemoryBufferManager;
}

namespace ui {

class FakeContextFactory : public ui::ContextFactory {
 public:
  FakeContextFactory();

  FakeContextFactory(const FakeContextFactory&) = delete;
  FakeContextFactory& operator=(const FakeContextFactory&) = delete;

  ~FakeContextFactory() override;

  const viz::CompositorFrame& GetLastCompositorFrame() const;

  // ui::ContextFactory:
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
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
  raw_ptr<cc::FakeLayerTreeFrameSink> frame_sink_ = nullptr;
  cc::TestTaskGraphRunner task_graph_runner_;
  gpu::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  viz::RendererSettings renderer_settings_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_
