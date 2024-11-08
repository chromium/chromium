// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_
#define UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/native_library.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/drm_modifiers_filter.h"
#include "ui/ozone/public/gl_ozone.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace gfx {
class NativePixmap;
}

namespace gpu {
class VulkanDeviceQueue;
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
  SurfaceFactoryOzone(const SurfaceFactoryOzone&) = delete;
  SurfaceFactoryOzone& operator=(const SurfaceFactoryOzone&) = delete;

  // Returns a list of allowed GL implementations. The default implementation
  // will be the first item.
  virtual std::vector<gl::GLImplementationParts> GetAllowedGLImplementations();

  // Returns the GLOzone to use for the specified GL implementation, or null if
  // GL implementation doesn't exist.
  virtual GLOzone* GetGLOzone(const gl::GLImplementationParts& implementation);

  // Returns the current GLOzone based on the OzonePlatform and
  // GLImplementationParts currently in use.
  GLOzone* GetCurrentGLOzone();

#if BUILDFLAG(ENABLE_VULKAN)
  // Creates the vulkan implementation. This object should be capable of
  // creating surfaces that swap to a platform window.
  // |use_swiftshader| suggests using Swiftshader.  The actual support depends
  // on the platform.
  // |allow_protected_memory| suggests that the vulkan implementation should
  // create protected-capable resources, such as VkQueue.
  virtual std::unique_ptr<gpu::VulkanImplementation> CreateVulkanImplementation(
      bool use_swiftshader,
      bool allow_protected_memory);

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

  // Create SurfaceOzoneCanvas for the specified gfx::AcceleratedWidget.
  //
  // Note: The platform must support creation of SurfaceOzoneCanvas from the
  // Browser Process using only the handle contained in gfx::AcceleratedWidget.
  virtual std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget);

  // Create a single native buffer to be used for overlay planes or zero copy
  // for |widget| representing a particular display controller or default
  // display controller for kNullAcceleratedWidget. |size| corresponds to the
  // dimensions used to allocate the buffer. |framebuffer_size| is used to
  // create a framebuffer for the allocated buffer when the usage requires one.
  // If |framebuffer_size| is not provided, |size| is used instead. In the
  // typical case |framebuffer_size| represents a 'visible size', i.e., a buffer
  // of size |size| may actually contain visible data only in the subregion of
  // size |framebuffer_size|. In more complex cases, it's possible that the
  // buffer has a visible rectangle whose origin is not at (0, 0). In this case,
  // |framebuffer_size| would also include some of the non-visible area. For
  // example, suppose we need to allocate a buffer of size 100x100 for a
  // hardware decoder, but the visible rectangle is (10, 10, 80x80). In this
  // case, |size| would be 100x100 while |framebuffer_size| would be 90x90. If
  // |framebuffer_size| is not contained by |size|, this method returns nullptr.
  // This method can be called on any thread.
  virtual scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gpu::VulkanDeviceQueue* device_queue,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      std::optional<gfx::Size> framebuffer_size = std::nullopt);

  virtual bool CanCreateNativePixmapForFormat(gfx::BufferFormat format);

  // Similar to CreateNativePixmap, but returns the result asynchronously.
  using NativePixmapCallback =
      base::OnceCallback<void(scoped_refptr<gfx::NativePixmap>)>;
  virtual void CreateNativePixmapAsync(gfx::AcceleratedWidget widget,
                                       gpu::VulkanDeviceQueue* device_queue,
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

  // Returns whether the platform supports an external filter on DRM modifiers.
  virtual bool SupportsDrmModifiersFilter() const;

  // Sets the filter that can remove modifiers incompatible with usage elsewhere
  // in Chrome.
  virtual void SetDrmModifiersFilter(
      std::unique_ptr<DrmModifiersFilter> filter);

  // Enumerates the BufferFormats that the platform can allocate (and use for
  // texturing) via CreateNativePixmap(), or returns empty if those could not be
  // retrieved or the platform doesn't know in advance.
  // Enumeration should not be assumed to take a trivial amount of time.
  virtual std::vector<gfx::BufferFormat> GetSupportedFormatsForTexturing()
      const;

  // Enumerates the BufferFormats that the platform can import via
  // CreateNativePixmapFromHandle() to use for GL, or returns empty if those
  // could not be retrieved or the platform doesn't know in advance.
  // Enumeration should not be assumed to take a trivial amount of time.
  std::vector<gfx::BufferFormat> GetSupportedFormatsForGLNativePixmapImport();

  // This returns a preferred format for solid color image on Wayland.
  virtual std::optional<gfx::BufferFormat> GetPreferredFormatForSolidColor()
      const;

 protected:
  SurfaceFactoryOzone();
  virtual ~SurfaceFactoryOzone();
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_
