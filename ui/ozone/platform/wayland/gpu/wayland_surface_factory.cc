// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/presenter.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/common/native_pixmap_egl_binding.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/gpu/gl_surface_egl_readback_wayland.h"
#include "ui/ozone/platform/wayland/gpu/gl_surface_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_canvas_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

#if defined(WAYLAND_GBM)
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_device.h"  // nogncheck
#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "ui/ozone/platform/wayland/gpu/vulkan_implementation_wayland.h"
#endif

namespace ui {

namespace {

class GLOzoneEGLWayland : public GLOzoneEGL {
 public:
  GLOzoneEGLWayland(WaylandConnection* connection,
                    WaylandBufferManagerGpu* buffer_manager)
      : connection_(connection), buffer_manager_(buffer_manager) {}

  GLOzoneEGLWayland(const GLOzoneEGLWayland&) = delete;
  GLOzoneEGLWayland& operator=(const GLOzoneEGLWayland&) = delete;

  ~GLOzoneEGLWayland() override {}

  bool CanImportNativePixmap(gfx::BufferFormat format) override;

  std::unique_ptr<NativePixmapGLBinding> ImportNativePixmap(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id) override;

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget widget) override;

  scoped_refptr<gl::Presenter> CreateSurfacelessViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override;

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override;

 protected:
  gl::EGLDisplayPlatform GetNativeDisplay() override;
  bool LoadGLES2Bindings(const gl::GLImplementationParts& impl) override;

 private:
  const raw_ptr<WaylandConnection, AcrossTasksDanglingUntriaged> connection_;
  const raw_ptr<WaylandBufferManagerGpu, AcrossTasksDanglingUntriaged>
      buffer_manager_;
};

bool GLOzoneEGLWayland::CanImportNativePixmap(gfx::BufferFormat format) {
  if (!gl::GLSurfaceEGL::GetGLDisplayEGL()
           ->ext->b_EGL_EXT_image_dma_buf_import) {
    return false;
  }

  return NativePixmapEGLBinding::IsBufferFormatSupported(format);
}

std::unique_ptr<NativePixmapGLBinding> GLOzoneEGLWayland::ImportNativePixmap(
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat plane_format,
    gfx::BufferPlane plane,
    gfx::Size plane_size,
    const gfx::ColorSpace& color_space,
    GLenum target,
    GLuint texture_id) {
  return NativePixmapEGLBinding::Create(pixmap, plane_format, plane, plane_size,
                                        color_space, target, texture_id);
}

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateViewGLSurface(
    gl::GLDisplay* display,
    gfx::AcceleratedWidget widget) {
  // If we run with software GL implementation, use GLSurface which will read
  // pixels back and present via shared memory.
  if (gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<GLSurfaceEglReadbackWayland>(
            display->GetAs<gl::GLDisplayEGL>(), widget, buffer_manager_));
  }

  if ((gl::GetGLImplementation() != gl::kGLImplementationEGLGLES2 &&
       gl::GetGLImplementation() != gl::kGLImplementationEGLANGLE) ||
      !connection_) {
    return nullptr;
  }

  WaylandWindow* window = connection_->window_manager()->GetWindow(widget);
  if (!window) {
    return nullptr;
  }

  // The wl_egl_window needs to be created before the GLSurface so it can be
  // used in the GLSurface constructor.
  auto egl_window = CreateWaylandEglWindow(window);
  if (!egl_window) {
    return nullptr;
  }
  return gl::InitializeGLSurface(new GLSurfaceWayland(
      display->GetAs<gl::GLDisplayEGL>(), std::move(egl_window), window));
}

scoped_refptr<gl::Presenter> GLOzoneEGLWayland::CreateSurfacelessViewGLSurface(
    gl::GLDisplay* display,
    gfx::AcceleratedWidget window) {
  if (gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
    return nullptr;
  } else {
#if defined(WAYLAND_GBM)
  // If there is a gbm device available, use surfaceless gl surface.
  if (!buffer_manager_->GetGbmDevice()) {
    return nullptr;
  }
  return base::MakeRefCounted<GbmSurfacelessWayland>(
      display->GetAs<gl::GLDisplayEGL>(), buffer_manager_, window);
#else
  return nullptr;
#endif
  }
}

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateOffscreenGLSurface(
    gl::GLDisplay* display,
    const gfx::Size& size) {
  if (display->GetAs<gl::GLDisplayEGL>()->IsEGLSurfacelessContextSupported() &&
      size.width() == 0 && size.height() == 0) {
    return gl::InitializeGLSurface(
        new gl::SurfacelessEGL(display->GetAs<gl::GLDisplayEGL>(), size));
  } else {
    return gl::InitializeGLSurface(
        new gl::PbufferGLSurfaceEGL(display->GetAs<gl::GLDisplayEGL>(), size));
  }
}

gl::EGLDisplayPlatform GLOzoneEGLWayland::GetNativeDisplay() {
  if (connection_) {
    return connection_->GetNativeDisplay();
  }
  return gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
}

bool GLOzoneEGLWayland::LoadGLES2Bindings(
    const gl::GLImplementationParts& impl) {
  // TODO: It may not be necessary to set this environment variable when using
  // swiftshader.
  setenv("EGL_PLATFORM", "wayland", 0);
  return LoadDefaultEGLGLES2Bindings(impl);
}

}  // namespace

WaylandSurfaceFactory::WaylandSurfaceFactory(
    WaylandConnection* connection,
    WaylandBufferManagerGpu* buffer_manager)
    : connection_(connection), buffer_manager_(buffer_manager) {
  egl_implementation_ =
      std::make_unique<GLOzoneEGLWayland>(connection_, buffer_manager_);
}

WaylandSurfaceFactory::~WaylandSurfaceFactory() = default;

std::unique_ptr<SurfaceOzoneCanvas>
WaylandSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  return std::make_unique<WaylandCanvasSurface>(buffer_manager_, widget);
}

std::vector<gl::GLImplementationParts>
WaylandSurfaceFactory::GetAllowedGLImplementations() {
  std::vector<gl::GLImplementationParts> impls;
  if (egl_implementation_) {
    // Allow for Angle-vulkan implementation.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    impls.emplace_back(gl::kGLImplementationEGLANGLE);
#endif
    impls.emplace_back(gl::ANGLEImplementation::kOpenGL);
    impls.emplace_back(gl::ANGLEImplementation::kOpenGLES);
    impls.emplace_back(gl::ANGLEImplementation::kSwiftShader);
    impls.emplace_back(gl::ANGLEImplementation::kVulkan);
  }
  return impls;
}

GLOzone* WaylandSurfaceFactory::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
WaylandSurfaceFactory::CreateVulkanImplementation(bool use_swiftshader,
                                                  bool allow_protected_memory) {
  return std::make_unique<VulkanImplementationWayland>(use_swiftshader);
}
#endif

scoped_refptr<gfx::NativePixmap> WaylandSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    std::optional<gfx::Size> framebuffer_size) {
  if (framebuffer_size &&
      !gfx::Rect(size).Contains(gfx::Rect(*framebuffer_size))) {
    return nullptr;
  }
#if defined(WAYLAND_GBM)
  auto* gbm_device = buffer_manager_->GetGbmDevice();
  if (gbm_device && gbm_device->CanCreateBufferForFormat(
                        GetFourCCFormatFromBufferFormat(format))) {
    scoped_refptr<GbmPixmapWayland> pixmap =
        base::MakeRefCounted<GbmPixmapWayland>(buffer_manager_);

    if (!pixmap->InitializeBuffer(widget, size, format, usage,
                                  framebuffer_size)) {
      return nullptr;
    }
    return pixmap;
  }
#endif
  return nullptr;
}

void WaylandSurfaceFactory::CreateNativePixmapAsync(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    NativePixmapCallback callback) {
  // CreateNativePixmap is non-blocking operation. Thus, it is safe to call it
  // and return the result with the provided callback.
  std::move(callback).Run(
      CreateNativePixmap(widget, device_queue, size, format, usage));
}

scoped_refptr<gfx::NativePixmap>
WaylandSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
#if defined(WAYLAND_GBM)
  auto* gbm_device = buffer_manager_->GetGbmDevice();
  if (gbm_device && gbm_device->CanCreateBufferForFormat(
                        GetFourCCFormatFromBufferFormat(format))) {
    scoped_refptr<GbmPixmapWayland> pixmap =
        base::MakeRefCounted<GbmPixmapWayland>(buffer_manager_);
    if (pixmap->InitializeBufferFromHandle(widget, size, format,
                                           std::move(handle))) {
      return pixmap;
    }
  } else {
    scoped_refptr<gfx::NativePixmapDmaBuf> pixmap =
        base::MakeRefCounted<gfx::NativePixmapDmaBuf>(size, format,
                                                      std::move(handle));
    if (pixmap->AreDmaBufFdsValid()) {
      return pixmap;
    }
  }
#endif  //  defined(WAYLAND_GBM)
  return nullptr;
}

bool WaylandSurfaceFactory::SupportsNativePixmaps() const {
  bool supports_native_pixmaps = false;
#if defined(WAYLAND_GBM)
  supports_native_pixmaps = buffer_manager_->GetGbmDevice() != nullptr;
#endif
  // Native pixmaps are not supported with swiftshader.
  if (gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
    supports_native_pixmaps = false;
  }
  return supports_native_pixmaps;
}

std::optional<gfx::BufferFormat>
WaylandSurfaceFactory::GetPreferredFormatForSolidColor() const {
  if (!buffer_manager_->SupportsFormat(gfx::BufferFormat::RGBA_8888)) {
    return gfx::BufferFormat::BGRA_8888;
  }
  return gfx::BufferFormat::RGBA_8888;
}

bool WaylandSurfaceFactory::SupportsDrmModifiersFilter() const {
  return true;
}

void WaylandSurfaceFactory::SetDrmModifiersFilter(
    std::unique_ptr<DrmModifiersFilter> filter) {
  buffer_manager_->set_drm_modifiers_filter(std::move(filter));
}

std::vector<gfx::BufferFormat>
WaylandSurfaceFactory::GetSupportedFormatsForTexturing() const {
#if defined(WAYLAND_GBM)
  GbmDevice* const gbm_device = buffer_manager_->GetGbmDevice();
  if (!gbm_device) {
    return {};
  }

  std::vector<gfx::BufferFormat> supported_buffer_formats;
  for (int j = 0; j <= static_cast<int>(gfx::BufferFormat::LAST); ++j) {
    const gfx::BufferFormat buffer_format = static_cast<gfx::BufferFormat>(j);
    if (gbm_device->CanCreateBufferForFormat(
            GetFourCCFormatFromBufferFormat(buffer_format))) {
      supported_buffer_formats.push_back(buffer_format);
    }
  }
  return supported_buffer_formats;
#else
  return {};
#endif
}

}  // namespace ui
