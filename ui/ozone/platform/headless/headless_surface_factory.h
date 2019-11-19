// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class HeadlessSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit HeadlessSurfaceFactory(base::FilePath base_path);
  ~HeadlessSurfaceFactory() override;

  // SurfaceFactoryOzone:
  std::vector<gl::GLImplementation> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(gl::GLImplementation implementation) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget,
      base::TaskRunner* task_runner) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      VkDevice vk_device,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

 private:
  void CheckBasePath() const;

  // Base path for window output PNGs.
  base::FilePath base_path_;

  std::unique_ptr<GLOzone> swiftshader_implementation_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_
