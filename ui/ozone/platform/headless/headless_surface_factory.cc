// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_surface_factory.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/common/gl_surface_egl_readback.h"
#include "ui/ozone/platform/headless/headless_window.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

#if BUILDFLAG(ENABLE_VULKAN) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA))
#include "ui/ozone/platform/headless/vulkan_implementation_headless.h"
#endif

namespace ui {

namespace {

const base::FilePath::CharType kDevNull[] = FILE_PATH_LITERAL("/dev/null");

base::FilePath GetPathForWidget(const base::FilePath& base_path,
                                gfx::AcceleratedWidget widget) {
  if (base_path.empty() || base_path == base::FilePath(kDevNull))
    return base_path;

  // Disambiguate multiple window output files with the window id.
  return base_path.Append(base::NumberToString(widget) + ".png");
}

void WriteDataToFile(const base::FilePath& location, const SkBitmap& bitmap) {
  DCHECK(!location.empty());
  std::vector<unsigned char> png_data;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, true, &png_data);
  if (!base::WriteFile(location, png_data)) {
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
  void ResizeCanvas(const gfx::Size& viewport_size, float scale) override {
    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    surface_ =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(viewport_size.width(),
                                                      viewport_size.height()),
                           &props);
  }
  SkCanvas* GetCanvas() override { return surface_->getCanvas(); }
  void PresentCanvas(const gfx::Rect& damage) override {
    if (base_path_.empty())
      return;
    SkBitmap bitmap;
    bitmap.allocPixels(surface_->getCanvas()->imageInfo());

    // TODO(dnicoara) Use SkImage instead to potentially avoid a copy.
    // See crbug.com/361605 for details.
    if (surface_->getCanvas()->readPixels(bitmap, 0, 0)) {
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
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
  FileGLSurface(gl::GLDisplayEGL* display, const base::FilePath& location)
      : GLSurfaceEglReadback(display), location_(location) {}

  FileGLSurface(const FileGLSurface&) = delete;
  FileGLSurface& operator=(const FileGLSurface&) = delete;

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

    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&WriteDataToFile, location_, bitmap));
    return true;
  }

  base::FilePath location_;
};

class TestPixmap : public gfx::NativePixmap {
 public:
  explicit TestPixmap(gfx::BufferFormat format) : format_(format) {}

  TestPixmap(const TestPixmap&) = delete;
  TestPixmap& operator=(const TestPixmap&) = delete;

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
  bool SupportsZeroCopyWebGPUImport() const override { return false; }
  gfx::Size GetBufferSize() const override { return gfx::Size(); }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(
      gfx::AcceleratedWidget widget,
      const gfx::OverlayPlaneData& overlay_plane_data,
      std::vector<gfx::GpuFence> acquire_fences,
      std::vector<gfx::GpuFence> release_fences) override {
    return true;
  }
  gfx::NativePixmapHandle ExportHandle() const override {
    return gfx::NativePixmapHandle();
  }

 private:
  ~TestPixmap() override {}

  gfx::BufferFormat format_;
};

class GLOzoneEGLHeadless : public GLOzoneEGL {
 public:
  GLOzoneEGLHeadless(const base::FilePath& base_path) : base_path_(base_path) {}

  GLOzoneEGLHeadless(const GLOzoneEGLHeadless&) = delete;
  GLOzoneEGLHeadless& operator=(const GLOzoneEGLHeadless&) = delete;

  ~GLOzoneEGLHeadless() override = default;

  // GLOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override {
    return gl::InitializeGLSurface(base::MakeRefCounted<FileGLSurface>(
        display->GetAs<gl::GLDisplayEGL>(),
        GetPathForWidget(base_path_, window)));
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(
            display->GetAs<gl::GLDisplayEGL>(), size));
  }

 protected:
  // GLOzoneEGL:
  gl::EGLDisplayPlatform GetNativeDisplay() override {
    return gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
  }

  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  base::FilePath base_path_;
};

}  // namespace

HeadlessSurfaceFactory::HeadlessSurfaceFactory(base::FilePath base_path)
    : base_path_(base_path),
      swiftshader_implementation_(
          std::make_unique<GLOzoneEGLHeadless>(base_path)) {
  CheckBasePath();
}

HeadlessSurfaceFactory::~HeadlessSurfaceFactory() = default;

std::vector<gl::GLImplementationParts>
HeadlessSurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationEGLANGLE),
  };
}

GLOzone* HeadlessSurfaceFactory::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      return swiftshader_implementation_.get();

    default:
      return nullptr;
  }
}

std::unique_ptr<SurfaceOzoneCanvas>
HeadlessSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  return std::make_unique<FileSurface>(GetPathForWidget(base_path_, widget));
}

scoped_refptr<gfx::NativePixmap> HeadlessSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    std::optional<gfx::Size> framebuffer_size) {
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

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
HeadlessSurfaceFactory::CreateVulkanImplementation(
    bool use_swiftshader,
    bool allow_protected_memory) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA)
  return std::make_unique<VulkanImplementationHeadless>(use_swiftshader);
#else
  return nullptr;
#endif
}
#endif  // BUILDFLAG(ENABLE_VULKAN)

}  // namespace ui
