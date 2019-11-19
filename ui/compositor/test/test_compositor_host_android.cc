// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class TestCompositorHostAndroid : public TestCompositorHost {
 public:
  TestCompositorHostAndroid(
      const gfx::Rect& bounds,
      ui::ContextFactory* context_factory,
      ui::ContextFactoryPrivate* context_factory_private) {
    compositor_ = std::make_unique<ui::Compositor>(
        context_factory_private->AllocateFrameSinkId(), context_factory,
        context_factory_private, base::ThreadTaskRunnerHandle::Get(),
        false /* enable_pixel_canvas */);
    // TODO(sievers): Support onscreen here.
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
    compositor_->SetScaleAndSize(1.0f,
                                 gfx::Size(bounds.width(), bounds.height()),
                                 viz::LocalSurfaceIdAllocation());
  }

  // Overridden from TestCompositorHost:
  void Show() override { compositor_->SetVisible(true); }
  ui::Compositor* GetCompositor() override { return compositor_.get(); }

 private:
  std::unique_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostAndroid);
};

TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  return new TestCompositorHostAndroid(bounds, context_factory,
                                       context_factory_private);
}

}  // namespace ui
