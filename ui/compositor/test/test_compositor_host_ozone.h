// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_OZONE_H_
#define UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_OZONE_H_

#include "ui/compositor/test/test_compositor_host.h"

#include <memory>

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class PlatformWindow;

class TestCompositorHostOzone : public TestCompositorHost {
 public:
  TestCompositorHostOzone(const gfx::Rect& bounds,
                          ui::ContextFactory* context_factory);
  TestCompositorHostOzone(const TestCompositorHostOzone&) = delete;
  TestCompositorHostOzone& operator=(const TestCompositorHostOzone&) = delete;
  ~TestCompositorHostOzone() override;

 private:
  class StubPlatformWindowDelegate;

  // Overridden from TestCompositorHost:
  void Show() override;
  ui::Compositor* GetCompositor() override;

  gfx::Rect bounds_;
  ui::Compositor compositor_;
  std::unique_ptr<PlatformWindow> window_;
  std::unique_ptr<StubPlatformWindowDelegate> window_delegate_;
  viz::ParentLocalSurfaceIdAllocator allocator_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_OZONE_H_
