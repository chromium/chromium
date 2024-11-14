// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SURFACE_H_

#include <wayland-util.h>

#include <components/exo/wayland/protocol/aura-shell-client-protocol.h>

#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {

class WaylandConnection;

// Wrapper class for an instance of the zaura_surface client-side wayland
// object.
class WaylandZAuraSurface {
 public:
  WaylandZAuraSurface(zaura_shell* zaura_shell,
                      wl_surface* wl_surface,
                      WaylandConnection* connection);
  WaylandZAuraSurface(const WaylandZAuraSurface&) = delete;
  WaylandZAuraSurface& operator=(const WaylandZAuraSurface&) = delete;
  ~WaylandZAuraSurface();

  // The following methods return true if the bound zaura_surface version
  // supports the request and it was successfully sent to the server.
  bool SetFrame(zaura_surface_frame_type type);
  bool Activate();
  bool SetFullscreenMode(zaura_surface_fullscreen_mode mode);
  bool SetServerStartResize();
  bool SetCanGoBack();
  bool UnsetCanGoBack();
  bool SetPip();
  bool SetAspectRatio(float width, float height);
  bool SetInitialWorkspace(int workspace);

  // Helpers to check whether a given request is supported by the currently
  // bound version.
  bool SupportsActivate() const;
  bool SupportsSetServerStartResize() const;

  zaura_surface* wl_object() { return zaura_surface_.get(); }

 private:
  // Returns true if `zaura_surface_` version is equal or newer than `version`.
  bool IsSupported(uint32_t version) const;

  // The client-side resource handle for the zaura_shell object.
  wl::Object<zaura_surface> zaura_surface_;

  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SURFACE_H_
