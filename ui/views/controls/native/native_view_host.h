// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_H_

#include <memory>
#include <optional>

#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"

namespace gfx {
class RoundedCornersF;
}

namespace ui {
class Layer;
}  // namespace ui

namespace views {
namespace test {
class NativeViewHostTestBase;
}

class NativeViewHostWrapper;

// If a NativeViewHost's native view is a Widget, this native window
// property is set on the widget, pointing to the owning NativeViewHost.
extern const char kWidgetNativeViewHostKey[];

// A View type that hosts a gfx::NativeView. The bounds of the native view are
// kept in sync with the bounds of this view as it is moved and sized.
// Under the hood, a platform-specific NativeViewHostWrapper implementation does
// the platform-specific work of manipulating the underlying OS widget type.
class VIEWS_EXPORT NativeViewHost : public View {
  METADATA_HEADER(NativeViewHost, View)

 public:
  NativeViewHost();

  NativeViewHost(const NativeViewHost&) = delete;
  NativeViewHost& operator=(const NativeViewHost&) = delete;

  ~NativeViewHost() override;

  // Attach a gfx::NativeView to this View. Its bounds will be kept in sync
  // with the bounds of this View until Detach is called.
  //
  // Because native views are positioned in the coordinates of their parent
  // native view, this function should only be called after this View has been
  // added to a View hierarchy hosted within a valid Widget.
  void Attach(gfx::NativeView native_view);

  // Detach the attached native view. Its bounds and visibility will no
  // longer be manipulated by this View. The native view may be destroyed and
  // detached before calling this function, and this has no effect in that case.
  void Detach();

  // Sets the corner radii for clipping gfx::NativeView. Returns true on success
  // or false if the platform doesn't support the operation. This method calls
  // SetCustomMask internally.
  bool SetCornerRadii(const gfx::RoundedCornersF& corner_radii);

  // Sets the height of the top region where the gfx::NativeView shouldn't be
  // targeted. This will be used when another view is covering there
  // temporarily, like the immersive fullscreen mode of ChromeOS.
  void SetHitTestTopInset(int top_inset);
  int GetHitTestTopInset() const;

  // Sets the size for the NativeView that may or may not match the size of this
  // View when it is being captured. If the size does not match, scaling will
  // occur. Pass an empty size to revert to the default behavior, where the
  // NatieView's size always equals this View's size.
  void SetNativeViewSize(const gfx::Size& size);

  // Returns the container that contains this host's native view. Returns null
  // if there's no attached native view or it has no container.
  gfx::NativeView GetNativeViewContainer() const;

  // Pass the parent accessible object to this host's native view so that
  // it can return this value when querying its parent accessible.
  void SetParentAccessible(gfx::NativeViewAccessible);

  // Returns the parent accessible object to this host's native view.
  gfx::NativeViewAccessible GetParentAccessible();

  // Fast resizing will move the native view and clip its visible region, this
  // will result in white areas and will not resize the content (so scrollbars
  // will be all wrong and content will flow offscreen). Only use this
  // when you're doing extremely quick, high-framerate vertical resizes
  // and don't care about accuracy. Make sure you do a real resize at the
  // end. USE WITH CAUTION.
  void set_fast_resize(bool fast_resize) { fast_resize_ = fast_resize; }
  bool fast_resize() const { return fast_resize_; }

  gfx::NativeView native_view() const { return native_view_; }

  void NativeViewDestroyed();

  // Sets the desired background color for repainting when the view is clipped.
  // Defaults to transparent color if unset.
  void SetBackgroundColorWhenClipped(std::optional<SkColor> color);

  // Returns the ui::Layer backing the attached gfx::NativeView.
  ui::Layer* GetUILayer();

  // Overridden from View:
  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnFocus() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void SetVisible(bool visible) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

 protected:
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  friend class test::NativeViewHostTestBase;

  // Detach the native view. |destroyed| is true if the native view is
  // detached because it's being destroyed, or false otherwise.
  void Detach(bool destroyed);

  // Invokes ViewRemoved() on the FocusManager for all the child Widgets of our
  // NativeView. This is used when detaching to ensure the FocusManager doesn't
  // have a reference to a View that is no longer reachable.
  void ClearFocus();

  // The attached native view. There is exactly one native_view_ attached.
  gfx::NativeView native_view_ = gfx::NativeView();

  // A platform-specific wrapper that does the OS-level manipulation of the
  // attached gfx::NativeView.
  std::unique_ptr<NativeViewHostWrapper> native_wrapper_;

  // The actual size of the NativeView, or an empty size if no scaling of the
  // NativeView should occur.
  gfx::Size native_view_size_;

  // True if the native view is being resized using the fast method described
  // in the setter/accessor above.
  bool fast_resize_ = false;

  // The color to use for repainting the background when the view is clipped.
  std::optional<SkColor> background_color_when_clipped_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_H_
