// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SKIA_SKIA_RENDERER_FACTORY_H_
#define UI_OZONE_DEMO_SKIA_SKIA_RENDERER_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/demo/renderer_factory.h"

namespace gl {
class GLDisplay;
}

namespace ui {

class Renderer;

class SkiaRendererFactory : public RendererFactory {
 public:
  enum RendererType {
    SKIA,
    SOFTWARE,
  };

  SkiaRendererFactory();

  SkiaRendererFactory(const SkiaRendererFactory&) = delete;
  SkiaRendererFactory& operator=(const SkiaRendererFactory&) = delete;

  ~SkiaRendererFactory() override;

  bool Initialize() override;
  std::unique_ptr<Renderer> CreateRenderer(gfx::AcceleratedWidget widget,
                                           const gfx::Size& size) override;

 private:
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SKIA_SKIA_RENDERER_FACTORY_H_
