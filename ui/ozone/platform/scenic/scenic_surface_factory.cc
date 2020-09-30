// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/zx/event.h>
#include <memory>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/angle/src/common/fuchsia_egl/fuchsia_egl.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/scenic/scenic_gpu_service.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_canvas.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "ui/ozone/platform/scenic/vulkan_implementation_scenic.h"
#endif

namespace ui {

namespace {

struct FuchsiaEGLWindowDeleter {
  void operator()(fuchsia_egl_window* egl_window) {
    fuchsia_egl_window_destroy(egl_window);
  }
};

fuchsia::ui::scenic::ScenicPtr ConnectToScenic() {
  fuchsia::ui::scenic::ScenicPtr scenic =
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::ui::scenic::Scenic>();
  scenic.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << "Scenic connection failed";
  });
  return scenic;
}

class GLSurfaceFuchsiaImagePipe : public gl::NativeViewGLSurfaceEGL {
 public:
  explicit GLSurfaceFuchsiaImagePipe(
      ScenicSurfaceFactory* scenic_surface_factory,
      gfx::AcceleratedWidget widget)
      : NativeViewGLSurfaceEGL(0, nullptr),
        scenic_surface_factory_(scenic_surface_factory),
        widget_(widget) {}
  GLSurfaceFuchsiaImagePipe(const GLSurfaceFuchsiaImagePipe&) = delete;
  GLSurfaceFuchsiaImagePipe& operator=(const GLSurfaceFuchsiaImagePipe&) =
      delete;

  // gl::NativeViewGLSurfaceEGL:
  bool InitializeNativeWindow() override {
    fuchsia::images::ImagePipe2Ptr image_pipe;
    ScenicSurface* scenic_surface =
        scenic_surface_factory_->GetSurface(widget_);
    scenic_surface->SetTextureToNewImagePipe(image_pipe.NewRequest());
    egl_window_.reset(
        fuchsia_egl_window_create(image_pipe.Unbind().TakeChannel().release(),
                                  size_.width(), size_.height()));
    window_ = reinterpret_cast<EGLNativeWindowType>(egl_window_.get());
    return true;
  }

  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override {
    fuchsia_egl_window_resize(egl_window_.get(), size.width(), size.height());
    return gl::NativeViewGLSurfaceEGL::Resize(size, scale_factor, color_space,
                                              has_alpha);
  }

 private:
  ~GLSurfaceFuchsiaImagePipe() override {}

  ScenicSurfaceFactory* const scenic_surface_factory_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  std::unique_ptr<fuchsia_egl_window, FuchsiaEGLWindowDeleter> egl_window_;
};

class GLOzoneEGLScenic : public GLOzoneEGL {
 public:
  explicit GLOzoneEGLScenic(ScenicSurfaceFactory* scenic_surface_factory)
      : scenic_surface_factory_(scenic_surface_factory) {}
  ~GLOzoneEGLScenic() override = default;

  // GLOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<GLSurfaceFuchsiaImagePipe>(scenic_surface_factory_,
                                                        window));
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(size));
  }

  gl::EGLDisplayPlatform GetNativeDisplay() override {
    return gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
  }

 protected:
  bool LoadGLES2Bindings(gl::GLImplementation implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  ScenicSurfaceFactory* const scenic_surface_factory_;
  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLScenic);
};

fuchsia::sysmem::AllocatorHandle ConnectSysmemAllocator() {
  fuchsia::sysmem::AllocatorHandle allocator;
  base::ComponentContextForProcess()->svc()->Connect(allocator.NewRequest());
  return allocator;
}

}  // namespace

ScenicSurfaceFactory::ScenicSurfaceFactory()
    : egl_implementation_(std::make_unique<GLOzoneEGLScenic>(this)),
      sysmem_buffer_manager_(this),
      weak_ptr_factory_(this) {}

ScenicSurfaceFactory::~ScenicSurfaceFactory() {
  Shutdown();
}

void ScenicSurfaceFactory::Initialize(
    mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(surface_lock_);
  DCHECK(surface_map_.empty());

  main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(main_thread_task_runner_);

  DCHECK(!gpu_host_);
  gpu_host_.Bind(std::move(gpu_host));

  sysmem_buffer_manager_.Initialize(ConnectSysmemAllocator());

  // Scenic is lazily connected to avoid a dependency in headless mode.
  DCHECK(!scenic_);
}

void ScenicSurfaceFactory::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock lock(surface_lock_);
  DCHECK(surface_map_.empty());
  main_thread_task_runner_ = nullptr;
  gpu_host_.reset();
  sysmem_buffer_manager_.Shutdown();
  scenic_ = nullptr;
}

std::vector<gl::GLImplementation>
ScenicSurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementation>{
      gl::kGLImplementationEGLANGLE,
      gl::kGLImplementationSwiftShaderGL,
      gl::kGLImplementationEGLGLES2,
      gl::kGLImplementationStubGL,
  };
}

GLOzone* ScenicSurfaceFactory::GetGLOzone(gl::GLImplementation implementation) {
  switch (implementation) {
    case gl::kGLImplementationSwiftShaderGL:
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

std::unique_ptr<PlatformWindowSurface>
ScenicSurfaceFactory::CreatePlatformWindowSurface(
    gfx::AcceleratedWidget window) {
  DCHECK_NE(window, gfx::kNullAcceleratedWidget);
  auto surface =
      std::make_unique<ScenicSurface>(this, window, CreateScenicSession());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScenicSurfaceFactory::AttachSurfaceToWindow,
                                weak_ptr_factory_.GetWeakPtr(), window,
                                surface->CreateView()));
  return surface;
}

std::unique_ptr<SurfaceOzoneCanvas> ScenicSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  ScenicSurface* surface = GetSurface(widget);
  return std::make_unique<ScenicWindowCanvas>(surface);
}

scoped_refptr<gfx::NativePixmap> ScenicSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    base::Optional<gfx::Size> framebuffer_size) {
  DCHECK(!framebuffer_size || framebuffer_size == size);
  auto collection = sysmem_buffer_manager_.CreateCollection(vk_device, size,
                                                            format, usage, 1);
  if (!collection)
    return nullptr;

  return collection->CreateNativePixmap(0);
}

void ScenicSurfaceFactory::CreateNativePixmapAsync(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    NativePixmapCallback callback) {
  std::move(callback).Run(
      CreateNativePixmap(widget, vk_device, size, format, usage));
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
ScenicSurfaceFactory::CreateVulkanImplementation(
    bool allow_protected_memory,
    bool enforce_protected_memory) {
  return std::make_unique<ui::VulkanImplementationScenic>(
      this, &sysmem_buffer_manager_, allow_protected_memory,
      enforce_protected_memory);
}
#endif

void ScenicSurfaceFactory::AddSurface(gfx::AcceleratedWidget widget,
                                      ScenicSurface* surface) {
  base::AutoLock lock(surface_lock_);
  DCHECK(!base::Contains(surface_map_, widget));
  surface->AssertBelongsToCurrentThread();
  surface_map_.emplace(widget, surface);
}

void ScenicSurfaceFactory::RemoveSurface(gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_lock_);
  auto it = surface_map_.find(widget);
  DCHECK(it != surface_map_.end());
  ScenicSurface* surface = it->second;
  surface->AssertBelongsToCurrentThread();
  surface_map_.erase(it);
}

ScenicSurface* ScenicSurfaceFactory::GetSurface(gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_lock_);
  auto it = surface_map_.find(widget);
  DCHECK(it != surface_map_.end());
  ScenicSurface* surface = it->second;
  surface->AssertBelongsToCurrentThread();
  return surface;
}

scenic::SessionPtrAndListenerRequest
ScenicSurfaceFactory::CreateScenicSession() {
  fuchsia::ui::scenic::SessionPtr session;
  fidl::InterfaceHandle<fuchsia::ui::scenic::SessionListener> listener_handle;
  auto listener_request = listener_handle.NewRequest();

  {
    // Cache Scenic connection for main thread. For other treads create
    // one-shot connection.
    fuchsia::ui::scenic::ScenicPtr local_scenic;
    fuchsia::ui::scenic::ScenicPtr* scenic =
        main_thread_task_runner_->BelongsToCurrentThread() ? &scenic_
                                                           : &local_scenic;
    if (!*scenic)
      *scenic = ConnectToScenic();
    (*scenic)->CreateSession(session.NewRequest(), std::move(listener_handle));
  }

  return {std::move(session), std::move(listener_request)};
}

void ScenicSurfaceFactory::AttachSurfaceToWindow(
    gfx::AcceleratedWidget window,
    mojo::PlatformHandle surface_view_holder_token_mojo) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu_host_->AttachSurfaceToWindow(window,
                                   std::move(surface_view_holder_token_mojo));
}

}  // namespace ui
