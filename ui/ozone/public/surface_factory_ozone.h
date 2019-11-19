// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_
#define UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/gl_ozone.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace gfx {
class NativePixmap;
}

namespace ui {

class SurfaceOzoneCanvas;
class OverlaySurface;
class PlatformWindowSurface;

// The Ozone interface allows external implementations to hook into Chromium to
// provide a system specific implementation. The Ozone interface supports two
// drawing modes: 1) accelerated drawing using GL and 2) software drawing
// through Skia.
//
// If you want to paint on a window with ozone, you need to create a GLSurface
// or SurfaceOzoneCanvas for that window. The platform can support software, GL,
// or both for painting on the window. The following functionality is specific
// to the drawing mode and may not have any meaningful implementation in the
// other mode. An implementation must provide functionality for at least one
// mode.
//
// 1) Accelerated Drawing (GL path):
//
// The following functions are specific to GL:
//  - GetAllowedGLImplementations
//  - GetGLOzone (along with the associated GLOzone)
//
// 2) Software Drawing (Skia):
//
// The following function is specific to the software path:
//  - CreateCanvasForWidget
//
// The accelerated path can optionally provide support for the software drawing
// path.
//
// The remaining functions are not covered since they are needed in both drawing
// modes (See comments below for descriptions).
class COMPONENT_EXPORT(OZONE_BASE) SurfaceFactoryOzone {
 public:
  // Returns a list of allowed GL implementations. The default implementation
  // will be the first item.
  virtual std::vector<gl::GLImplementation> GetAllowedGLImplementations();

  // Returns the GLOzone to use for the specified GL implementation, or null if
  // GL implementation doesn't exist.
  virtual GLOzone* GetGLOzone(gl::GLImplementation implementation);

#if BUILDFLAG(ENABLE_VULKAN)
  // Creates the vulkan implementation. This object should be capable of
  // creating surfaces that swap to a platform window.
  // |allow_protected_memory| suggests that the vulkan implementation should
  // create protected-capable resources, such as VkQueue.
  // |enforce_protected_memory| suggests that the vulkan implementation should
  // always use protected memory and resources, such as CommandBuffers.
  virtual std::unique_ptr<gpu::VulkanImplementation> CreateVulkanImplementation(
      bool allow_protected_memory,
      bool enforce_protected_memory);

  // Creates a scanout NativePixmap that can be rendered using Vulkan.
  // TODO(spang): Remove this once VK_EXT_image_drm_format_modifier is
  // available.
  virtual scoped_refptr<gfx::NativePixmap> CreateNativePixmapForVulkan(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      VkDevice vk_device,
      VkDeviceMemory* vk_device_memory,
      VkImage* vk_image);
#endif

  // Creates a rendering and presentation API agnostic surface for a platform
  // window.
  virtual std::unique_ptr<PlatformWindowSurface> CreatePlatformWindowSurface(
      gfx::AcceleratedWidget window);

  // Creates an overlay surface for a platform window.
  virtual std::unique_ptr<OverlaySurface> CreateOverlaySurface(
      gfx::AcceleratedWidget window);

  // Create SurfaceOzoneCanvas for the specified gfx::AcceleratedWidget. The
  // |task_runner| may be null if the gpu service runs in a host process.
  //
  // Note: The platform must support creation of SurfaceOzoneCanvas from the
  // Browser Process using only the handle contained in gfx::AcceleratedWidget.
  virtual std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget,
      base::TaskRunner* task_runner);

  // Create a single native buffer to be used for overlay planes or zero copy
  // for |widget| representing a particular display controller or default
  // display controller for kNullAcceleratedWidget.
  // It can be called on any thread.
  virtual scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      VkDevice vk_device,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  // Similar to CreateNativePixmap, but returns the result asynchronously.
  using NativePixmapCallback =
      base::OnceCallback<void(scoped_refptr<gfx::NativePixmap>)>;
  virtual void CreateNativePixmapAsync(gfx::AcceleratedWidget widget,
                                       VkDevice vk_device,
                                       gfx::Size size,
                                       gfx::BufferFormat format,
                                       gfx::BufferUsage usage,
                                       NativePixmapCallback callback);

  // Create a single native buffer from an existing handle. Takes ownership of
  // |handle| and can be called on any thread.
  virtual scoped_refptr<gfx::NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::NativePixmapHandle handle);

  // A temporary solution that allows protected NativePixmap management to be
  // handled outside the Ozone platform (crbug.com/771863).
  // The current implementation uses dummy NativePixmaps as transparent handles
  // to separate NativePixmaps with actual contents. This method takes
  // a NativePixmapHandle to such a dummy pixmap, and creates a NativePixmap
  // instance for it.
  virtual scoped_refptr<gfx::NativePixmap>
  CreateNativePixmapForProtectedBufferHandle(gfx::AcceleratedWidget widget,
                                             gfx::Size size,
                                             gfx::BufferFormat format,
                                             gfx::NativePixmapHandle handle);

  // This callback can be used by implementations of this interface to query
  // for a NativePixmap for the given NativePixmapHandle, instead of importing
  // it via standard means. This happens if an external service is maintaining
  // a separate mapping of NativePixmapHandles to NativePixmaps.
  // If this callback returns non-nullptr, the returned NativePixmap should
  // be used instead of the NativePixmap that would have been produced by the
  // standard, implementation-specific NativePixmapHandle import mechanism.
  using GetProtectedNativePixmapCallback =
      base::RepeatingCallback<scoped_refptr<gfx::NativePixmap>(
          const gfx::NativePixmapHandle& handle)>;
  // Called by an external service to set the GetProtectedNativePixmapCallback,
  // to be used by the implementation when importing NativePixmapHandles.
  // TODO(posciak): crbug.com/778555, move this to platform-specific
  // implementation(s) and make protected pixmap handling transparent to the
  // clients of this interface, removing the need for this callback.
  virtual void SetGetProtectedNativePixmapDelegate(
      const GetProtectedNativePixmapCallback&
          get_protected_native_pixmap_callback);

  // Enumerates the BufferFormats that the platform can allocate (and use for
  // texturing) via CreateNativePixmap(), or returns empty if those could not be
  // retrieved or the platform doesn't know in advance.
  // Enumeration should not be assumed to take a trivial amount of time.
  virtual std::vector<gfx::BufferFormat> GetSupportedFormatsForTexturing()
      const;

 protected:
  SurfaceFactoryOzone();
  virtual ~SurfaceFactoryOzone();

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceFactoryOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_
