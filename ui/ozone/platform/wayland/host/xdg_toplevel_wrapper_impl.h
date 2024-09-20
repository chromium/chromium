// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_WRAPPER_IMPL_H_

#include <xdg-shell-client-protocol.h>
#include <memory>

#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

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
  void SetCanMaximize(bool can_maximize) override;
  void SetMaximized() override;
  void UnSetMaximized() override;
  void SetCanFullscreen(bool can_fullscreen) override;
  void SetFullscreen(WaylandOutput* wayland_output) override;
  void UnSetFullscreen() override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetUseImmersiveMode(bool immersive) override;
  void SetTopInset(int height) override;
  void SetShadowCornersRadii(const gfx::RoundedCornersF& radii) override;
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
  void RequestWindowBounds(const gfx::Rect& bounds,
                           int64_t display_id) override;
  void SetRestoreInfo(int32_t, int32_t) override;
  void SetRestoreInfoWithWindowIdSource(int32_t, const std::string&) override;
  void SetSystemModal(bool modal) override;
  bool SupportsScreenCoordinates() const override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void EnableScreenCoordinates() override;
#endif
  void SetFloatToLocation(
      WaylandFloatStartLocation float_start_location) override;
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
  void AckRotateFocus(uint32_t serial, uint32_t handled) override;
  void SetIcon(const gfx::ImageSkia& icon) override;

  XDGToplevelWrapperImpl* AsXDGToplevelWrapper() override;

  XDGSurfaceWrapperImpl* xdg_surface_wrapper() const;

 private:
  friend class WaylandWindowDragController;
  // xdg_toplevel_listener callbacks:
  static void OnToplevelConfigure(void* data,
                                  xdg_toplevel* toplevel,
                                  int32_t width,
                                  int32_t height,
                                  wl_array* states);
  static void OnToplevelClose(void* data, xdg_toplevel* toplevel);
  static void OnConfigureBounds(void* data,
                                xdg_toplevel* toplevel,
                                int32_t width,
                                int32_t height);
  static void OnWmCapabilities(void* data,
                               xdg_toplevel* toplevel,
                               wl_array* capabilities);

  // zxdg_decoration_listener callbacks:
  static void OnDecorationConfigure(void* data,
                                    zxdg_toplevel_decoration_v1* decoration,
                                    uint32_t mode);

  // zaura_toplevel_listener callbacks:
  static void OnAuraToplevelConfigure(void* data,
                                      zaura_toplevel* aura_toplevel,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height,
                                      wl_array* states);
  static void OnOriginChange(void* data,
                             zaura_toplevel* aura_toplevel,
                             int32_t x,
                             int32_t y);

  static void OnConfigureRasterScale(void* data,
                                     struct zaura_toplevel* zaura_toplevel,
                                     uint32_t scale_as_uint);

  static void OnConfigureOcclusionState(void* data,
                                        struct zaura_toplevel* zaura_toplevel,
                                        uint32_t mode);
  static void OnRotateFocus(void* data,
                            zaura_toplevel* aura_toplevel,
                            uint32_t serial,
                            uint32_t direction,
                            uint32_t restart);
  static void OnOverviewChange(void* data,
                               zaura_toplevel* aura_toplevel,
                               uint32_t in_overview_as_uint);

  // Send request to wayland compositor to enable a requested decoration mode.
  void SetTopLevelDecorationMode(DecorationMode requested_mode);

  // Initializes the xdg-decoration protocol extension, if available.
  void InitializeXdgDecoration();

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
