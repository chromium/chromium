// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host_ozone.h"

#include <memory>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

// Stub implementation of PlatformWindowDelegate that stores the
// AcceleratedWidget.
class TestCompositorHostOzone::StubPlatformWindowDelegate
    : public PlatformWindowDelegate {
 public:
  StubPlatformWindowDelegate() {}

  StubPlatformWindowDelegate(const StubPlatformWindowDelegate&) = delete;
  StubPlatformWindowDelegate& operator=(const StubPlatformWindowDelegate&) =
      delete;

  ~StubPlatformWindowDelegate() override {}

  gfx::AcceleratedWidget widget() const { return widget_; }

  // PlatformWindowDelegate:
  void OnBoundsChanged(const BoundsChange& change) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(PlatformWindowState old_state,
                            PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    widget_ = widget;
  }
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {
    widget_ = gfx::kNullAcceleratedWidget;
  }
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}
  int64_t OnStateUpdate(const State& old, const State& latest) override {
    return -1;
  }

 private:
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
};

TestCompositorHostOzone::TestCompositorHostOzone(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory)
    : bounds_(bounds),
      compositor_(context_factory->AllocateFrameSinkId(),
                  context_factory,
                  base::SingleThreadTaskRunner::GetCurrentDefault(),
                  false /* enable_pixel_canvas */),
      window_delegate_(std::make_unique<StubPlatformWindowDelegate>()) {}

TestCompositorHostOzone::~TestCompositorHostOzone() {
  // |window_| should be destroyed earlier than |window_delegate_| as it refers
  // to its delegate on destroying.
  window_.reset();
}

void TestCompositorHostOzone::Show() {
  ui::PlatformWindowInitProperties properties;
  properties.bounds = bounds_;
  // Create a PlatformWindow to get the AcceleratedWidget backing it.
  window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
      window_delegate_.get(), std::move(properties));
  window_->Show();
  DCHECK_NE(window_delegate_->widget(), gfx::kNullAcceleratedWidget);

  allocator_.GenerateId();
  compositor_.SetAcceleratedWidget(window_delegate_->widget());
  compositor_.SetScaleAndSize(1.0f, bounds_.size(),
                              allocator_.GetCurrentLocalSurfaceId());
  compositor_.SetVisible(true);
}

ui::Compositor* TestCompositorHostOzone::GetCompositor() {
  return &compositor_;
}

// static
TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
  return new TestCompositorHostOzone(bounds, context_factory);
}

}  // namespace ui
