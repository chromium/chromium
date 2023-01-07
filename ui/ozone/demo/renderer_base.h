// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_RENDERER_BASE_H_
#define UI_OZONE_DEMO_RENDERER_BASE_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/demo/renderer.h"

namespace ui {

class RendererBase : public Renderer {
 public:
  RendererBase(gfx::AcceleratedWidget widget, const gfx::Size& size);
  ~RendererBase() override;

 protected:
  float CurrentFraction() const;
  float NextFraction();

  gfx::AcceleratedWidget widget_;
  gfx::Size size_;

  int iteration_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_RENDERER_BASE_H_
