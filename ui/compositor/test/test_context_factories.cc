// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_context_factories.h"

#include "base/command_line.h"
#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_implementation.h"

namespace ui {

TestContextFactories::TestContextFactories(bool enable_pixel_output,
                                           bool output_to_window) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnablePixelOutputInTests))
    enable_pixel_output = true;
  if (enable_pixel_output)
    disable_null_draw_ = std::make_unique<gl::DisableNullDrawGLBindings>();
  shared_bitmap_manager_ = std::make_unique<viz::ServerSharedBitmapManager>();
  frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::FrameSinkManagerImpl::InitParams(shared_bitmap_manager_.get()));
  host_frame_sink_manager_ = std::make_unique<viz::HostFrameSinkManager>();
  implicit_factory_ = std::make_unique<InProcessContextFactory>(
      host_frame_sink_manager_.get(), frame_sink_manager_.get(),
      output_to_window);
  // Set to a high refresh rate to spend less time waiting for BeginFrame.
  implicit_factory_->SetRefreshRateForTests(200.0);

  // Directly connect without using Mojo.
  frame_sink_manager_->SetLocalClient(host_frame_sink_manager_.get());
  host_frame_sink_manager_->SetLocalManager(frame_sink_manager_.get());
}

TestContextFactories::~TestContextFactories() = default;

ui::InProcessContextFactory* TestContextFactories::GetContextFactory() const {
  return implicit_factory_.get();
}

}  // namespace ui
