// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_AURA_CONSTANTS_H_
#define UI_AURA_CLIENT_AURA_CONSTANTS_H_

#include <string>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
struct OwnedWindowAnchor;
}

namespace aura {
namespace client {
class FocusClient;

// Values used with property key kResizeBehaviorKey.
constexpr int kResizeBehaviorNone = 0;
constexpr int kResizeBehaviorCanResize = 1 << 0;
constexpr int kResizeBehaviorCanMaximize = 1 << 1;
constexpr int kResizeBehaviorCanMinimize = 1 << 2;
constexpr int kResizeBehaviorCanFullscreen = 1 << 3;

// A value used to represent an unassigned workspace for `kWindowWorkspaceKey`.
constexpr int kWindowWorkspaceUnassignedWorkspace = -1;

// A value used to represent a window being assigned to all workspaces for
// `kWindowWorkspaceKey`.
constexpr int kWindowWorkspaceVisibleOnAllWorkspaces = -2;

// Alphabetical sort.

// A property key to store whether accessibility touch exploration gets handled
// by the window and all touches pass through directly.
AURA_EXPORT extern const WindowProperty<bool>* const
    kAccessibilityTouchExplorationPassThrough;

// A property key to store whether activation on pointer event is enabled or
// not. The default value is true, which means windows are activated when a
// pointer down event occurs on them.
AURA_EXPORT extern const WindowProperty<bool>* const kActivateOnPointerKey;

// A property key to store whether animations are disabled for the window. Type
// of value is an int.
AURA_EXPORT extern const WindowProperty<bool>* const kAnimationsDisabledKey;

// A property key to store the app icon, typically larger for shelf icons, etc.
// This is not transported to the window service.
AURA_EXPORT extern const WindowProperty<gfx::ImageSkia*>* const kAppIconKey;

// A property key to store the aspect ratio of the window.
AURA_EXPORT extern const WindowProperty<gfx::SizeF*>* const kAspectRatio;

// A property key to store the avatar icon that will be displayed on the window
// frame to indicate the owner of the window when needed.
AURA_EXPORT extern const WindowProperty<gfx::ImageSkia*>* const kAvatarIconKey;

// A property key to indicate if a client window's layer is drawn.
// It is passed to the Window Service side for the occlusion tracker to process
// since the info is only available at the client side.
AURA_EXPORT extern const WindowProperty<bool>* const kWindowLayerDrawn;

// A property key to store if a window is a constrained window or not.
AURA_EXPORT extern const WindowProperty<bool>* const kConstrainedWindowKey;

// A property key to store if a window was created by a user gesture.
AURA_EXPORT extern const WindowProperty<bool>* const kCreatedByUserGesture;

// A property key to indicate the uuid of the desk this window belongs to.
AURA_EXPORT extern const WindowProperty<std::string*>* const kDeskUuidKey;

// A property key to indicate that a window should show that it deserves
// attention.
AURA_EXPORT extern const WindowProperty<bool>* const kDrawAttentionKey;

// A property key to store the focus client on the window.
AURA_EXPORT extern const WindowProperty<FocusClient*>* const kFocusClientKey;

// A property key to store the headless window bounds. This lets
// RenderWidgetHostViewAura find the requested headless window bounds which may
// be different from platform window bounds.
AURA_EXPORT extern const WindowProperty<gfx::Rect*>* const kHeadlessBoundsKey;

// A property key to store the host window of a window. This lets
// WebContentsViews find the windows that should constrain NPAPI plugins.
AURA_EXPORT extern const WindowProperty<Window*>* const kHostWindowKey;

// The modal parent of a child modal window.
AURA_EXPORT extern const WindowProperty<Window*>* const kChildModalParentKey;

// A property key to store the window modality.
AURA_EXPORT extern const WindowProperty<ui::mojom::ModalType>* const kModalKey;

// A property key to store the name of the window; mostly used for debugging.
AURA_EXPORT extern const WindowProperty<std::string*>* const kNameKey;

// A property key to store anchor to attach an owned anchored window to (such
// as tooltips, menus, etc).
AURA_EXPORT extern const WindowProperty<struct ui::OwnedWindowAnchor*>* const
    kOwnedWindowAnchor;

// A property key to store if a window drop shadow and resize shadow of a
// window are exactly the same as the window bounds, i.e. if resizing a window
// immediately resizes its shadows. Generally, resizing and content rendering
// happen in server side without any client involved, so without any delay in
// communication this value should be true: shadow bounds are the same as
// window bounds which define content bounds. For LaCros and other windows with
// server-controlled shadow but client-controlled content, this value should be
// false to ensure that the shadow is not immediately resized along with window
// in server side. Instead, the shadow waits for client content to catch up with
// the new window bounds first to avoid a gap between shadow and content
// (crbug.com/1199497).
// TODO(crbug.com/40197040): all exo clients that use server side resize shadow
// should have this property set to true.
AURA_EXPORT extern const WindowProperty<bool>* const kUseWindowBoundsForShadow;

// A property key to store the accessible parent of a native view. This is
// used to allow WebContents to access their accessible parents for use in
// walking up the accessibility tree via platform APIs.
AURA_EXPORT extern const aura::WindowProperty<gfx::NativeViewAccessible>* const
    kParentNativeViewAccessibleKey;

// A property key to store the preferred size of the window.
AURA_EXPORT extern const WindowProperty<gfx::Size*>* const kPreferredSize;

// A property key to store the resize behavior, which is a bitmask of the
// ResizeBehavior values.
AURA_EXPORT extern const WindowProperty<int>* const kResizeBehaviorKey;

// A property key to store the restore bounds in screen coordinates for a
// window.
AURA_EXPORT extern const WindowProperty<gfx::Rect*>* const kRestoreBoundsKey;

// A property key to store ui::mojom::WindowShowState for a window.
// See ui/base/ui_base_types.h for its definition.
AURA_EXPORT extern const WindowProperty<ui::mojom::WindowShowState>* const
    kShowStateKey;

// A property key to store the display id on which to put the fullscreen window.
// display::kInvalidDisplayId means use the display the window is currently on.
AURA_EXPORT extern const WindowProperty<int64_t>* const
    kFullscreenTargetDisplayIdKey;

// A property key to store ui::mojom::WindowShowState for a window to restore
// back to from the current window show state.
AURA_EXPORT extern const WindowProperty<ui::mojom::WindowShowState>* const
    kRestoreShowStateKey;

// A property key to store the raster scale. This affects the scale that exo
// windows are rasterized at. Currently, this only applies for lacros windows.
AURA_EXPORT extern const WindowProperty<float>* const kRasterScale;

// A property key to indicate if a window is currently being restored. Normally
// restoring a window equals to changing window's state to normal window state.
// This property will be used on Chrome OS to decide if we should use window
// state restore stack to decide which window state the window should restore
// back to, and it's not always the normal window state. As an example,
// unminimizing a window will restore the window back to its pre-minimized
// window state, which can have a non-normal window state. Note this property
// does not have any effort on any other operation systems except Chrome OS.
AURA_EXPORT extern const WindowProperty<bool>* const kIsRestoringKey;

// A property key to store key event dispatch policy. The default value is
// false, which means IME receives a key event in PREDISPATCH phace before a
// window receives it. If it's true, a window receives a key event before IME.
AURA_EXPORT extern const WindowProperty<bool>* const kSkipImeProcessing;

// A property key to store the title of the window; sometimes shown to users.
AURA_EXPORT extern const WindowProperty<std::u16string*>* const kTitleKey;

// The inset of the topmost view in the client view from the top of the
// non-client view. The topmost view depends on the window type. The topmost
// view is the tab strip for tabbed browser windows, the toolbar for popups,
// the web contents for app windows and varies for fullscreen windows.
AURA_EXPORT extern const WindowProperty<int>* const kTopViewInset;

// A property key to store the window icon, typically 16x16 for title bars.
AURA_EXPORT extern const WindowProperty<gfx::ImageSkia*>* const kWindowIconKey;

// The corner radius of a window in DIPs. Currently only used for shadows.
// Default is -1, meaning "unspecified". 0 Ensures corners are square.
AURA_EXPORT extern const WindowProperty<int>* const kWindowCornerRadiusKey;

// A property key to indicate a desk index of a workspace this window belongs
// to. The default value is kWindowWorkspaceUnassignedWorkspace.
AURA_EXPORT extern const WindowProperty<int>* const kWindowWorkspaceKey;

// A property key to store the z-ordering.
AURA_EXPORT extern const WindowProperty<ui::ZOrderLevel>* const kZOrderingKey;

// Alphabetical sort.

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_AURA_CONSTANTS_H_
