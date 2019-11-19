// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/surface_factory_cast.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromecast/public/cast_egl_platform.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

class DummySurface : public SurfaceOzoneCanvas {
 public:
  DummySurface() {}
  ~DummySurface() override {}

  // SurfaceOzoneCanvas implementation:
  sk_sp<SkSurface> GetSurface() override { return surface_; }

  void ResizeCanvas(const gfx::Size& viewport_size) override {
    surface_ =
        SkSurface::MakeNull(viewport_size.width(), viewport_size.height());
  }

  void PresentCanvas(const gfx::Rect& damage) override {}

  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return nullptr;
  }

 private:
  sk_sp<SkSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(DummySurface);
};

class CastPixmap : public gfx::NativePixmap {
 public:
  CastPixmap() {}

  bool AreDmaBufFdsValid() const override { return false; }
  int GetDmaBufFd(size_t plane) const override { return -1; }
  uint32_t GetDmaBufPitch(size_t plane) const override { return 0; }
  size_t GetDmaBufOffset(size_t plane) const override { return 0; }
  size_t GetDmaBufPlaneSize(size_t plane) const override { return 0; }
  uint64_t GetBufferFormatModifier() const override { return 0; }
  gfx::BufferFormat GetBufferFormat() const override {
    return gfx::BufferFormat::BGRA_8888;
  }
  size_t GetNumberOfPlanes() const override { return 1; }
  gfx::Size GetBufferSize() const override { return gfx::Size(); }
  uint32_t GetUniqueId() const override { return 0; }

  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override {
    return false;
  }
  gfx::NativePixmapHandle ExportHandle() override {
    return gfx::NativePixmapHandle();
  }

 private:
  ~CastPixmap() override {}

  DISALLOW_COPY_AND_ASSIGN(CastPixmap);
};

}  // namespace

SurfaceFactoryCast::SurfaceFactoryCast() : SurfaceFactoryCast(nullptr) {}

SurfaceFactoryCast::SurfaceFactoryCast(
    std::unique_ptr<chromecast::CastEglPlatform> egl_platform) {
  if (egl_platform) {
    egl_implementation_ =
        std::make_unique<GLOzoneEglCast>(std::move(egl_platform));
  }
}

SurfaceFactoryCast::~SurfaceFactoryCast() {}

std::vector<gl::GLImplementation>
SurfaceFactoryCast::GetAllowedGLImplementations() {
  std::vector<gl::GLImplementation> impls;
  if (egl_implementation_)
    impls.push_back(gl::kGLImplementationEGLGLES2);
  return impls;
}

GLOzone* SurfaceFactoryCast::GetGLOzone(gl::GLImplementation implementation) {
  switch (implementation) {
    case gl::kGLImplementationEGLGLES2:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryCast::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget,
    base::TaskRunner* task_runner) {
  // Software canvas support only in headless mode
  if (egl_implementation_)
    return nullptr;
  return base::WrapUnique<SurfaceOzoneCanvas>(new DummySurface());
}

scoped_refptr<gfx::NativePixmap> SurfaceFactoryCast::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return base::MakeRefCounted<CastPixmap>();
}

}  // namespace ui
