// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_RENDERER_FACTORY_H_
#define UI_OZONE_DEMO_RENDERER_FACTORY_H_

#include <memory>

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class Renderer;

class RendererFactory {
 public:
  virtual ~RendererFactory();

  virtual bool Initialize() = 0;

  virtual std::unique_ptr<Renderer> CreateRenderer(
      gfx::AcceleratedWidget widget,
      const gfx::Size& size) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_RENDERER_FACTORY_H_
