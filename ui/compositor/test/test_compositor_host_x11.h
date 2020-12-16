// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_X11_H_
#define UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_X11_H_

#include <memory>

#include "base/time/time.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/test_compositor_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {
class XScopedEventSelector;
}

namespace ui {

class TestCompositorHostX11 : public TestCompositorHost {
 public:
  TestCompositorHostX11(const gfx::Rect& bounds,
                        ui::ContextFactory* context_factory);
  TestCompositorHostX11(const TestCompositorHostX11&) = delete;
  TestCompositorHostX11& operator=(const TestCompositorHostX11&) = delete;
  ~TestCompositorHostX11() override;

 private:
  // Overridden from TestCompositorHost:
  void Show() override;
  ui::Compositor* GetCompositor() override;

  gfx::Rect bounds_;

  ui::ContextFactory* context_factory_;

  ui::Compositor compositor_;

  x11::Window window_;

  std::unique_ptr<x11::XScopedEventSelector> window_events_;
  viz::ParentLocalSurfaceIdAllocator allocator_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_X11_H_
