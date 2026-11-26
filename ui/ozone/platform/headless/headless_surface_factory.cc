// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_surface_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/presenter.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/common/gl_surface_egl_readback.h"
#include "ui/ozone/platform/headless/headless_window.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

#if BUILDFLAG(IS_LINUX)
#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>  // nogncheck
#include <xf86drm.h>

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"  // nogncheck
#include "ui/gfx/linux/drm_util_linux.h"               // nogncheck
#include "ui/gfx/linux/gbm_buffer.h"                   // nogncheck
#include "ui/gfx/linux/gbm_device.h"                   // nogncheck
#include "ui/gfx/linux/gbm_util.h"                     // nogncheck
#include "ui/gfx/linux/gbm_wrapper.h"                  // nogncheck
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/common/native_pixmap_egl_binding.h"
#include "ui/ozone/public/ozone_switches.h"
#endif

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
  CHECK(!location.empty(), base::NotFatalUntil::M163);
  std::optional<std::vector<uint8_t>> png_data =
      gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap,
                                            /*discard_transparency=*/true);
  if (!png_data || !base::WriteFile(location, png_data.value())) {
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

// Surfaceless presenter used when SupportsNativePixmaps() is true and no
// --ozone-dump-file path is set (e.g. Mesa's EGL_PLATFORM_GBM_KHR exposes only
// EGL_WINDOW_BIT configs, so pbuffer-backed FileGLSurface cannot be used).
class HeadlessPresenter : public gl::Presenter {
 public:
  HeadlessPresenter() = default;

  HeadlessPresenter(const HeadlessPresenter&) = delete;
  HeadlessPresenter& operator=(const HeadlessPresenter&) = delete;

  // gl::Presenter:
  bool ScheduleOverlayPlane(
      gl::OverlayImage image,
      gfx::GpuFenceHandle gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override {
    return true;
  }

  void Present(SwapCompletionCallback completion_callback,
               PresentationCallback presentation_callback,
               gfx::FrameData data) override {
    auto finish = base::BindOnce(
        [](SwapCompletionCallback completion_callback,
           PresentationCallback presentation_callback) {
          std::move(completion_callback)
              .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
          std::move(presentation_callback)
              .Run(gfx::PresentationFeedback(base::TimeTicks::Now(),
                                             base::TimeDelta(), /*flags=*/0));
        },
        std::move(completion_callback), std::move(presentation_callback));
    if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(finish));
    } else {
      std::move(finish).Run();
    }
  }

 private:
  ~HeadlessPresenter() override = default;
};

class TestPixmap : public gfx::NativePixmap {
 public:
  explicit TestPixmap(viz::SharedImageFormat format) : format_(format) {}

  TestPixmap(const TestPixmap&) = delete;
  TestPixmap& operator=(const TestPixmap&) = delete;

  bool AreDmaBufFdsValid() const override { return false; }
  int GetDmaBufFd(size_t plane) const override { return -1; }
  uint32_t GetDmaBufPitch(size_t plane) const override { return 0; }
  size_t GetDmaBufOffset(size_t plane) const override { return 0; }
  size_t GetDmaBufPlaneSize(size_t plane) const override { return 0; }
  uint64_t GetFormatModifier() const override { return 0; }
  viz::SharedImageFormat GetSharedImageFormat() const override {
    return format_;
  }
  size_t GetNumberOfPlanes() const override { return format_.NumberOfPlanes(); }
  bool SupportsZeroCopyWebGPUImport() const override { return false; }
  gfx::Size GetBufferSize() const override { return gfx::Size(); }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            const gfx::OverlayPlaneData& overlay_plane_data,
                            gfx::GpuFenceHandle acquire_fence) override {
    return true;
  }
  gfx::NativePixmapHandle ExportHandle() const override {
    return gfx::NativePixmapHandle();
  }

 private:
  ~TestPixmap() override {}

  viz::SharedImageFormat format_;
};

class GLOzoneEGLHeadless : public GLOzoneEGL {
 public:
  GLOzoneEGLHeadless(const base::FilePath& base_path,
                     HeadlessSurfaceFactory* surface_factory)
      : base_path_(base_path), surface_factory_(surface_factory) {}

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

  scoped_refptr<gl::Presenter> CreateSurfacelessViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override {
    if (base_path_.empty() && surface_factory_ &&
        surface_factory_->SupportsNativePixmaps()) {
      return base::MakeRefCounted<HeadlessPresenter>();
    }
    return nullptr;
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override {
    gl::GLDisplayEGL* egl_display = display->GetAs<gl::GLDisplayEGL>();
    if (egl_display->IsEGLSurfacelessContextSupported() && size.IsEmpty()) {
      return gl::InitializeGLSurface(
          base::MakeRefCounted<gl::SurfacelessEGL>(egl_display, size));
    }
    return gl::InitializeGLSurface(
        base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(egl_display, size));
  }

  bool CanImportNativePixmap(viz::SharedImageFormat format) override {
#if BUILDFLAG(IS_LINUX)
    auto* display = gl::GLSurfaceEGL::GetGLDisplayEGL();
    if (!display || !display->ext->b_EGL_EXT_image_dma_buf_import) {
      return false;
    }
    return NativePixmapEGLBinding::IsSharedImageFormatSupported(format);
#else
    return false;
#endif
  }

  std::unique_ptr<NativePixmapGLBinding> ImportNativePixmap(
      scoped_refptr<gfx::NativePixmap> pixmap,
      viz::SharedImageFormat plane_format,
      std::optional<int> plane_index,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id) override {
#if BUILDFLAG(IS_LINUX)
    return NativePixmapEGLBinding::Create(pixmap, plane_format, plane_index,
                                          plane_size, color_space, target,
                                          texture_id);
#else
    return nullptr;
#endif
  }

 protected:
  // GLOzoneEGL:
  gl::EGLDisplayPlatform GetNativeDisplay() override {
#if BUILDFLAG(IS_LINUX)
    if (surface_factory_) {
      gl::EGLDisplayPlatform gbm_display = surface_factory_->GetNativeDisplay();
      if (gbm_display.Valid()) {
        return gbm_display;
      }
    }
#endif
    return gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
  }

  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  base::FilePath base_path_;
  raw_ptr<HeadlessSurfaceFactory> surface_factory_;
};

}  // namespace

HeadlessSurfaceFactory::HeadlessSurfaceFactory(base::FilePath base_path)
    : base_path_(base_path) {
  swiftshader_implementation_ =
      std::make_unique<GLOzoneEGLHeadless>(base_path, this);
  CheckBasePath();
}

HeadlessSurfaceFactory::~HeadlessSurfaceFactory() = default;

bool HeadlessSurfaceFactory::SupportsNativePixmaps() const {
#if BUILDFLAG(IS_LINUX)
  if (gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
    return false;
  }
  return gbm_device_ != nullptr;
#else
  return false;
#endif
}

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
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    std::optional<gfx::Size> framebuffer_size) {
#if BUILDFLAG(IS_LINUX)
  if (framebuffer_size &&
      !gfx::Rect(size).Contains(gfx::Rect(*framebuffer_size))) {
    return nullptr;
  }
  InitializeGbm();
  if (gbm_device_) {
    const uint32_t fourcc =
        static_cast<uint32_t>(GetFourCCFormatFromSharedImageFormat(format));
    auto buffer =
        gbm_device_->CreateBuffer(fourcc, size, BufferUsageToGbmFlags(usage));
    if (!buffer) {
      return nullptr;
    }
    gfx::NativePixmapHandle handle = buffer->ExportHandle();
    if (handle.planes.empty()) {
      return nullptr;
    }
    return base::MakeRefCounted<gfx::NativePixmapDmaBuf>(size, format,
                                                         std::move(handle));
  }
#endif
  return new TestPixmap(format);
}

#if BUILDFLAG(IS_LINUX)
void HeadlessSurfaceFactory::InitializeGbm() {
  if (gbm_init_attempted_) {
    return;
  }
  gbm_init_attempted_ = true;

  auto try_open_node = [this](const std::string& path, bool skip_vgem) -> bool {
    base::ScopedFD fd(HANDLE_EINTR(open(path.c_str(), O_RDWR | O_CLOEXEC)));
    if (!fd.is_valid()) {
      return false;
    }
    if (skip_vgem) {
      using ScopedDrmVersion =
          std::unique_ptr<drmVersion, decltype(&drmFreeVersion)>;
      ScopedDrmVersion version(drmGetVersion(fd.get()), &drmFreeVersion);
      if (!version || !version->name || version->name_len <= 0) {
        return false;
      }
      if (std::string_view(version->name,
                           static_cast<size_t>(version->name_len)) == "vgem") {
        return false;
      }
    }
    gbm_device* device = gbm_create_device(fd.get());
    if (!device) {
      return false;
    }
    drm_fd_ = std::move(fd);
    native_display_ = gl::EGLDisplayPlatform(
        reinterpret_cast<EGLNativeDisplayType>(device), EGL_PLATFORM_GBM_KHR);
    gbm_device_ = WrapGbmDevice(device);
    return true;
  };

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kRenderNodeOverride)) {
    const std::string override_path =
        command_line->GetSwitchValueASCII(switches::kRenderNodeOverride);
    if (!override_path.empty() &&
        !try_open_node(override_path, /*skip_vgem=*/false)) {
      LOG(WARNING) << "Failed to initialize GBM for override node "
                   << override_path;
    }
    return;
  }

  constexpr uint32_t kRenderNodeStart = 128;
  constexpr uint32_t kRenderNodeEnd = kRenderNodeStart + 16;
  for (uint32_t i = kRenderNodeStart; i < kRenderNodeEnd; ++i) {
    if (try_open_node(base::StringPrintf("/dev/dri/renderD%u", i),
                      /*skip_vgem=*/true)) {
      return;
    }
  }

  VLOG(1) << "No usable DRM render node found; using CPU pixmaps.";
}

gl::EGLDisplayPlatform HeadlessSurfaceFactory::GetNativeDisplay() const {
  if (!base_path_.empty() ||
      gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts()) ||
      !native_display_.Valid() ||
      !gl::g_driver_egl.client_ext.b_EGL_EXT_platform_base ||
      !gl::g_driver_egl.client_ext.b_EGL_KHR_platform_gbm) {
    return gl::EGLDisplayPlatform();
  }
  return native_display_;
}

bool HeadlessSurfaceFactory::IsNativePixmapConfigSupported(
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) const {
  if (!SupportsNativePixmaps()) {
    return false;
  }
  const uint32_t fourcc =
      static_cast<uint32_t>(GetFourCCFormatFromSharedImageFormat(format));
  if (!gbm_device_->CanCreateBufferForFormat(fourcc)) {
    return false;
  }
  return gfx::ClientNativePixmapDmaBuf::IsConfigurationSupported(format, usage);
}

bool HeadlessSurfaceFactory::CanCreateNativePixmapForFormat(
    viz::SharedImageFormat format) {
  InitializeGbm();
  if (!gbm_device_) {
    return false;
  }
  const uint32_t fourcc =
      static_cast<uint32_t>(GetFourCCFormatFromSharedImageFormat(format));
  // Hardware video decoders import decoded NV12/P010 dmabufs via
  // CreateNativePixmapFromHandle() rather than allocating via GBM. Drivers
  // such as iris and radeonsi report false for NV12/P010 allocation queries
  // even though EGL dmabuf import succeeds (see https://crbug.com/343352540).
  if (fourcc == DRM_FORMAT_NV12 || fourcc == DRM_FORMAT_P010) {
    return true;
  }
  return gbm_device_->CanCreateBufferForFormat(fourcc);
}

scoped_refptr<gfx::NativePixmap>
HeadlessSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    viz::SharedImageFormat format,
    gfx::NativePixmapHandle handle) {
  InitializeGbm();
  if (!gbm_device_) {
    return nullptr;
  }
  if (handle.planes.empty()) {
    return nullptr;
  }
  // Wrap imported dmabufs directly in NativePixmapDmaBuf rather than routing
  // through GbmDevice::CreateBufferFromHandle(), which enforces scanout
  // allocation checks that reject YUV video buffers. EGL import validates
  // the real dmabuf attributes below (see https://crbug.com/343352540).
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap =
      base::MakeRefCounted<gfx::NativePixmapDmaBuf>(size, format,
                                                    std::move(handle));
  if (!pixmap->AreDmaBufFdsValid()) {
    return nullptr;
  }
  return pixmap;
}
#endif  // BUILDFLAG(IS_LINUX)

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
