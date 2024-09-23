// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_node_id.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContentsAccessibility;
}

namespace ui {

// Pure abstract class that is used by `AXPlatformTreeManager` to gather
// information or perform actions that are implemented differently between the
// Web Content and the Views layers.
//
// TODO(nektar): Change `AXPlatformTreeManager` to take this delegate in its
// constructor and store it as a member variable.
class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformTreeManagerDelegate {
 public:
  virtual ~AXPlatformTreeManagerDelegate() = default;
  AXPlatformTreeManagerDelegate(const AXPlatformTreeManagerDelegate&) = delete;
  AXPlatformTreeManagerDelegate& operator=(
      const AXPlatformTreeManagerDelegate&) = delete;

  // Carries out an action on a specific UI element as requested by an assistive
  // software, such as opening the context menu or setting the value of a text
  // field. In this context, a "UI element" is represented by an `AXNodeID`,
  // i.e. the ID of a node in an accessibility tree.
  //
  // Note that even though such actions are programmatically requested, they
  // should be performed as if the user has initiated them, since the assistive
  // software is (directly or indirectly) acting on behalf of the user.
  virtual void AccessibilityPerformAction(const AXActionData& data) = 0;

  // Returns true if the `View` that contains the current accessibility tree is
  // focused. Example: the `content::RenderWidgetHostView` exposing the current
  // accessibility tree.
  //
  // Note that when accessibility is turned on, focus might behave differently,
  // including making some unfocusable UI elements to become focusable. See
  // `views::FocusBehavior`.
  virtual bool AccessibilityViewHasFocus() = 0;

  // Moves the focus inside the View` that contains the current accessibility
  // tree, Example: the `content::RenderWidgetHostView` exposing the current
  // accessibility tree.
  //
  // Note that when accessibility is turned on, focus might behave differently,
  // including making some unfocusable UI elements to become focusable. See
  // `views::FocusBehavior`.
  virtual void AccessibilityViewSetFocus() = 0;

  // Returns the bounds (in screen coordinates) of the `View` that contains the
  // current accessibility tree, Example: the `content::RenderWidgetHostView`
  // exposing the current accessibility tree.
  virtual gfx::Rect AccessibilityGetViewBounds() = 0;

  // Returns the multiplication factor by which the sizes of UI elements need to
  // be adjusted. In most cases this is 1.0F, however on some platforms element
  // sizes are not adjusted at the accessibility tree source, such as in Blink.
  virtual float AccessibilityGetDeviceScaleFactor() = 0;

  // The accessibility tree source has sent us invalid information. This could
  // indicate either a serious error or a malicious attack, e.g. from a rogue
  // renderer.
  virtual void UnrecoverableAccessibilityError() = 0;

  // Returns a handle to the platform specific widget containing the current
  // accessibility tree. Example: the HWND of the widget containing the
  // currently displayed web contents on Windows.
  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() = 0;

  // Returns a pointer to the platform specific accessibility object containing
  // the current accessibility tree. Example: a pointer to a Cocoa object
  // conforming to the `NSAccessibility` protocol on macOS.
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() = 0;

  // Same as above but for the current window, e.g. the currently focused
  // `NSWindow` object on macOS.
  virtual gfx::NativeViewAccessible
  AccessibilityGetNativeViewAccessibleForWindow() = 0;

  // Determines the node, together with its corresponding accessibility tree,
  // that is located at a specific point in the current tree's coordinates, i.e.
  // assumes that the current tree's root is located at (0, 0). Note that
  // multiple child trees could be contained within the current accessibility
  // tree, for example the accessibility tree for a webpage could contain
  // multiple child trees representing remote iframes.
  //
  // Note that hit testing in the Web layer needs to send a message via
  // inter-process communication to Blink in order to get back an accurate
  // response, which may depend on the interaction among various CSS rules. In
  // the Views layer, there is no such requirement, and thus a much simpler
  // implementation of "hit testing" could be provided.
  //
  // TODO(nektar): Use `std::optional` for all optional arguments.
  virtual void AccessibilityHitTest(
      const gfx::Point& point_in_view_pixels,
      const ax::mojom::Event& opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(AXPlatformTreeManager* hit_manager,
                              AXNodeID hit_node_id)> opt_callback) = 0;

  virtual gfx::NativeWindow GetTopLevelNativeWindow() = 0;

  virtual bool CanFireAccessibilityEvents() const = 0;

  // These methods are all specific to Web content, and should be removed from
  // here and into the content layer if and when possible. These were
  // moved into AXPlatformTreeManagerDelegate as part of the refactor
  // to move BrowserAccessibility* into the ui/ layer to support their reuse
  // in views. crbug.com/327499435
  virtual bool AccessibilityIsRootFrame() const = 0;

  // On Mac, VoiceOver moves focus to the web content when it receives an
  // AXLoadComplete event. On chrome's new tab page, focus should stay
  // in the omnibox, so we purposefully do not fire the AXLoadComplete
  // event in this case.
  virtual bool ShouldSuppressAXLoadComplete() = 0;
  virtual content::WebContentsAccessibility*
  AccessibilityGetWebContentsAccessibility() = 0;

 protected:
  AXPlatformTreeManagerDelegate() = default;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_
