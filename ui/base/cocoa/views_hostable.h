// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_VIEWS_HOSTABLE_H_
#define UI_BASE_COCOA_VIEWS_HOSTABLE_H_

#import <objc/objc.h>

#include "ui/base/ui_base_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class Layer;

// Interface that it used to stitch a content::WebContentsView into a
// views::View.
class ViewsHostableView {
 public:
  // Host interface through which the WebContentsView may indicate that its C++
  // object is destroying.
  class Host {
   public:
    // Query the ui::Layer of the host.
    virtual ui::Layer* GetUiLayer() const = 0;

    // Return the id for the process in which the host NSView exists. Used to
    // migrate the content::WebContentsView and content::RenderWidgetHostview
    // to that process.
    virtual uint64_t GetViewsFactoryHostId() const = 0;

    // The id for the views::View's NSView. Used to add the
    // content::WebContentsView's NSView as a child view.
    virtual uint64_t GetNSViewId() const = 0;

    // Query the parent accessibility element of the host.
    virtual id GetAccessibilityElement() const = 0;

    // Called when the hostable view will be destroyed.
    virtual void OnHostableViewDestroying() = 0;
  };

  // Called when the content::WebContentsView's NSView is added as a subview of
  // the views::View's NSView (note that these are the browser-side NSViews).
  // This is responsible for:
  // - Adding the WebContentsView's ui::Layer to the parent's ui::Layer tree.
  // - Stitching together the accessibility tree between the views::View and
  //   the WebContentsView.
  // - Stitching together any app-shim-side NSViews.
  virtual void OnViewsHostableAttached(Host* host) = 0;

  // Called when the WebContentsView's NSView has been removed from the
  // views::View's NSView. This is responsible for un-doing all of the actions
  // taken when attaching.
  virtual void OnViewsHostableDetached() = 0;

  // Called when the WebContentsView's NSView is to be shown or resized.
  virtual void OnViewsHostableShow(const gfx::Rect& bounds_in_window) = 0;

  // Called when the WebContentsView's NSView is to be hidden.
  virtual void OnViewsHostableHide() = 0;

  // Called when the WebContentsView's NSView is to be made a first responder.
  virtual void OnViewsHostableMakeFirstResponder() = 0;
};

}  // namespace ui

// The protocol through which an NSView indicates support for the
// ViewsHostableView interface.
@protocol ViewsHostable

- (ui::ViewsHostableView*)viewsHostableView;

@end

#endif  // UI_BASE_COCOA_VIEWS_HOSTABLE_H_
