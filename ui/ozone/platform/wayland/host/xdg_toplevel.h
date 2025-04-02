// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_H_

#include <xdg-shell-client-protocol.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/xdg_surface.h"

namespace ui {

class WaylandConnection;
class WaylandOutput;
class WaylandWindow;

// Wrapper class for xdg-shell's xdg_toplevel protocol objects. Used by
// WaylandToplevelWindow to set window-like properties such as maximize,
// fullscreen, and minimize, set application-specific metadata like title and
// id, as well as trigger user interactive operations such as interactive resize
// and move.
class XdgToplevel {
 public:
  using ShapeRects = std::vector<gfx::Rect>;
  enum class DecorationMode { kNone, kClientSide, kServerSide };

  explicit XdgToplevel(std::unique_ptr<XdgSurface> xdg_surface);
  XdgToplevel(const XdgToplevel&) = delete;
  XdgToplevel& operator=(const XdgToplevel&) = delete;
  ~XdgToplevel();

  bool Initialize();
  void SetMaximized();
  void UnSetMaximized();
  void SetFullscreen(WaylandOutput* wayland_output);
  void UnSetFullscreen();
  void SetMinimized();
  void SurfaceMove(WaylandConnection* connection);
  void SurfaceResize(WaylandConnection* connection, uint32_t hittest);
  void SetTitle(const std::u16string& title);
  void AckConfigure(uint32_t serial);
  bool IsConfigured();
  void SetWindowGeometry(const gfx::Rect& bounds);
  void SetMinSize(int32_t width, int32_t height);
  void SetMaxSize(int32_t width, int32_t height);
  void SetAppId(const std::string& app_id);
  void ShowWindowMenu(WaylandConnection* connection, const gfx::Point& point);
  void SetDecoration(DecorationMode decoration);
  void SetSystemModal(bool modal);
  void SetIcon(const gfx::ImageSkia& icon);

  struct xdg_surface* xdg_surface() const { return xdg_surface_->wl_object(); }
  struct xdg_toplevel* wl_object() const { return xdg_toplevel_.get(); }

 private:
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

  // Send request to wayland compositor to enable a requested decoration mode.
  void SetTopLevelDecorationMode(DecorationMode requested_mode);

  // Initializes the xdg-decoration protocol extension, if available.
  void InitializeXdgDecoration();

  // Creates a wl_region from `shape_rects`.
  wl::Object<wl_region> CreateAndAddRegion(const ShapeRects& shape_rects);

  WaylandConnection* connection() const { return xdg_surface_->connection_; }
  WaylandWindow* window() const { return xdg_surface_->wayland_window_; }

  // Ground surface for this toplevel wrapper.
  const std::unique_ptr<XdgSurface> xdg_surface_;

  // XDG Shell Stable object.
  wl::Object<xdg_toplevel> xdg_toplevel_;

  wl::Object<zxdg_toplevel_decoration_v1> zxdg_toplevel_decoration_;

  // On client side, it keeps track of the decoration mode currently in
  // use if xdg-decoration protocol extension is available.
  DecorationMode decoration_mode_ = DecorationMode::kNone;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_TOPLEVEL_H_
