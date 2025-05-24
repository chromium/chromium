// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform_node_id.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class AXPlatformNodeDelegate;

// AXPlatformNode is the abstract base class for an implementation of
// native accessibility APIs on supported platforms (e.g. Windows, Mac OS X).
// An object that wants to be accessible can derive from AXPlatformNodeDelegate
// and then call AXPlatformNode::Create. The delegate implementation should
// own the AXPlatformNode instance (or otherwise manage its lifecycle).
class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNode {
 public:
  // A Deleter for a std::unique_ptr<AXPlatformNode>.
  struct Deleter {
    void operator()(AXPlatformNode* platform_node) const {
      platform_node->Destroy();
    }
  };

  // A smart pointer for an AXPlatformNode.
  using Pointer = std::unique_ptr<AXPlatformNode, Deleter>;

  enum class AnnouncementType { kAlert, kPolite };

  using NativeWindowHandlerCallback =
      base::RepeatingCallback<AXPlatformNode*(gfx::NativeWindow)>;

  // Create an appropriate platform-specific instance. The delegate owns the
  // AXPlatformNode instance (or manages its lifecycle in some other way).
  static Pointer Create(AXPlatformNodeDelegate& delegate);

  // Cast a gfx::NativeViewAccessible to an AXPlatformNode if it is one,
  // or return nullptr if it's not an instance of this class.
  static AXPlatformNode* FromNativeViewAccessible(
      gfx::NativeViewAccessible accessible);

  // Return the AXPlatformNode at the root of the tree for a native window.
  static AXPlatformNode* FromNativeWindow(gfx::NativeWindow native_window);

  AXPlatformNode(const AXPlatformNode&) = delete;
  AXPlatformNode& operator=(const AXPlatformNode&) = delete;

  // Provide a function that returns the AXPlatformNode at the root of the
  // tree for a native window.
  static void RegisterNativeWindowHandler(NativeWindowHandlerCallback handler);

  // Disallow any updates to the AXMode when needing to force a certain AXMode,
  // like during testing.
  static void SetAXModeChangeAllowed(bool allow);
  static bool IsAXModeChangeAllowed();

  // Return the focused object in any UI popup overlaying content, or null.
  static gfx::NativeViewAccessible GetPopupFocusOverride();

  // Set the focused object withn any UI popup overlaying content, or null.
  // The focus override is the perceived focus within the popup, and it changes
  // each time a user navigates to a new item within the popup.
  static void SetPopupFocusOverride(gfx::NativeViewAccessible focus_override);

  // Returns true if the instance has been destroyed. Generally, no consumers
  // should hold a pointer to an instance after calling `Destroy`. On platforms
  // where an instance may outlive its delegate (e.g., on Windows where an
  // accessibility tool may hold references to COM objects), it is necessary to
  // check that an instance hasn't been destroyed before handling an inbound
  // call from the platform.
  virtual bool IsDestroyed() const = 0;

  // Get the platform-specific accessible object type for this instance.
  // On some platforms this is just a type cast, on others it may be a
  // wrapper object or handle.
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // Fire a platform-specific notification that an event has occurred on
  // this object.
  virtual void NotifyAccessibilityEvent(ax::mojom::Event event_type) = 0;

  // Returns the top-level URL for the active document. This should generally
  // correspond to what would be shown in the Omnibox.
  virtual std::string GetRootURL() const = 0;

  // Returns true if this node from web content.
  virtual bool IsWebContent() const = 0;

#if BUILDFLAG(IS_APPLE)
  // Fire a platform-specific notification to speak the |text| string.
  // AnnouncementType kPolite will speak the given string.
  // AnnouncementType kAlert may make a stronger attempt to be noticeable;
  // the screen reader may say something like "Alert: hello" instead of
  // just "hello", and may interrupt any existing text being spoken.
  // However, the screen reader may also just treat the two calls the same.
  virtual void AnnounceTextAs(const std::u16string& text,
                              AnnouncementType announcement_type) = 0;
#endif

  // Return this object's delegate. As with all methods, this must not be called
  // on an instance that has been destroyed (see `IsDestroyed()`).
  virtual AXPlatformNodeDelegate* GetDelegate() const = 0;

  // Return true if this object is equal to or a descendant of |ancestor|.
  virtual bool IsDescendantOf(AXPlatformNode* ancestor) const = 0;

  // Return the unique ID.
  AXPlatformNodeId GetUniqueId() const;

  // Creates a string representation of this node's data.
  std::string ToString() const;

  // Returns a string representation of the subtree of nodes rooted at this
  // node.
  std::string SubtreeToString() const;

  friend std::ostream& operator<<(std::ostream& stream, AXPlatformNode& node);

 protected:
  AXPlatformNode() = default;
  virtual ~AXPlatformNode() = default;

  // Associates a node delegate object to the platform node.
  // Keep it protected. Only AXPlatformNode::Create should be calling this.
  // Note: it would make a nicer design if initialization was integrated into
  // the platform node constructor, but platform node implementation on Windows
  // (AXPlatformNodeWin) relies on CComObject::CreateInstance() in order to
  // create a platform node instance, and it doesn't allow to pass arguments to
  // the constructor.
  virtual void Init(AXPlatformNodeDelegate& delegate) = 0;

  // Call Destroy rather than deleting this, because the subclass may
  // use reference counting.
  virtual void Destroy() = 0;

 private:
  static bool allow_ax_mode_changes_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_H_
