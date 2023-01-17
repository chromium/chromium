// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_surface_factory.h"

#include <memory>

#include "gpu/vulkan/buildflags.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gpu_memory_buffer_support_x11.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/x/connection.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_egl_x11_gles2.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/common/native_pixmap_egl_binding.h"
#include "ui/ozone/platform/x11/gl_surface_egl_readback_x11.h"
#include "ui/ozone/platform/x11/native_pixmap_egl_x11_binding.h"
#include "ui/ozone/platform/x11/x11_canvas_surface.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "ui/ozone/platform/x11/vulkan_implementation_x11.h"
#endif

namespace ui {
namespace {

class GLOzoneEGLX11 : public GLOzoneEGL {
 public:
  GLOzoneEGLX11() = default;

  GLOzoneEGLX11(const GLOzoneEGLX11&) = delete;
  GLOzoneEGLX11& operator=(const GLOzoneEGLX11&) = delete;

  ~GLOzoneEGLX11() override = default;

  // GLOzone:
  bool InitializeStaticGLBindings(
      const gl::GLImplementationParts& implementation) override {
    is_swiftshader_ = gl::IsSoftwareGLImplementation(implementation);
    return GLOzoneEGL::InitializeStaticGLBindings(implementation);
  }

  bool CanImportNativePixmap() override {
    return gl::GLSurfaceEGL::GetGLDisplayEGL()
        ->ext->b_EGL_EXT_image_dma_buf_import;
  }

  std::unique_ptr<NativePixmapGLBinding> ImportNativePixmap(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id) override {
    return NativePixmapEGLBinding::Create(pixmap, plane_format, plane,
                                          plane_size, color_space, target,
                                          texture_id);
  }

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override {
    if (is_swiftshader_) {
      return gl::InitializeGLSurface(
          base::MakeRefCounted<GLSurfaceEglReadbackX11>(
              display->GetAs<gl::GLDisplayEGL>(), window));
    } else {
      switch (gl::GetGLImplementation()) {
        case gl::kGLImplementationEGLGLES2:
          DCHECK(window != gfx::kNullAcceleratedWidget);
          return gl::InitializeGLSurface(new gl::NativeViewGLSurfaceEGLX11GLES2(
              display->GetAs<gl::GLDisplayEGL>(),
              static_cast<x11::Window>(window)));
        case gl::kGLImplementationEGLANGLE:
          DCHECK(window != gfx::kNullAcceleratedWidget);
          return gl::InitializeGLSurface(new gl::NativeViewGLSurfaceEGLX11(
              display->GetAs<gl::GLDisplayEGL>(),
              static_cast<x11::Window>(window)));
        default:
          NOTREACHED();
          return nullptr;
      }
    }
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override {
    gl::GLDisplayEGL* egl_display = display->GetAs<gl::GLDisplayEGL>();
    if (egl_display->IsEGLSurfacelessContextSupported() && size.width() == 0 &&
        size.height() == 0) {
      return InitializeGLSurface(new gl::SurfacelessEGL(egl_display, size));
    } else {
      return InitializeGLSurface(
          new gl::PbufferGLSurfaceEGL(egl_display, size));
    }
  }

 protected:
  // GLOzoneEGL:
  gl::EGLDisplayPlatform GetNativeDisplay() override {
    return gl::EGLDisplayPlatform(reinterpret_cast<EGLNativeDisplayType>(
        x11::Connection::Get()->GetXlibDisplay().display()));
  }

  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  bool is_swiftshader_ = false;
};

}  // namespace

X11SurfaceFactory::X11SurfaceFactory(
    std::unique_ptr<x11::Connection> connection)
    : egl_implementation_(std::make_unique<GLOzoneEGLX11>()),
      connection_(std::move(connection)) {}

X11SurfaceFactory::~X11SurfaceFactory() = default;

std::vector<gl::GLImplementationParts>
X11SurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationEGLANGLE),
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
  };
}

GLOzone* X11SurfaceFactory::GetGLOzone(
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
X11SurfaceFactory::CreateVulkanImplementation(bool use_swiftshader,
                                              bool allow_protected_memory) {
  return std::make_unique<VulkanImplementationX11>(use_swiftshader);
}
#endif

std::unique_ptr<SurfaceOzoneCanvas> X11SurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  // X11SoftwareBitmapPresenter (created via X11CanvasSurface) requres a
  // Connection TLS instance and a PlatformEventSource.
  if (connection_) {
    auto* connection = connection_.get();
    x11::Connection::Set(std::move(connection_));
    connection->platform_event_source =
        std::make_unique<X11EventSource>(connection);
  }
  return std::make_unique<X11CanvasSurface>(widget);
}

scoped_refptr<gfx::NativePixmap> X11SurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    absl::optional<gfx::Size> framebuffer_size) {
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap;
  auto buffer = ui::GpuMemoryBufferSupportX11::GetInstance()->CreateBuffer(
      format, size, usage);
  if (buffer) {
    gfx::NativePixmapHandle handle = buffer->ExportHandle();
    pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(size, format,
                                                           std::move(handle));
  }

  // CreateNativePixmap is non-blocking operation. Thus, it is safe to call it
  // and return the result with the provided callback.
  return pixmap;
}

bool X11SurfaceFactory::CanCreateNativePixmapForFormat(
    gfx::BufferFormat format) {
  return ui::GpuMemoryBufferSupportX11::GetInstance()
      ->CanCreateNativePixmapForFormat(format);
}

void X11SurfaceFactory::CreateNativePixmapAsync(
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
X11SurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap;
  auto buffer =
      ui::GpuMemoryBufferSupportX11::GetInstance()->CreateBufferFromHandle(
          size, format, std::move(handle));
  if (buffer) {
    gfx::NativePixmapHandle buffer_handle = buffer->ExportHandle();
    pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
        size, format, std::move(buffer_handle));
  }
  return pixmap;
}

}  // namespace ui
