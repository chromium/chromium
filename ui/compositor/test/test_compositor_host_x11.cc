// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/base/x/x11_window_event_manager.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

class TestCompositorHostX11 : public TestCompositorHost {
 public:
  TestCompositorHostX11(const gfx::Rect& bounds,
                        ui::ContextFactory* context_factory,
                        ui::ContextFactoryPrivate* context_factory_private);
  ~TestCompositorHostX11() override;

 private:
  // Overridden from TestCompositorHost:
  void Show() override;
  ui::Compositor* GetCompositor() override;

  gfx::Rect bounds_;

  ui::ContextFactory* context_factory_;
  ui::ContextFactoryPrivate* context_factory_private_;

  ui::Compositor compositor_;

  XID window_;

  std::unique_ptr<XScopedEventSelector> window_events_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostX11);
};

TestCompositorHostX11::TestCompositorHostX11(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private)
    : bounds_(bounds),
      context_factory_(context_factory),
      context_factory_private_(context_factory_private),
      compositor_(context_factory_private_->AllocateFrameSinkId(),
                  context_factory_,
                  context_factory_private_,
                  base::ThreadTaskRunnerHandle::Get(),
                  false /* enable_surface_synchronization */,
                  false /* enable_pixel_canvas */) {}

TestCompositorHostX11::~TestCompositorHostX11() {}

void TestCompositorHostX11::Show() {
  XDisplay* display = gfx::GetXDisplay();
  XSetWindowAttributes swa;
  swa.override_redirect = x11::True;
  window_ = XCreateWindow(
      display, XRootWindow(display, DefaultScreen(display)),  // parent
      bounds_.x(), bounds_.y(), bounds_.width(), bounds_.height(),
      0,               // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      CWOverrideRedirect, &swa);
  window_events_.reset(
      new XScopedEventSelector(window_, StructureNotifyMask | ExposureMask));
  XMapWindow(display, window_);

  while (1) {
    XEvent event;
    XNextEvent(display, &event);
    if (event.type == MapNotify && event.xmap.window == window_)
      break;
  }
  compositor_.SetAcceleratedWidget(window_);
  compositor_.SetScaleAndSize(1.0f, bounds_.size(), viz::LocalSurfaceId(),
                              base::TimeTicks());
  compositor_.SetVisible(true);
}

ui::Compositor* TestCompositorHostX11::GetCompositor() {
  return &compositor_;
}

// static
TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  return new TestCompositorHostX11(bounds, context_factory,
                                   context_factory_private);
}

}  // namespace ui
