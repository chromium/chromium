// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WINDOWS_WINDOWS_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_WINDOWS_WINDOWS_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

// Handles GL initialization and surface/context creation for Windows
class WindowsSurfaceFactory : public SurfaceFactoryOzone {
 public:
  WindowsSurfaceFactory();

  WindowsSurfaceFactory(const WindowsSurfaceFactory&) = delete;
  WindowsSurfaceFactory& operator=(const WindowsSurfaceFactory&) = delete;

  ~WindowsSurfaceFactory() override;

  // SurfaceFactoryOzone:
  std::vector<gl::GLImplementationParts> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(const gl::GLImplementationParts& implementation) override;

 private:
  std::unique_ptr<GLOzone> egl_implementation_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WINDOWS_WINDOWS_SURFACE_FACTORY_H_
