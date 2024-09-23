// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_

#include <string>

#include "ui/display/types/display_constants.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/platform_window/extensions/wayland_extension.h"

namespace gfx {
class ImageSkia;
class Rect;
class RoundedCornersF;
}

namespace ui {

class WaylandConnection;
class WaylandOutput;
class XDGToplevelWrapperImpl;
enum class ZOrderLevel;

// Wrapper interface for shell top level windows.
//
// This is one of three wrapper classes: Shell{Surface,Toplevel,Popup}Wrapper.
// It has the only sub-class in Chromium, but should not be removed because it
// eases downstream implementations.
// See https://crbug.com/1402672
//
// Allows WaylandToplevelWindow to set window-like properties such as maximize,
// fullscreen, and minimize, set application-specific metadata like title and
// id, as well as trigger user interactive operations such as interactive resize
// and move.
class ShellToplevelWrapper {
 public:
  using ShapeRects = std::vector<gfx::Rect>;

  enum class DecorationMode {
    // Initial mode that the surface has till the first configure event.
    kNone,
    // Client-side decoration for a window.
    // In this case, the client is responsible for drawing decorations
    // for a window (e.g. caption bar, close button). This is suitable for
    // windows using custom frame.
    kClientSide,
    // Server-side decoration for a window.
    // In this case, the ash window manager is responsible for drawing
    // decorations. This is suitable for windows using native frame.
    // e.g. taskmanager.
    kServerSide
  };

  virtual ~ShellToplevelWrapper() = default;

  // Initializes the ShellToplevel.
  virtual bool Initialize() = 0;

  // Returns true if `aura_toplevel_` version is equal or newer than `version`.
  virtual bool IsSupportedOnAuraToplevel(uint32_t version) const = 0;

  // Sets whether the window can be maximized.
  virtual void SetCanMaximize(bool can_maximize) = 0;

  // Sets a native window to maximized state.
  virtual void SetMaximized() = 0;

  // Unsets a native window from maximized state.
  virtual void UnSetMaximized() = 0;

  // Sets whether the window can enter fullscreen.
  virtual void SetCanFullscreen(bool can_fullscreen) = 0;

  // Sets a native window to fullscreen state. If the `wayland_output` is a
  // `nullptr`, the current output will be used, otherwise the requested one.
  virtual void SetFullscreen(WaylandOutput* wayland_output) = 0;

  // Unsets a native window from fullscreen state.
  virtual void UnSetFullscreen() = 0;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Sets a native window's immersive mode.
  virtual void SetUseImmersiveMode(bool immersive) = 0;

  // Sets the top inset (header) height which is reserved or occupied by the top
  // window frame.
  virtual void SetTopInset(int height) = 0;

  // Sets the radius of each corner of the drop shadow associated with the
  // window.
  virtual void SetShadowCornersRadii(const gfx::RoundedCornersF& radii) = 0;

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Sets a native window to minimized state.
  virtual void SetMinimized() = 0;

  // Tells wayland to start interactive window drag.
  virtual void SurfaceMove(WaylandConnection* connection) = 0;

  // Tells wayland to start interactive window resize.
  virtual void SurfaceResize(WaylandConnection* connection,
                             uint32_t hittest) = 0;

  // Sets a title of a native window.
  virtual void SetTitle(const std::u16string& title) = 0;

  // Sends acknowledge configure event back to wayland.
  virtual void AckConfigure(uint32_t serial) = 0;

  // Tells if the surface has been AckConfigured at least once.
  virtual bool IsConfigured() = 0;

  // Sets a desired window geometry in surface local coordinates that specifies
  // the content area of the surface.
  virtual void SetWindowGeometry(const gfx::Rect& bounds) = 0;

  // Requests a desired window position and size in global screen coordinates,
  // with a hint in which display the window should be placed.  The compositor
  // may or may not filfill the request.
  virtual void RequestWindowBounds(
      const gfx::Rect& bounds,
      int64_t display_id = display::kInvalidDisplayId) = 0;

  // Sets the minimum size for the top level.
  virtual void SetMinSize(int32_t width, int32_t height) = 0;

  // Sets the maximum size for the top level.
  virtual void SetMaxSize(int32_t width, int32_t height) = 0;

  // Sets an app id of the native window that is shown as an application name
  // and hints the compositor that it can group application surfaces together by
  // their app id. This also helps the compositor to identify application's
  // .desktop file and use the icon set there.
  virtual void SetAppId(const std::string& app_id) = 0;

  // In case of kClientSide or kServerSide, this function sends a request to the
  // wayland compositor to update the decoration mode for a surface associated
  // with this top level window.
  virtual void SetDecoration(DecorationMode decoration) = 0;

  // Set session id and restore id for the top level.
  virtual void SetRestoreInfo(int32_t restore_session_id,
                              int32_t restore_window_id) = 0;

  virtual void SetRestoreInfoWithWindowIdSource(
      int32_t restore_session_id,
      const std::string& restore_window_id_source) = 0;

  // Request that the server set the orientation lock to the provided lock type.
  // This is only accepted if the requesting window is running in immersive
  // fullscreen mode and in a tablet configuration.
  virtual void Lock(WaylandOrientationLockType lock_type) = 0;

  // Request that the server remove the applied orientation lock.
  virtual void Unlock() = 0;

  // Request that the window be made a system modal.
  virtual void SetSystemModal(bool modal) = 0;

  // Checks if the server supports chrome to control the window position in
  // screen coordinates.
  virtual bool SupportsScreenCoordinates() const = 0;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Enables screen coordinates support. This is no-op if the server does not
  // support the screen coordinates.
  virtual void EnableScreenCoordinates() = 0;
#endif

  // Sets/usets a native window to float state. This places it on top of other
  // windows.
  virtual void SetFloatToLocation(
      WaylandFloatStartLocation float_start_location) = 0;
  virtual void UnSetFloat() = 0;

  // Sets the z order of the window.
  virtual void SetZOrder(ZOrderLevel z_order) = 0;

  // Activation brings a window to the foreground. Deactivation makes a window
  // non-foregrounded.
  virtual bool SupportsActivation() = 0;
  virtual void Activate() = 0;
  virtual void Deactivate() = 0;

  // Sets the scale factor for the next commit. Scale factor persists until a
  // new one is set.
  virtual void SetScaleFactor(float scale_factor) = 0;

  // Snaps the window in the direction of `snap_direction`. `snap_ratio`
  // indicates the width of the work area to snap to in landscape mode, or
  // height in portrait mode.
  virtual void CommitSnap(WaylandWindowSnapDirection snap_direction,
                          float snap_ratio) = 0;

  // Signals the underneath platform to shows a preview for the given window
  // snap direction. `allow_haptic_feedback` indicates if it should send haptic
  // feedback.
  virtual void ShowSnapPreview(WaylandWindowSnapDirection snap_direction,
                               bool allow_haptic_feedback) = 0;

  // Sets the persistable window property.
  virtual void SetPersistable(bool persistable) const = 0;

  // Sets the shape of the toplevel window. If `shape_rects` is null this will
  // unset the window shape.
  virtual void SetShape(std::unique_ptr<ShapeRects> shape_rects) = 0;

  virtual void AckRotateFocus(uint32_t serial, uint32_t handled) = 0;

  virtual void SetIcon(const gfx::ImageSkia& icon) = 0;

  // Casts `this` to XDGToplevelWrapperImpl, if it is of that type.
  virtual XDGToplevelWrapperImpl* AsXDGToplevelWrapper();
};

// Look for `value` in `wl_array` in C++ style.
bool CheckIfWlArrayHasValue(struct wl_array* wl_array, uint32_t value);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_
