// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_VULKAN_SURFACE_X11_H_
#define UI_OZONE_PLATFORM_X11_VULKAN_SURFACE_X11_H_

#include <vulkan/vulkan.h>
// vulkan.h includes <X11/Xlib.h> when VK_USE_PLATFORM_XLIB_KHR is defined
// after https://github.com/KhronosGroup/Vulkan-Headers/pull/534.
// This defines some macros which break build, so undefine them here.
#undef Above
#undef AllTemporary
#undef AlreadyGrabbed
#undef Always
#undef AsyncBoth
#undef AsyncKeyboard
#undef AsyncPointer
#undef Below
#undef BottomIf
#undef Button1
#undef Button2
#undef Button3
#undef Button4
#undef Button5
#undef ButtonPress
#undef ButtonRelease
#undef ClipByChildren
#undef Complex
#undef Convex
#undef CopyFromParent
#undef CurrentTime
#undef DestroyAll
#undef DirectColor
#undef DisplayString
#undef EnterNotify
#undef GrayScale
#undef IncludeInferiors
#undef InputFocus
#undef InputOnly
#undef InputOutput
#undef KeyPress
#undef KeyRelease
#undef LSBFirst
#undef LeaveNotify
#undef LowerHighest
#undef MSBFirst
#undef Nonconvex
#undef None
#undef NotUseful
#undef Opposite
#undef ParentRelative
#undef PointerRoot
#undef PointerWindow
#undef PseudoColor
#undef RaiseLowest
#undef ReplayKeyboard
#undef ReplayPointer
#undef RetainPermanent
#undef RetainTemporary
#undef StaticColor
#undef StaticGray
#undef Success
#undef SyncBoth
#undef SyncKeyboard
#undef SyncPointer
#undef TopIf
#undef TrueColor
#undef Unsorted
#undef WhenMapped
#undef XYBitmap
#undef XYPixmap
#undef YSorted
#undef YXBanded
#undef YXSorted
#undef ZPixmap

#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace x11 {
class ScopedEventSelector;
}

namespace ui {

class VulkanSurfaceX11 : public gpu::VulkanSurface, public x11::EventObserver {
 public:
  static std::unique_ptr<VulkanSurfaceX11> Create(VkInstance vk_instance,
                                                  x11::Window parent_window);
  VulkanSurfaceX11(VkInstance vk_instance,
                   VkSurfaceKHR vk_surface,
                   x11::Window parent_window,
                   x11::Window window);

  VulkanSurfaceX11(const VulkanSurfaceX11&) = delete;
  VulkanSurfaceX11& operator=(const VulkanSurfaceX11&) = delete;

  ~VulkanSurfaceX11() override;

  // gpu::VulkanSurface:
  void Destroy() override;
  bool Reshape(const gfx::Size& size,
               gfx::OverlayTransform pre_transform) override;

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& xevent) override;

  const x11::Window parent_window_;
  x11::Window window_;
  x11::ScopedEventSelector event_selector_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_VULKAN_SURFACE_X11_H_
