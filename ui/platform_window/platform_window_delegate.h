// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_

#include <string>

#include "base/component_export.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "ui/gfx/geometry/insets.h"
#endif  // BUILDFLAG(IS_FUCHSIA)

namespace gfx {
class Rect;
class Size;
class PointF;
}  // namespace gfx

class SkPath;

namespace ui {

class Event;
struct OwnedWindowAnchor;

enum class PlatformWindowState {
  kUnknown,
  kMaximized,
  kMinimized,
  kNormal,
  kFullScreen,

  // Currently, only used by ChromeOS.
  kSnappedPrimary,
  kSnappedSecondary,
  kFloated,
};

enum class PlatformWindowOcclusionState {
  kUnknown,
  kVisible,
  kOccluded,
  kHidden,
};

enum class PlatformWindowTooltipTrigger {
  kCursor,
  kKeyboard,
};

class COMPONENT_EXPORT(PLATFORM_WINDOW) PlatformWindowDelegate {
 public:
  struct COMPONENT_EXPORT(PLATFORM_WINDOW) BoundsChange {
    BoundsChange() = delete;
    constexpr BoundsChange(bool origin_changed)
        : origin_changed(origin_changed) {}
    ~BoundsChange() = default;

    // True if the bounds change resulted in the origin change.
    bool origin_changed : 1;

#if BUILDFLAG(IS_FUCHSIA)
    // The widths of border regions which are obscured by overlapping
    // platform UI elements like onscreen keyboards.
    //
    // As an example, the overlap from an onscreen keyboard covering
    // the bottom of the Window would be represented like this:
    //
    // +------------------------+                ---
    // |                        |                 |
    // |        content         |                 |
    // |                        |                 | window
    // +------------------------+  ---            |
    // |    onscreen keyboard   |   |  overlap    |
    // +------------------------+  ---           ---
    gfx::Insets system_ui_overlap;
#endif  // BUILDFLAG(IS_FUCHSIA)
  };

  // State describes important data about this window, for example data that
  // needs to be synchronized and acked. We apply this state to the client
  // (us) and wait for a frame to be produced matching this state. That frame
  // is identified by the sequence id.
  // This is used by OnStateChanged and currently only by ozone/wayland.
  struct COMPONENT_EXPORT(PLATFORM_WINDOW) State {
    bool operator==(const State& rhs) const {
      return std::tie(bounds_dip, size_px, window_scale, raster_scale) ==
             std::tie(rhs.bounds_dip, rhs.size_px, rhs.window_scale,
                      rhs.raster_scale);
    }

    // Bounds in DIP.
    gfx::Rect bounds_dip;
    // Size in pixels. Note that it's required to keep information in both DIP
    // and pixels since it is not always possible to convert between them.
    gfx::Size size_px;
    // Current scale factor of the output where the window is located at.
    float window_scale = 1.0;
    // TODO(crbug.com/1395267): Add window states here.

    // Scale to raster the window at.
    float raster_scale = 1.0;

    // Returns true if updating from the given State |old| to this state
    // should produce a frame.
    bool ProducesFrameOnUpdateFrom(const State& old) const;

    std::string ToString() const;
  };

  PlatformWindowDelegate();
  virtual ~PlatformWindowDelegate();

  virtual void OnBoundsChanged(const BoundsChange& change) = 0;

  // Note that |damaged_region| is in the platform-window's coordinates, in
  // physical pixels.
  virtual void OnDamageRect(const gfx::Rect& damaged_region) = 0;

  virtual void DispatchEvent(Event* event) = 0;

  virtual void OnCloseRequest() = 0;
  virtual void OnClosed() = 0;

  virtual void OnWindowStateChanged(PlatformWindowState old_state,
                                    PlatformWindowState new_state) = 0;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Notifies the delegate that the tiled state of the window edges has changed.
  virtual void OnWindowTiledStateChanged(WindowTiledEdges new_tiled_edges);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(ffred): We should just add kImmersiveFullscreen as a state. However,
  // that will require more refactoring in other places to understand that
  // kImmersiveFullscreen is a fullscreen status.
  // Sets the immersive mode for the window. This will only have an effect on
  // ChromeOS platforms.
  virtual void OnImmersiveModeChanged(bool immersive) {}

  // Lets the window know that ChromeOS overview mode has changed.
  virtual void OnOverviewModeChanged(bool in_overview) {}
#endif

  enum RotateDirection {
    kForward,
    kBackward,
  };
  // Rotates the focus within the window. The method will return true if there
  // are more views left after rotation and false otherwise. Reset will restart
  // the focus and focus on the first view for the given direction.
  virtual bool OnRotateFocus(RotateDirection direction, bool reset);

  virtual void OnLostCapture() = 0;

  virtual void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) = 0;

  // Notifies the delegate that the widget is about to be destroyed.
  virtual void OnWillDestroyAcceleratedWidget() = 0;

  // Notifies the delegate that the widget cannot be used anymore until
  // a new widget is made available through OnAcceleratedWidgetAvailable().
  // Must not be called when the PlatformWindow is being destroyed.
  virtual void OnAcceleratedWidgetDestroyed() = 0;

  virtual void OnActivationChanged(bool active) = 0;

  // Requests size constraints for the PlatformWindow in DIP.
  virtual absl::optional<gfx::Size> GetMinimumSizeForWindow();
  virtual absl::optional<gfx::Size> GetMaximumSizeForWindow();

  // Returns a mask to be used to clip the window for the size of
  // |WindowTreeHost::GetBoundsInPixels|.
  // This is used to create the non-rectangular window shape.
  virtual SkPath GetWindowMaskForWindowShapeInPixels();

  // Called while dragging maximized window when SurfaceFrame associated with
  // this window is locked to normal state or unlocked from previously locked
  // state. This function is used by chromeos for syncing
  // `chromeos::kFrameRestoreLookKey` window property
  // with lacros-chrome.
  virtual void OnSurfaceFrameLockingChanged(bool lock);

  // Returns a menu type of the window. Valid only for the menu windows.
  virtual absl::optional<MenuType> GetMenuType();

  // Called when the location of mouse pointer entered the window.  This is
  // different from ui::ET_MOUSE_ENTERED which may not be generated when mouse
  // is captured either by implicitly or explicitly.
  virtual void OnMouseEnter() = 0;

  // Called when the occlusion state changes, if the underlying platform
  // is providing us with occlusion information.
  virtual void OnOcclusionStateChanged(
      PlatformWindowOcclusionState occlusion_state);

  // Updates state for clients that need sequence point synchronized
  // PlatformWindowDelegate::State operations. In particular, this requests a
  // new LocalSurfaceId for the window tree of this platform window. It returns
  // the new parent ID. Calling code can compare this value with the
  // gfx::FrameData::seq value to see when viz has produced a frame at or after
  // the (conceptually) inserted sequence point. OnStateUpdate may return -1 if
  // the state update does not require a new frame to be considered
  // synchronized. For example, this can happen if the old and new states are
  // the same, or it only changes the origin of the bounds.
  virtual int64_t OnStateUpdate(const State& old, const State& latest);

  // Returns optional information for owned windows that require anchor for
  // positioning. Useful for such backends as Wayland as it provides flexibility
  // in positioning child windows, which must be repositioned if the originally
  // intended position caused the surface to be constrained.
  virtual absl::optional<OwnedWindowAnchor> GetOwnedWindowAnchorAndRectInDIP();

  // Enables or disables frame rate throttling.
  virtual void SetFrameRateThrottleEnabled(bool enabled);

  // Called when tooltip is shown on server.
  // `bounds` is in screen coordinates.
  virtual void OnTooltipShownOnServer(const std::u16string& text,
                                      const gfx::Rect& bounds);

  // Called when tooltip is hidden on server.
  virtual void OnTooltipHiddenOnServer();

  // Convert gfx::Rect in pixels to DIP in screen, and vice versa.
  virtual gfx::Rect ConvertRectToPixels(const gfx::Rect& rect_in_dp) const;
  virtual gfx::Rect ConvertRectToDIP(const gfx::Rect& rect_in_pixells) const;

  // Convert gfx::Point in screen pixels to dip in the window's local
  // coordinate.
  virtual gfx::PointF ConvertScreenPointToLocalDIP(
      const gfx::Point& screen_in_pixels) const;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_DELEGATE_H_
