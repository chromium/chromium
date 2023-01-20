// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_H_
#define UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_H_

namespace cc {
class LayerTreeHost;
}

namespace gfx {
class Rect;
}

namespace ui {

class Compositor;
class ContextFactory;

class TestCompositorHost {
 public:
  virtual ~TestCompositorHost() {}

  // Creates a new TestCompositorHost. The caller owns the returned value.
  static TestCompositorHost* Create(const gfx::Rect& bounds,
                                    ui::ContextFactory* context_factory);

  // Shows the TestCompositorHost. Must only be called once.
  virtual void Show() = 0;

  virtual Compositor* GetCompositor() = 0;

  cc::LayerTreeHost* GetLayerTreeHost();
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_COMPOSITOR_HOST_H_
