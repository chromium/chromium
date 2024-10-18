// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/surface_factory_cast.h"

#include <memory>
#include <utility>

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

  DummySurface(const DummySurface&) = delete;
  DummySurface& operator=(const DummySurface&) = delete;

  ~DummySurface() override {}

  // SurfaceOzoneCanvas implementation:
  SkCanvas* GetCanvas() override { return surface_->getCanvas(); }

  void ResizeCanvas(const gfx::Size& viewport_size, float scale) override {
    surface_ = SkSurfaces::Null(viewport_size.width(), viewport_size.height());
  }

  void PresentCanvas(const gfx::Rect& damage) override {}

  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return nullptr;
  }

 private:
  sk_sp<SkSurface> surface_;
};

class CastPixmap : public gfx::NativePixmap {
 public:
  CastPixmap() {}

  CastPixmap(const CastPixmap&) = delete;
  CastPixmap& operator=(const CastPixmap&) = delete;

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
  bool SupportsZeroCopyWebGPUImport() const override {
    // TODO(crbug.com/40201271): Figure out how to import multi-planar pixmap
    // into WebGPU without copy.
    return false;
  }
  gfx::Size GetBufferSize() const override { return gfx::Size(); }
  uint32_t GetUniqueId() const override { return 0; }

  bool ScheduleOverlayPlane(
      gfx::AcceleratedWidget widget,
      const gfx::OverlayPlaneData& overlay_plane_data,
      std::vector<gfx::GpuFence> acquire_fences,
      std::vector<gfx::GpuFence> release_fences) override {
    return false;
  }
  gfx::NativePixmapHandle ExportHandle() const override {
    return gfx::NativePixmapHandle();
  }

 private:
  ~CastPixmap() override {}
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

std::vector<gl::GLImplementationParts>
SurfaceFactoryCast::GetAllowedGLImplementations() {
  std::vector<gl::GLImplementationParts> impls;
  if (egl_implementation_)
    impls.emplace_back(
        gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
  return impls;
}

GLOzone* SurfaceFactoryCast::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryCast::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  // Software canvas support only in headless mode
  if (egl_implementation_)
    return nullptr;
  return base::WrapUnique<SurfaceOzoneCanvas>(new DummySurface());
}

scoped_refptr<gfx::NativePixmap> SurfaceFactoryCast::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    std::optional<gfx::Size> framebuffer_size) {
  DCHECK(!framebuffer_size || framebuffer_size == size);
  return base::MakeRefCounted<CastPixmap>();
}

}  // namespace ui
