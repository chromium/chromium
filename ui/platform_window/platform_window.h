// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "ui/base/class_property.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_delegate.h"

template <class T>
class scoped_refptr;

namespace gfx {
class ImageSkia;
class Point;
class Rect;
class SizeF;
class Transform;
}  // namespace gfx

namespace ui {
class PlatformCursor;

// Generic PlatformWindow interface.
class COMPONENT_EXPORT(PLATFORM_WINDOW) PlatformWindow
    : public PropertyHandler {
 public:
  PlatformWindow();
  ~PlatformWindow() override;

  // PlatformWindow may be called with the |inactive| set to true in some cases.
  // That means that the Window Manager must not activate the window when it is
  // shown.  Most of PlatformWindow may ignore this value if not supported.
  virtual void Show(bool inactive = false) = 0;
  virtual void Hide() = 0;
  virtual void Close() = 0;

  virtual bool IsVisible() const = 0;

  // Informs the window it is going to be destroyed sometime soon. This is only
  // called for specific code paths, for example by Ash, so it shouldn't be
  // assumed this will get called before destruction.
  virtual void PrepareForShutdown() = 0;

  // Sets and gets the bounds of the platform-window. Note that the bounds is in
  // physical pixel coordinates. The implementation should use
  // `PlatformWindowDelegate::ConvertRectToPixels|DIP` if conversion is
  // necessary.
  virtual void SetBoundsInPixels(const gfx::Rect& bounds) = 0;
  virtual gfx::Rect GetBoundsInPixels() const = 0;

  // Sets and gets the bounds of the platform-window. Note that the bounds is in
  // device-independent-pixel (dip) coordinates. The implementation should use
  // `PlatformWindowDelegate::ConvertRectToPixels|DIP` if conversion is
  // necessary.
  virtual void SetBoundsInDIP(const gfx::Rect& bounds) = 0;
  virtual gfx::Rect GetBoundsInDIP() const = 0;

  virtual void SetTitle(const std::u16string& title) = 0;

  virtual void SetCapture() = 0;
  virtual void ReleaseCapture() = 0;
  virtual bool HasCapture() const = 0;

  // Sets and releases video capture state for the platform-window.
  virtual void SetVideoCapture();
  // NOTE: This may not be called if the platform-window is deleted while
  // video capture is still active.
  virtual void ReleaseVideoCapture();

  // Enters or exits fullscreen when `fullscreen` is true or false respectively.
  // This operation may have no effect if the window is already in the specified
  // state. `target_display_id` indicates the display where the window should be
  // shown fullscreen when entering into fullscreen; display::kInvalidDisplayId
  // indicates that no display was specified, so the current display may be
  // used.
  virtual void SetFullscreen(bool fullscreen, int64_t target_display_id) = 0;
  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;
  virtual PlatformWindowState GetPlatformWindowState() const = 0;

  virtual void Activate() = 0;
  virtual void Deactivate() = 0;

  // Sets whether the window should have the standard title bar provided by the
  // underlying windowing system.  For the main browser window, this may be
  // changed by the user at any time via 'Show system title bar' option in the
  // tab strip menu.
  virtual void SetUseNativeFrame(bool use_native_frame) = 0;
  virtual bool ShouldUseNativeFrame() const = 0;

  // This method sets the current cursor to `cursor`. Note that the platform
  // window should keep a copy of `cursor` and also avoid replacing it until the
  // new value has been set if any kind of platform-specific resources are
  // managed by the platform cursor, e.g. HCURSOR on Windows, which are
  // destroyed once the last copy of the platform cursor goes out of scope.
  virtual void SetCursor(scoped_refptr<PlatformCursor> cursor) = 0;

  // Moves the cursor to |location|. Location is in platform window coordinates.
  virtual void MoveCursorTo(const gfx::Point& location) = 0;

  // Confines the cursor to |bounds| when it is in the platform window. |bounds|
  // is in platform window coordinates.
  virtual void ConfineCursorToBounds(const gfx::Rect& bounds) = 0;

  // Sets and gets the restored bounds of the platform-window.
  virtual void SetRestoredBoundsInDIP(const gfx::Rect& bounds) = 0;
  virtual gfx::Rect GetRestoredBoundsInDIP() const = 0;

  // Sets the Window icons. |window_icon| is a 16x16 icon suitable for use in
  // a title bar. |app_icon| is a larger size for use in the host environment
  // app switching UI.
  virtual void SetWindowIcons(const gfx::ImageSkia& window_icon,
                              const gfx::ImageSkia& app_icon) = 0;

  // Notifies that size constraints of the host have been changed and the
  // PlatformWindow must react on them accordingly.
  virtual void SizeConstraintsChanged() = 0;

  // Tells if the content of the platform window should be transparent. By
  // default returns false.
  virtual bool ShouldWindowContentsBeTransparent() const;

  // Sets and gets ZOrderLevel of the PlatformWindow. Such platforms that do not
  // support ordering, should not implement these methods as the default
  // implementation always returns ZOrderLevel::kNormal value.
  virtual void SetZOrderLevel(ZOrderLevel order);
  virtual ZOrderLevel GetZOrderLevel() const;

  // Asks the PlatformWindow to stack itself on top of |widget|.
  virtual void StackAbove(gfx::AcceleratedWidget widget);
  virtual void StackAtTop();

  // Flashes the frame of the window to draw attention to it. If |flash_frame|
  // is set, the PlatformWindow must draw attention to it. If |flash_frame| is
  // not set, flashing must be stopped.
  virtual void FlashFrame(bool flash_frame);

  using ShapeRects = std::vector<gfx::Rect>;
  // Sets shape of the PlatformWindow. ShapeRects corresponds to the
  // Widget::ShapeRects that is a vector of gfx::Rects that describe the shape.
  virtual void SetShape(std::unique_ptr<ShapeRects> native_shape,
                        const gfx::Transform& transform);

  // Sets the aspect ratio of the Platform Window, which will be
  // maintained during interactive resizing. This size disregards title bar and
  // borders. Once set, some platforms ensure the content will only size to
  // integer multiples of |aspect_ratio|.
  virtual void SetAspectRatio(const gfx::SizeF& aspect_ratio);

  // Returns true if the window was closed but is still showing because of
  // animations.
  virtual bool IsAnimatingClosed() const;

  // Sets opacity of the platform window.
  virtual void SetOpacity(float opacity);

  // Enables or disables platform provided animations of the PlatformWindow.
  // If |enabled| is set to false, animations are disabled.
  virtual void SetVisibilityChangedAnimationsEnabled(bool enabled);

  // Returns a unique ID for the window. The interpretation of the ID is
  // platform specific. Overriding this method is optional.
  virtual std::string GetWindowUniqueId() const;

  // Returns true if window shape should be updated in host,
  // otherwise false when platform window or specific frame views updates the
  // window shape.
  virtual bool ShouldUpdateWindowShape() const;

  // Returns true if the WM supports setting the frame extents for client side
  // decorations.  This typically requires a compositor and an extension for
  // specifying the decoration insets.
  virtual bool CanSetDecorationInsets() const;

  // Sets a hint for the compositor so it can avoid unnecessarily redrawing
  // occluded portions of windows.  If |region_px| is nullopt or empty, then any
  // existing region will be reset.
  virtual void SetOpaqueRegion(std::optional<std::vector<gfx::Rect>> region_px);

  // Sets the clickable region of a window.  This is useful for trimming down a
  // potentially large (24px) hit area for window resizing on the window shadow
  // to a more reasonable (10px) area.  If |region_px| is nullopt, then any
  // existing region will be reset.
  virtual void SetInputRegion(std::optional<std::vector<gfx::Rect>> region_px);

  // Whether the platform supports client-controlled window movement. Under
  // Wayland, for example, this returns false, unless the required protocol
  // extension is supported by the compositor.
  virtual bool IsClientControlledWindowMovementSupported() const;

  // Notifies the DE that the app is done loading, so that it can dismiss any
  // loading animations.
  virtual void NotifyStartupComplete(const std::string& startup_id);

  // Shows tooltip with this platform window as a parent window.
  // `position` is relative to this platform window.
  // `show_delay` and `hide_delay` specify the delay before showing or hiding
  // tooltip on server side. `show_delay` may be set to zero only for testing.
  // If `hide_delay` is zero, the tooltip will not be hidden by timer on server
  // side.
  virtual void ShowTooltip(const std::u16string& text,
                           const gfx::Point& position,
                           const PlatformWindowTooltipTrigger trigger,
                           const base::TimeDelta show_delay,
                           const base::TimeDelta hide_delay) {}

  // Hides tooltip.
  virtual void HideTooltip() {}
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_H_
