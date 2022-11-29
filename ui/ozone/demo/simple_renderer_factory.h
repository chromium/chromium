// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SIMPLE_RENDERER_FACTORY_H_
#define UI_OZONE_DEMO_SIMPLE_RENDERER_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/ozone/demo/renderer_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace gl {
class GLDisplay;
}

namespace ui {

class SimpleRendererFactory : public RendererFactory {
 public:
  enum RendererType {
    GL,
    SOFTWARE,
#if BUILDFLAG(ENABLE_VULKAN)
    VULKAN,
#endif
  };

  SimpleRendererFactory();

  SimpleRendererFactory(const SimpleRendererFactory&) = delete;
  SimpleRendererFactory& operator=(const SimpleRendererFactory&) = delete;

  ~SimpleRendererFactory() override;

  bool Initialize() override;
  std::unique_ptr<Renderer> CreateRenderer(gfx::AcceleratedWidget widget,
                                           const gfx::Size& size) override;

 private:
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation_;
#endif

  RendererType type_ = SOFTWARE;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SIMPLE_RENDERER_FACTORY_H_
