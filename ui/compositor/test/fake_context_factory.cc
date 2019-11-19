// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/fake_context_factory.h"

#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/switches.h"

#if defined(OS_MACOSX)
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#endif

namespace ui {

FakeContextFactory::FakeContextFactory() {
#if defined(OS_MACOSX)
  renderer_settings_.release_overlay_resources_after_gpu_query = true;
  // Ensure that tests don't wait for frames that will never come.
  ui::CATransactionCoordinator::Get().DisableForTesting();
#endif
}

FakeContextFactory::~FakeContextFactory() = default;

const viz::CompositorFrame& FakeContextFactory::GetLastCompositorFrame() const {
  return *frame_sink_->last_sent_frame();
}

void FakeContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  auto frame_sink = cc::FakeLayerTreeFrameSink::Create3d();
  frame_sink_ = frame_sink.get();
  compositor->SetLayerTreeFrameSink(std::move(frame_sink));
}

scoped_refptr<viz::ContextProvider>
FakeContextFactory::SharedMainThreadContextProvider() {
  return nullptr;
}

scoped_refptr<viz::RasterContextProvider>
FakeContextFactory::SharedMainThreadRasterContextProvider() {
  return nullptr;
}

void FakeContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  frame_sink_ = nullptr;
}

gpu::GpuMemoryBufferManager* FakeContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* FakeContextFactory::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

bool FakeContextFactory::SyncTokensRequiredForDisplayCompositor() {
  return true;
}

}  // namespace ui
