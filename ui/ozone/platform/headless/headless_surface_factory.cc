// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_surface_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/common/gl_surface_egl_readback.h"
#include "ui/ozone/platform/headless/headless_window.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

const base::FilePath::CharType kDevNull[] = FILE_PATH_LITERAL("/dev/null");

base::FilePath GetPathForWidget(const base::FilePath& base_path,
                                gfx::AcceleratedWidget widget) {
  if (base_path.empty() || base_path == base::FilePath(kDevNull))
    return base_path;

    // Disambiguate multiple window output files with the window id.
#if defined(OS_WIN)
  std::string path =
      base::NumberToString(reinterpret_cast<int>(widget)) + ".png";
  std::wstring wpath(path.begin(), path.end());
  return base_path.Append(wpath);
#else
  return base_path.Append(base::NumberToString(widget) + ".png");
#endif
}

void WriteDataToFile(const base::FilePath& location, const SkBitmap& bitmap) {
  DCHECK(!location.empty());
  std::vector<unsigned char> png_data;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, true, &png_data);
  if (base::WriteFile(location, reinterpret_cast<const char*>(png_data.data()),
                      png_data.size()) < 0) {
    static bool logged_once = false;
    LOG_IF(ERROR, !logged_once)
        << "Failed to write frame to file. "
           "If running with the GPU process try --no-sandbox.";
    logged_once = true;
  }
}

// TODO(altimin): Find a proper way to capture rendering output.
class FileSurface : public SurfaceOzoneCanvas {
 public:
  explicit FileSurface(const base::FilePath& location) : base_path_(location) {}
  ~FileSurface() override {}

  // SurfaceOzoneCanvas overrides:
  void ResizeCanvas(const gfx::Size& viewport_size) override {
    surface_ = SkSurface::MakeRaster(SkImageInfo::MakeN32Premul(
        viewport_size.width(), viewport_size.height()));
  }
  sk_sp<SkSurface> GetSurface() override { return surface_; }
  void PresentCanvas(const gfx::Rect& damage) override {
    if (base_path_.empty())
      return;
    SkBitmap bitmap;
    bitmap.allocPixels(surface_->getCanvas()->imageInfo());

    // TODO(dnicoara) Use SkImage instead to potentially avoid a copy.
    // See crbug.com/361605 for details.
    if (surface_->getCanvas()->readPixels(bitmap, 0, 0)) {
      base::PostTask(FROM_HERE,
                     {base::ThreadPool(), base::MayBlock(),
                      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                     base::BindOnce(&WriteDataToFile, base_path_, bitmap));
    }
  }
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return nullptr;
  }

 private:
  base::FilePath base_path_;
  sk_sp<SkSurface> surface_;
};

class FileGLSurface : public GLSurfaceEglReadback {
 public:
  explicit FileGLSurface(const base::FilePath& location)
      : location_(location) {}

 private:
  ~FileGLSurface() override = default;

  // GLSurfaceEglReadback:
  bool HandlePixels(uint8_t* pixels) override {
    if (location_.empty())
      return true;

    const gfx::Size size = GetSize();
    SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
    SkPixmap pixmap(info, pixels, info.minRowBytes());

    SkBitmap bitmap;
    bitmap.allocPixels(info);
    if (!bitmap.writePixels(pixmap))
      return false;

    base::PostTask(FROM_HERE,
                   {base::ThreadPool(), base::MayBlock(),
                    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                   base::BindOnce(&WriteDataToFile, location_, bitmap));
    return true;
  }

  base::FilePath location_;

  DISALLOW_COPY_AND_ASSIGN(FileGLSurface);
};

class TestPixmap : public gfx::NativePixmap {
 public:
  explicit TestPixmap(gfx::BufferFormat format) : format_(format) {}

  bool AreDmaBufFdsValid() const override { return false; }
  int GetDmaBufFd(size_t plane) const override { return -1; }
  uint32_t GetDmaBufPitch(size_t plane) const override { return 0; }
  size_t GetDmaBufOffset(size_t plane) const override { return 0; }
  size_t GetDmaBufPlaneSize(size_t plane) const override { return 0; }
  uint64_t GetBufferFormatModifier() const override { return 0; }
  gfx::BufferFormat GetBufferFormat() const override { return format_; }
  size_t GetNumberOfPlanes() const override {
    return gfx::NumberOfPlanesForLinearBufferFormat(format_);
  }
  gfx::Size GetBufferSize() const override { return gfx::Size(); }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override {
    return true;
  }
  gfx::NativePixmapHandle ExportHandle() override {
    return gfx::NativePixmapHandle();
  }

 private:
  ~TestPixmap() override {}

  gfx::BufferFormat format_;

  DISALLOW_COPY_AND_ASSIGN(TestPixmap);
};

class GLOzoneEGLHeadless : public GLOzoneEGL {
 public:
  GLOzoneEGLHeadless(const base::FilePath& base_path) : base_path_(base_path) {}
  ~GLOzoneEGLHeadless() override = default;

  // GLOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override {
    return gl::InitializeGLSurface(base::MakeRefCounted<FileGLSurface>(
        GetPathForWidget(base_path_, window)));
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(size));
  }

 protected:
  // GLOzoneEGL:
  intptr_t GetNativeDisplay() override { return EGL_DEFAULT_DISPLAY; }

  bool LoadGLES2Bindings(gl::GLImplementation implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  base::FilePath base_path_;

  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLHeadless);
};

}  // namespace

HeadlessSurfaceFactory::HeadlessSurfaceFactory(base::FilePath base_path)
    : base_path_(base_path),
      swiftshader_implementation_(
          std::make_unique<GLOzoneEGLHeadless>(base_path)) {
  CheckBasePath();
}

HeadlessSurfaceFactory::~HeadlessSurfaceFactory() = default;

std::vector<gl::GLImplementation>
HeadlessSurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementation>{gl::kGLImplementationSwiftShaderGL};
}

GLOzone* HeadlessSurfaceFactory::GetGLOzone(
    gl::GLImplementation implementation) {
  switch (implementation) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationSwiftShaderGL:
      return swiftshader_implementation_.get();

    default:
      return nullptr;
  }
}

std::unique_ptr<SurfaceOzoneCanvas>
HeadlessSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget,
                                              base::TaskRunner* task_runner) {
  return std::make_unique<FileSurface>(GetPathForWidget(base_path_, widget));
}

scoped_refptr<gfx::NativePixmap> HeadlessSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return new TestPixmap(format);
}

void HeadlessSurfaceFactory::CheckBasePath() const {
  if (base_path_.empty())
    return;

  if (!DirectoryExists(base_path_) && !base::CreateDirectory(base_path_) &&
      base_path_ != base::FilePath(kDevNull))
    PLOG(FATAL) << "Unable to create output directory";

  if (!base::PathIsWritable(base_path_))
    PLOG(FATAL) << "Unable to write to output location";
}

}  // namespace ui
