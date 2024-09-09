// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_WRAPPER_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_WRAPPER_H_

#include "ui/base/cursor/cursor.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace views {

class NativeViewHost;

// An interface that implemented by an object that wraps a gfx::NativeView on
// a specific platform, used to perform platform specific operations on that
// native view when attached, detached, moved and sized.
class NativeViewHostWrapper {
 public:
  virtual ~NativeViewHostWrapper() = default;

  // Called at the end of NativeViewHost::Attach, allowing the wrapper to
  // perform platform-specific operations that need to occur to complete
  // attaching the gfx::NativeView.
  virtual void AttachNativeView() = 0;

  // Called before the attached gfx::NativeView is detached from the
  // NativeViewHost, allowing the wrapper to perform platform-specific
  // cleanup. |destroyed| is true if the native view is detached
  // because it's being destroyed, or false otherwise.
  virtual void NativeViewDetaching(bool destroyed) = 0;

  // Called when our associated NativeViewHost is added to a View hierarchy
  // rooted at a valid Widget.
  virtual void AddedToWidget() = 0;

  // Called when our associated NativeViewHost is removed from a View hierarchy
  // rooted at a valid Widget.
  virtual void RemovedFromWidget() = 0;

  // Clips the corners of the gfx::NativeView to the `corner_radii` specified.
  // Returns true on success or false if the platform doesn't support the
  // operation.
  virtual bool SetCornerRadii(const gfx::RoundedCornersF& corner_radii) = 0;

  // Sets the height of the top region where gfx::NativeView shouldn't be
  // targeted.
  virtual void SetHitTestTopInset(int top_inset) = 0;
  virtual int GetHitTestTopInset() const = 0;

  // Installs a clip on the gfx::NativeView. These values are in the coordinate
  // space of the Widget, so if this method is called from ShowWidget
  // then the values need to be translated.
  virtual void InstallClip(int x, int y, int w, int h) = 0;

  // Whether or not a clip has been installed on the wrapped gfx::NativeView.
  virtual bool HasInstalledClip() = 0;

  // Removes the clip installed on the gfx::NativeView by way of InstallClip. A
  // following call to ShowWidget should occur after calling this method to
  // position the gfx::NativeView correctly, since the clipping process may have
  // adjusted its position.
  virtual void UninstallClip() = 0;

  // Shows the gfx::NativeView within the specified region (relative to the
  // parent native view) and with the given native size. The content will
  // appear scaled if the |native_w| or |native_h| are different from |w| or
  // |h|.
  virtual void ShowWidget(int x,
                          int y,
                          int w,
                          int h,
                          int native_w,
                          int native_h) = 0;

  // Hides the gfx::NativeView. NOTE: this may be invoked when the native view
  // is already hidden.
  virtual void HideWidget() = 0;

  // Sets focus to the gfx::NativeView.
  virtual void SetFocus() = 0;

  // Returns the container that contains the NativeViewHost's native view if
  // any.
  virtual gfx::NativeView GetNativeViewContainer() const = 0;

  // Return the native view accessible corresponding to the wrapped native
  // view.
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // Returns the cursor corresponding to the point (x, y) in the native view.
  virtual ui::Cursor GetCursor(int x, int y) = 0;

  // Sets the visibility of the gfx::NativeView. This differs from
  // {Show,Hide}Widget because it doesn't affect the placement, size,
  // or clipping of the view.
  virtual void SetVisible(bool visible) = 0;

  // Pass the parent accessible object to the native view so that it can return
  // this value when querying its parent accessible.
  virtual void SetParentAccessible(gfx::NativeViewAccessible) = 0;

  // Returns the parent accessible object to the native view.
  virtual gfx::NativeViewAccessible GetParentAccessible() = 0;

  // Returns the ui::Layer hosting the WebContents.
  virtual ui::Layer* GetUILayer() = 0;

  // Creates a platform-specific instance of an object implementing this
  // interface.
  static NativeViewHostWrapper* CreateWrapper(NativeViewHost* host);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_WRAPPER_H_
