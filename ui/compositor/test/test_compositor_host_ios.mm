// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class TestCompositorHostIOS : public TestCompositorHost {
 public:
  TestCompositorHostIOS(const gfx::Rect& bounds,
                        ui::ContextFactory* context_factory) {
    compositor_ = std::make_unique<ui::Compositor>(
        context_factory->AllocateFrameSinkId(), context_factory,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        false /* enable_pixel_canvas */);
    // TODO(sievers): Support onscreen here.
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
    compositor_->SetScaleAndSize(1.0f,
                                 gfx::Size(bounds.width(), bounds.height()),
                                 viz::LocalSurfaceId());
  }

  TestCompositorHostIOS(const TestCompositorHostIOS&) = delete;
  TestCompositorHostIOS& operator=(const TestCompositorHostIOS&) = delete;

  // Overridden from TestCompositorHost:
  void Show() override { compositor_->SetVisible(true); }
  ui::Compositor* GetCompositor() override { return compositor_.get(); }

 private:
  std::unique_ptr<ui::Compositor> compositor_;
};

TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
  return new TestCompositorHostIOS(bounds, context_factory);
}

}  // namespace ui
