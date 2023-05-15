// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_WRAPPER_IMPL_H_

#include <xdg-shell-client-protocol.h>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"

namespace ui {

class XDGSurfaceWrapperImpl;
class WaylandConnection;
class WaylandWindow;

// Toplevel wrapper for xdg-shell stable
class XDGToplevelWrapperImpl : public ShellToplevelWrapper {
 public:
  XDGToplevelWrapperImpl(std::unique_ptr<XDGSurfaceWrapperImpl> surface,
                         WaylandWindow* wayland_window,
                         WaylandConnection* connection);
  XDGToplevelWrapperImpl(const XDGToplevelWrapperImpl&) = delete;
  XDGToplevelWrapperImpl& operator=(const XDGToplevelWrapperImpl&) = delete;
  ~XDGToplevelWrapperImpl() override;

  // ShellToplevelWrapper overrides:
  bool Initialize() override;
  bool IsSupportedOnAuraToplevel(uint32_t version) const override;
  void SetMaximized() override;
  void UnSetMaximized() override;
  void SetFullscreen() override;
  void UnSetFullscreen() override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetUseImmersiveMode(bool immersive) override;
  bool SupportsTopLevelImmersiveStatus() const override;
#endif
  void SetMinimized() override;
  void SurfaceMove(WaylandConnection* connection) override;
  void SurfaceResize(WaylandConnection* connection, uint32_t hittest) override;
  void SetTitle(const std::u16string& title) override;
  void AckConfigure(uint32_t serial) override;
  bool IsConfigured() override;
  void SetWindowGeometry(const gfx::Rect& bounds) override;
  void SetMinSize(int32_t width, int32_t height) override;
  void SetMaxSize(int32_t width, int32_t height) override;
  void SetAppId(const std::string& app_id) override;
  void SetDecoration(DecorationMode decoration) override;
  void Lock(WaylandOrientationLockType lock_type) override;
  void Unlock() override;
  void RequestWindowBounds(const gfx::Rect& bounds) override;
  void SetRestoreInfo(int32_t, int32_t) override;
  void SetRestoreInfoWithWindowIdSource(int32_t, const std::string&) override;
  void SetSystemModal(bool modal) override;
  bool SupportsScreenCoordinates() const override;
  void EnableScreenCoordinates() override;
  void SetFloat() override;
  void UnSetFloat() override;
  void SetZOrder(ZOrderLevel z_order) override;
  bool SupportsActivation() override;
  void Activate() override;
  void Deactivate() override;
  void SetScaleFactor(float scale_factor) override;
  void CommitSnap(WaylandWindowSnapDirection snap_direction,
                  float snap_ratio) override;
  void ShowSnapPreview(WaylandWindowSnapDirection snap_direction,
                       bool allow_haptic_feedback) override;
  void SetPersistable(bool persistable) const override;
  void SetShape(std::unique_ptr<ShapeRects> shape_rects) override;

  XDGToplevelWrapperImpl* AsXDGToplevelWrapper() override;

  XDGSurfaceWrapperImpl* xdg_surface_wrapper() const;

 private:
  // xdg_toplevel_listener
  static void ConfigureTopLevel(void* data,
                                struct xdg_toplevel* xdg_toplevel,
                                int32_t width,
                                int32_t height,
                                struct wl_array* states);
  static void CloseTopLevel(void* data, struct xdg_toplevel* xdg_toplevel);
  static void ConfigureBounds(void* data,
                              struct xdg_toplevel* xdg_toplevel,
                              int32_t width,
                              int32_t height);
  static void WmCapabilities(void* data,
                             struct xdg_toplevel* xdg_toplevel,
                             struct wl_array* capabilities);

  // zxdg_decoration_listener
  static void ConfigureDecoration(
      void* data,
      struct zxdg_toplevel_decoration_v1* decoration,
      uint32_t mode);

  // aura_toplevel_listener
  static void ConfigureAuraTopLevel(void* data,
                                    struct zaura_toplevel* zaura_toplevel,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    struct wl_array* states);

  static void OnOriginChange(void* data,
                             struct zaura_toplevel* zaura_toplevel,
                             int32_t x,
                             int32_t y);

  static void ConfigureRasterScale(void* data,
                                   struct zaura_toplevel* zaura_toplevel,
                                   uint32_t scale_as_uint);

  // Send request to wayland compositor to enable a requested decoration mode.
  void SetTopLevelDecorationMode(DecorationMode requested_mode);

  // Initializes the xdg-decoration protocol extension, if available.
  void InitializeXdgDecoration();

  // Called when raster scale is changed.
  void OnConfigureRasterScale(double scale);

  // Creates a wl_region from `shape_rects`.
  wl::Object<wl_region> CreateAndAddRegion(const ShapeRects& shape_rects);

  // Ground surface for this toplevel wrapper.
  std::unique_ptr<XDGSurfaceWrapperImpl> xdg_surface_wrapper_;

  // Non-owing WaylandWindow that uses this toplevel wrapper.
  const raw_ptr<WaylandWindow> wayland_window_;
  const raw_ptr<WaylandConnection> connection_;

  // XDG Shell Stable object.
  wl::Object<xdg_toplevel> xdg_toplevel_;
  // Aura shell toplevel addons.
  wl::Object<zaura_toplevel> aura_toplevel_;

  wl::Object<zxdg_toplevel_decoration_v1> zxdg_toplevel_decoration_;

  // On client side, it keeps track of the decoration mode currently in
  // use if xdg-decoration protocol extension is available.
  DecorationMode decoration_mode_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_WRAPPER_IMPL_H_
