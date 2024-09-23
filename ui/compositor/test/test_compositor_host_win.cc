// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include <windows.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/win/window_impl.h"

namespace ui {

class TestCompositorHostWin : public TestCompositorHost,
                              public gfx::WindowImpl {
 public:
  TestCompositorHostWin(const gfx::Rect& bounds,
                        ui::ContextFactory* context_factory) {
    Init(NULL, bounds);
    compositor_ = std::make_unique<ui::Compositor>(
        context_factory->AllocateFrameSinkId(), context_factory,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        false /* enable_pixel_canvas */);
    allocator_.GenerateId();
    compositor_->SetAcceleratedWidget(hwnd());
    compositor_->SetScaleAndSize(1.0f, GetSize(),
                                 allocator_.GetCurrentLocalSurfaceId());
  }

  TestCompositorHostWin(const TestCompositorHostWin&) = delete;
  TestCompositorHostWin& operator=(const TestCompositorHostWin&) = delete;

  ~TestCompositorHostWin() override { DestroyWindow(hwnd()); }

  // Overridden from TestCompositorHost:
  void Show() override {
    ShowWindow(hwnd(), SW_SHOWNORMAL);
    compositor_->SetVisible(true);
  }
  ui::Compositor* GetCompositor() override { return compositor_.get(); }

 private:
  CR_BEGIN_MSG_MAP_EX(TestCompositorHostWin)
    CR_MSG_WM_PAINT(OnPaint)
  CR_END_MSG_MAP()

  void OnPaint(HDC dc) {
    compositor_->ScheduleFullRedraw();
    ValidateRect(hwnd(), NULL);
  }

  gfx::Size GetSize() {
    RECT r;
    GetClientRect(hwnd(), &r);
    return gfx::Rect(r).size();
  }

  std::unique_ptr<ui::Compositor> compositor_;
  viz::ParentLocalSurfaceIdAllocator allocator_;

  CR_MSG_MAP_CLASS_DECLARATIONS(TestCompositorHostWin)
};

TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
  return new TestCompositorHostWin(bounds, context_factory);
}

}  // namespace ui
