// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_VIEWS_HOSTABLE_H_
#define UI_BASE_COCOA_VIEWS_HOSTABLE_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace remote_cocoa {
namespace mojom {
class Application;
}  // namespace mojom
}  // namespace remote_cocoa

namespace ui {

class Layer;

// Interface that it used to stitch a content::WebContentsView into a
// views::View.
// TODO(ccameron): Move this to components/remote_cocoa.
class ViewsHostableView {
 public:
  // Host interface through which the WebContentsView may indicate that its C++
  // object is destroying.
  class Host {
   public:
    // Query the ui::Layer of the host.
    virtual ui::Layer* GetUiLayer() const = 0;

    // Return the mojo interface to the application in a remote process in which
    // the host NSView exists. Used to migrate the content::WebContentsView and
    // content::RenderWidgetHostView to that process.
    virtual remote_cocoa::mojom::Application* GetRemoteCocoaApplication()
        const = 0;

    // The id for the views::View's NSView. Used to add the
    // content::WebContentsView's NSView as a child view.
    virtual uint64_t GetNSViewId() const = 0;

    // Called when the hostable view will be destroyed.
    virtual void OnHostableViewDestroying() = 0;
  };

  // Called to add the content::WebContentsView's NSView as a subview of the
  // views::View's NSView. This is responsible for:
  // - Adding the WebContentsView's ui::Layer to the parent's ui::Layer tree.
  // - Stitching together the accessibility tree between the views::View and
  //   the WebContentsView.
  // - Adding the WebContents browser-side and app-shim-side NSViews as children
  //   to the views::View's NSViews.
  virtual void ViewsHostableAttach(Host* host) = 0;

  // Called when the WebContentsView's NSView has been removed from the
  // views::View's NSView. This is responsible for un-doing all of the actions
  // taken when attaching.
  virtual void ViewsHostableDetach() = 0;

  // Resize the WebContentsView's NSView.
  virtual void ViewsHostableSetBounds(const gfx::Rect& bounds_in_window) = 0;

  // Show or hide the WebContentsView's NSView.
  virtual void ViewsHostableSetVisible(bool visible) = 0;

  // Make the WebContentsView's NSView be a first responder.
  virtual void ViewsHostableMakeFirstResponder() = 0;

  // Set the WebContentsView's parent accessibility element.
  virtual void ViewsHostableSetParentAccessible(
      gfx::NativeViewAccessible parent_accessibility_element) = 0;

  // Get the WebContentsView's parent accessibility element.
  virtual gfx::NativeViewAccessible ViewsHostableGetParentAccessible() = 0;

  // Retrieve the WebContentsView's accessibility element.
  virtual gfx::NativeViewAccessible ViewsHostableGetAccessibilityElement() = 0;
};

}  // namespace ui

// The protocol through which an NSView indicates support for the
// ViewsHostableView interface.
@protocol ViewsHostable

- (ui::ViewsHostableView*)viewsHostableView;

@end

#endif  // UI_BASE_COCOA_VIEWS_HOSTABLE_H_
