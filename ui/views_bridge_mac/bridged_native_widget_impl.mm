// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views_bridge_mac/bridged_native_widget_impl.h"

#import <objc/runtime.h>
#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/gfx/mac/nswindow_frame_controls.h"
#import "ui/views_bridge_mac/bridged_content_view.h"
#import "ui/views_bridge_mac/bridged_native_widget_host_helper.h"
#import "ui/views_bridge_mac/browser_native_widget_window_mac.h"
#import "ui/views_bridge_mac/cocoa_mouse_capture.h"
#import "ui/views_bridge_mac/cocoa_window_move_loop.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget_host.mojom.h"
#import "ui/views_bridge_mac/native_widget_mac_frameless_nswindow.h"
#import "ui/views_bridge_mac/native_widget_mac_nswindow.h"
#import "ui/views_bridge_mac/views_nswindow_delegate.h"

using views_bridge_mac::mojom::VisibilityTransition;
using views_bridge_mac::mojom::WindowVisibilityState;

namespace {
constexpr auto kUIPaintTimeout = base::TimeDelta::FromSeconds(5);
}  // namespace

// Self-owning animation delegate that starts a hide animation, then calls
// -[NSWindow close] when the animation ends, releasing itself.
@interface ViewsNSWindowCloseAnimator : NSObject<NSAnimationDelegate> {
 @private
  base::scoped_nsobject<NSWindow> window_;
  base::scoped_nsobject<NSAnimation> animation_;
}
+ (void)closeWindowWithAnimation:(NSWindow*)window;
@end

@implementation ViewsNSWindowCloseAnimator

- (instancetype)initWithWindow:(NSWindow*)window {
  if ((self = [super init])) {
    window_.reset([window retain]);
    animation_.reset(
        [[ConstrainedWindowAnimationHide alloc] initWithWindow:window]);
    [animation_ setDelegate:self];
    [animation_ setAnimationBlockingMode:NSAnimationNonblocking];
    [animation_ startAnimation];
  }
  return self;
}

+ (void)closeWindowWithAnimation:(NSWindow*)window {
  [[ViewsNSWindowCloseAnimator alloc] initWithWindow:window];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [window_ close];
  [animation_ setDelegate:nil];
  [self release];
}
@end

// This class overrides NSAnimation methods to invalidate the shadow for each
// frame. It is required because the show animation uses CGSSetWindowWarp()
// which is touchy about the consistency of the points it is given. The show
// animation includes a translate, which fails to apply properly to the window
// shadow, when that shadow is derived from a layer-hosting view. So invalidate
// it. This invalidation is only needed to cater for the translate. It is not
// required if CGSSetWindowWarp() is used in a way that keeps the center point
// of the window stationary (e.g. a scale). It's also not required for the hide
// animation: in that case, the shadow is never invalidated so retains the
// shadow calculated before a translate is applied.
@interface ModalShowAnimationWithLayer
    : ConstrainedWindowAnimationShow<NSAnimationDelegate>
@end

@implementation ModalShowAnimationWithLayer {
  // This is the "real" delegate, but this class acts as the NSAnimationDelegate
  // to avoid a separate object.
  views::BridgedNativeWidgetImpl* bridgedNativeWidget_;
}
- (instancetype)initWithBridgedNativeWidget:
    (views::BridgedNativeWidgetImpl*)widget {
  if ((self = [super initWithWindow:widget->ns_window()])) {
    bridgedNativeWidget_ = widget;
    [self setDelegate:self];
  }
  return self;
}
- (void)dealloc {
  DCHECK(!bridgedNativeWidget_);
  [super dealloc];
}
- (void)animationDidEnd:(NSAnimation*)animation {
  DCHECK(bridgedNativeWidget_);
  bridgedNativeWidget_->OnShowAnimationComplete();
  bridgedNativeWidget_ = nullptr;
  [self setDelegate:nil];
}
- (void)stopAnimation {
  [super stopAnimation];
  [window_ invalidateShadow];
}
- (void)setCurrentProgress:(NSAnimationProgress)progress {
  [super setCurrentProgress:progress];
  [window_ invalidateShadow];
}
@end

namespace views {

namespace {

using RankMap = std::map<NSView*, int>;

// SDK 10.11 contains incompatible changes of sortSubviewsUsingFunction.
// It takes (__kindof NSView*) as comparator argument.
// https://llvm.org/bugs/show_bug.cgi?id=25149
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
using NSViewComparatorValue = id;
#else
using NSViewComparatorValue = __kindof NSView*;
#endif

// Returns true if the content_view is reparented.
bool PositionWindowInNativeViewParent(NSView* content_view) {
  return [[content_view window] contentView] != content_view;
}

// Return the offset of the parent native view from the window.
gfx::Vector2d GetNativeViewParentOffset(NSView* content_view) {
  NSWindow* window = [content_view window];
  NSView* parent_view = [content_view superview];
  NSPoint p = NSMakePoint(0, NSHeight([parent_view frame]));
  p = [parent_view convertPoint:p toView:nil];
  return gfx::Vector2d(p.x, NSHeight([window frame]) - p.y);
}

// Return the content size for a minimum or maximum widget size.
gfx::Size GetClientSizeForWindowSize(NSWindow* window,
                                     const gfx::Size& window_size) {
  NSRect frame_rect =
      NSMakeRect(0, 0, window_size.width(), window_size.height());
  // Note gfx::Size will prevent dimensions going negative. They are allowed to
  // be zero at this point, because Widget::GetMinimumSize() may later increase
  // the size.
  return gfx::Size([window contentRectForFrameRect:frame_rect].size);
}

NSComparisonResult SubviewSorter(NSViewComparatorValue lhs,
                                 NSViewComparatorValue rhs,
                                 void* rank_as_void) {
  DCHECK_NE(lhs, rhs);

  const RankMap* rank = static_cast<const RankMap*>(rank_as_void);
  auto left_rank = rank->find(lhs);
  auto right_rank = rank->find(rhs);
  bool left_found = left_rank != rank->end();
  bool right_found = right_rank != rank->end();

  // Sort unassociated views above associated views.
  if (left_found != right_found)
    return left_found ? NSOrderedAscending : NSOrderedDescending;

  if (left_found) {
    return left_rank->second < right_rank->second ? NSOrderedAscending
                                                  : NSOrderedDescending;
  }

  // If both are unassociated, consider that order is not important
  return NSOrderedSame;
}

// Counts windows managed by a BridgedNativeWidgetImpl instance in the
// |child_windows| array ignoring the windows added by AppKit.
NSUInteger CountBridgedWindows(NSArray* child_windows) {
  NSUInteger count = 0;
  for (NSWindow* child in child_windows)
    if ([[child delegate] isKindOfClass:[ViewsNSWindowDelegate class]])
      ++count;

  return count;
}

std::map<uint64_t, BridgedNativeWidgetImpl*>& GetIdToWidgetImplMap() {
  static base::NoDestructor<std::map<uint64_t, BridgedNativeWidgetImpl*>>
      id_map;
  return *id_map;
}

}  // namespace

// static
gfx::Size BridgedNativeWidgetImpl::GetWindowSizeForClientSize(
    NSWindow* window,
    const gfx::Size& content_size) {
  NSRect content_rect =
      NSMakeRect(0, 0, content_size.width(), content_size.height());
  NSRect frame_rect = [window frameRectForContentRect:content_rect];
  return gfx::Size(NSWidth(frame_rect), NSHeight(frame_rect));
}

// static
BridgedNativeWidgetImpl* BridgedNativeWidgetImpl::GetFromId(
    uint64_t bridged_native_widget_id) {
  auto found = GetIdToWidgetImplMap().find(bridged_native_widget_id);
  if (found == GetIdToWidgetImplMap().end())
    return nullptr;
  return found->second;
}

// static
base::scoped_nsobject<NativeWidgetMacNSWindow>
BridgedNativeWidgetImpl::CreateNSWindow(
    const views_bridge_mac::mojom::CreateWindowParams* params) {
  base::scoped_nsobject<NativeWidgetMacNSWindow> ns_window;
  switch (params->window_class) {
    case views_bridge_mac::mojom::WindowClass::kDefault:
      ns_window.reset([[NativeWidgetMacNSWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:params->style_mask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
      break;
    case views_bridge_mac::mojom::WindowClass::kBrowser:
      ns_window.reset([[BrowserNativeWidgetWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:params->style_mask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
      break;
    case views_bridge_mac::mojom::WindowClass::kFrameless:
      ns_window.reset([[NativeWidgetMacFramelessNSWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:params->style_mask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
      break;
  }
  if (@available(macOS 10.10, *)) {
    if (params->titlebar_appears_transparent)
      [ns_window setTitlebarAppearsTransparent:YES];

    if (params->window_title_hidden)
      [ns_window setTitleVisibility:NSWindowTitleHidden];
  }
  if (params->animation_enabled)
    [ns_window setAnimationBehavior:NSWindowAnimationBehaviorDocumentWindow];
  return ns_window;
}

BridgedNativeWidgetImpl::BridgedNativeWidgetImpl(
    uint64_t bridged_native_widget_id,
    BridgedNativeWidgetHost* host,
    BridgedNativeWidgetHostHelper* host_helper)
    : id_(bridged_native_widget_id),
      host_(host),
      host_helper_(host_helper),
      bridge_mojo_binding_(this) {
  DCHECK(GetIdToWidgetImplMap().find(id_) == GetIdToWidgetImplMap().end());
  GetIdToWidgetImplMap().insert(std::make_pair(id_, this));
}

BridgedNativeWidgetImpl::~BridgedNativeWidgetImpl() {
  // The delegate should be cleared already. Note this enforces the precondition
  // that -[NSWindow close] is invoked on the hosted window before the
  // destructor is called.
  DCHECK(![window_ delegate]);
  DCHECK(child_windows_.empty());
  DestroyContentView();
}

void BridgedNativeWidgetImpl::BindRequest(
    views_bridge_mac::mojom::BridgedNativeWidgetAssociatedRequest request,
    base::OnceClosure connection_closed_callback) {
  bridge_mojo_binding_.Bind(std::move(request),
                            ui::WindowResizeHelperMac::Get()->task_runner());
  bridge_mojo_binding_.set_connection_error_handler(
      std::move(connection_closed_callback));
}

void BridgedNativeWidgetImpl::SetWindow(
    base::scoped_nsobject<NativeWidgetMacNSWindow> window) {
  DCHECK(!window_);
  window_delegate_.reset(
      [[ViewsNSWindowDelegate alloc] initWithBridgedNativeWidget:this]);
  window_ = std::move(window);
  [window_ setBridgeImpl:this];
  [window_ setBridgedNativeWidgetId:id_];
  [window_ setReleasedWhenClosed:NO];  // Owned by scoped_nsobject.
  [window_ setDelegate:window_delegate_];
  ui::CATransactionCoordinator::Get().AddPreCommitObserver(this);
}

void BridgedNativeWidgetImpl::SetParent(uint64_t new_parent_id) {
  // Remove from the old parent.
  if (parent_) {
    parent_->RemoveChildWindow(this);
    parent_ = nullptr;
  }
  if (!new_parent_id)
    return;

  // It is only valid to have a NativeWidgetMac be the parent of another
  // NativeWidgetMac.
  BridgedNativeWidgetImpl* new_parent =
      BridgedNativeWidgetImpl::GetFromId(new_parent_id);
  DCHECK(new_parent);

  // If the parent is another BridgedNativeWidgetImpl, just add to the
  // collection of child windows it owns and manages. Otherwise, create an
  // adapter to anchor the child widget and observe when the parent NSWindow is
  // closed.
  parent_ = new_parent;
  parent_->child_windows_.push_back(this);

  // Widget::ShowInactive() could result in a Space switch when the widget has a
  // parent, and we're calling -orderWindow:relativeTo:. Use Transient
  // collection behaviour to prevent that.
  // https://crbug.com/697829
  [window_ setCollectionBehavior:[window_ collectionBehavior] |
                                 NSWindowCollectionBehaviorTransient];

  // If |window_| was already visible then add it as a child window immediately.
  // As in OnVisibilityChanged, do not set a parent for sheets.
  if (window_visible_ && ![window_ isSheet])
    [parent_->ns_window() addChildWindow:window_ ordered:NSWindowAbove];
}

void BridgedNativeWidgetImpl::CreateWindow(
    views_bridge_mac::mojom::CreateWindowParamsPtr params) {
  SetWindow(CreateNSWindow(params.get()));
}

void BridgedNativeWidgetImpl::InitWindow(
    views_bridge_mac::mojom::BridgedNativeWidgetInitParamsPtr params) {
  modal_type_ = params->modal_type;
  is_translucent_window_ = params->is_translucent;
  widget_is_top_level_ = params->widget_is_top_level;
  position_window_in_screen_coords_ = params->position_window_in_screen_coords;

  // Register for application hide notifications so that visibility can be
  // properly tracked. This is not done in the delegate so that the lifetime is
  // tied to the C++ object, rather than the delegate (which may be reference
  // counted). This is required since the application hides do not send an
  // orderOut: to individual windows. Unhide, however, does send an order
  // message.
  [[NSNotificationCenter defaultCenter]
      addObserver:window_delegate_
         selector:@selector(onWindowOrderChanged:)
             name:NSApplicationDidHideNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:window_delegate_
         selector:@selector(onSystemControlTintChanged:)
             name:NSControlTintDidChangeNotification
           object:nil];

  // Validate the window's initial state, otherwise the bridge's initial
  // tracking state will be incorrect.
  DCHECK(![window_ isVisible]);
  DCHECK_EQ(0u, [window_ styleMask] & NSFullScreenWindowMask);

  // Include "regular" windows without the standard frame in the window cycle.
  // These use NSBorderlessWindowMask so do not get it by default.
  if (params->force_into_collection_cycle) {
    [window_
        setCollectionBehavior:[window_ collectionBehavior] |
                              NSWindowCollectionBehaviorParticipatesInCycle];
  }

  [window_ setHasShadow:params->has_window_server_shadow];
}

void BridgedNativeWidgetImpl::SetInitialBounds(
    const gfx::Rect& new_bounds,
    const gfx::Size& minimum_content_size) {
  gfx::Rect adjusted_bounds = new_bounds;
  if (new_bounds.IsEmpty()) {
    // If a position is set, but no size, complain. Otherwise, a 1x1 window
    // would appear there, which might be unexpected.
    DCHECK(new_bounds.origin().IsOrigin())
        << "Zero-sized windows not supported on Mac.";

    // Otherwise, bounds is all zeroes. Cocoa will currently have the window at
    // the bottom left of the screen. To support a client calling SetSize() only
    // (and for consistency across platforms) put it at the top-left instead.
    // Read back the current frame: it will be a 1x1 context rect but the frame
    // size also depends on the window style.
    NSRect frame_rect = [window_ frame];
    adjusted_bounds = gfx::Rect(
        gfx::Point(), gfx::Size(NSWidth(frame_rect), NSHeight(frame_rect)));
  }
  SetBounds(adjusted_bounds, minimum_content_size);
}

void BridgedNativeWidgetImpl::SetBounds(const gfx::Rect& new_bounds,
                                        const gfx::Size& minimum_content_size) {
  // -[NSWindow contentMinSize] is only checked by Cocoa for user-initiated
  // resizes. This is not what toolkit-views expects, so clamp. Note there is
  // no check for maximum size (consistent with aura::Window::SetBounds()).
  gfx::Size clamped_content_size =
      GetClientSizeForWindowSize(window_, new_bounds.size());
  clamped_content_size.SetToMax(minimum_content_size);

  // A contentRect with zero width or height is a banned practice in ChromeMac,
  // due to unpredictable OSX treatment.
  DCHECK(!clamped_content_size.IsEmpty())
      << "Zero-sized windows not supported on Mac";

  if (!window_visible_ && IsWindowModalSheet()) {
    // Window-Modal dialogs (i.e. sheets) are positioned by Cocoa when shown for
    // the first time. They also have no frame, so just update the content size.
    [window_ setContentSize:NSMakeSize(clamped_content_size.width(),
                                       clamped_content_size.height())];
    return;
  }
  gfx::Rect actual_new_bounds(
      new_bounds.origin(),
      GetWindowSizeForClientSize(window_, clamped_content_size));

  if (parent_ && !position_window_in_screen_coords_)
    actual_new_bounds.Offset(parent_->GetChildWindowOffset());

  if (PositionWindowInNativeViewParent(bridged_view_))
    actual_new_bounds.Offset(GetNativeViewParentOffset(bridged_view_));

  [window_ setFrame:gfx::ScreenRectToNSRect(actual_new_bounds)
            display:YES
            animate:NO];
}

void BridgedNativeWidgetImpl::SetSizeAndCenter(
    const gfx::Size& content_size,
    const gfx::Size& minimum_content_size) {
  gfx::Rect new_window_bounds = gfx::ScreenRectFromNSRect([window_ frame]);
  new_window_bounds.set_size(GetWindowSizeForClientSize(window_, content_size));
  SetBounds(new_window_bounds, minimum_content_size);

  // Note that this is not the precise center of screen, but it is the standard
  // location for windows like dialogs to appear on screen for Mac.
  // TODO(tapted): If there is a parent window, center in that instead.
  [window_ center];
}

void BridgedNativeWidgetImpl::DestroyContentView() {
  if (!bridged_view_)
    return;
  [bridged_view_ clearView];
  bridged_view_id_mapping_.reset();
  bridged_view_.reset();
  [window_ setContentView:nil];
}

void BridgedNativeWidgetImpl::CreateContentView(uint64_t ns_view_id,
                                                const gfx::Rect& bounds) {
  DCHECK(!bridged_view_);

  bridged_view_.reset(
      [[BridgedContentView alloc] initWithBridge:this bounds:bounds]);
  bridged_view_id_mapping_ = std::make_unique<ui::ScopedNSViewIdMapping>(
      ns_view_id, bridged_view_.get());

  // Objective C initializers can return nil. However, if |view| is non-NULL
  // this should be treated as an error and caught early.
  CHECK(bridged_view_);

  // Set the layer first to create a layer-hosting view (not layer-backed), and
  // set the compositor output to go to that layer.
  base::scoped_nsobject<CALayer> background_layer([[CALayer alloc] init]);
  display_ca_layer_tree_ =
      std::make_unique<ui::DisplayCALayerTree>(background_layer.get());
  [bridged_view_ setLayer:background_layer];
  [bridged_view_ setWantsLayer:YES];

  [window_ setContentView:bridged_view_];
}

void BridgedNativeWidgetImpl::CloseWindow() {
  // Keep |window| on the stack so that the ObjectiveC block below can capture
  // it and properly increment the reference count bound to the posted task.
  NSWindow* window = ns_window();

  if (IsWindowModalSheet()) {
    // Sheets can't be closed normally. This starts the sheet closing. Once the
    // sheet has finished animating, it will call sheetDidEnd: on the parent
    // window's delegate. Note it still needs to be asynchronous, since code
    // calling Widget::Close() doesn't expect things to be deleted upon return.
    // Ensure |window| is retained by a block. Note in some cases during
    // teardown, [window sheetParent] may be nil.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(base::RetainBlock(^{
          [NSApp endSheet:window];
        })));
    return;
  }

  // For other modal types, animate the close.
  if (ShouldRunCustomAnimationFor(VisibilityTransition::kHide)) {
    [ViewsNSWindowCloseAnimator closeWindowWithAnimation:window];
    return;
  }

  // Destroy the content view so that it won't call back into |host_| while
  // being torn down.
  DestroyContentView();

  // Widget::Close() ensures [Non]ClientView::CanClose() returns true, so there
  // is no need to call the NSWindow or its delegate's -windowShouldClose:
  // implementation in the manner of -[NSWindow performClose:]. But,
  // like -performClose:, first remove the window from AppKit's display
  // list to avoid crashes like http://crbug.com/156101.
  [window orderOut:nil];

  // Many tests assume that base::RunLoop().RunUntilIdle() is always sufficient
  // to execute a close. However, in rare cases, -performSelector:..afterDelay:0
  // does not do this. So post a regular task.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(base::RetainBlock(^{
        [window close];
      })));
}

void BridgedNativeWidgetImpl::CloseWindowNow() {
  // NSWindows must be retained until -[NSWindow close] returns.
  auto window_retain = window_;

  // If there's a bridge at this point, it means there must be a window as well.
  DCHECK(window_);
  [window_ close];
  // Note: |this| will be deleted here.
}

void BridgedNativeWidgetImpl::SetVisibilityState(
    WindowVisibilityState new_state) {
  // Ensure that:
  //  - A window with an invisible parent is not made visible.
  //  - A parent changing visibility updates child window visibility.
  //    * But only when changed via this function - ignore changes via the
  //      NSWindow API, or changes propagating out from here.
  wants_to_be_visible_ = new_state != WindowVisibilityState::kHideWindow;

  [show_animation_ stopAnimation];
  DCHECK(!show_animation_);

  if (new_state == WindowVisibilityState::kHideWindow) {
    // Calling -orderOut: on a window with an attached sheet encounters broken
    // AppKit behavior. The sheet effectively becomes "lost".
    // See http://crbug.com/667602. Alternatives: call -setAlphaValue:0 and
    // -setIgnoresMouseEvents:YES on the NSWindow, or dismiss the sheet before
    // hiding.
    //
    // TODO(ellyjones): Sort this entire situation out. This DCHECK doesn't
    // trigger in shipped builds, but it does trigger when the browser exits
    // "abnormally" (not via one of the UI paths to exiting), such as in browser
    // tests, so this breaks a slew of browser tests in MacViews mode. See also
    // https://crbug.com/834926.
    // DCHECK(![window_ attachedSheet]);

    [window_ orderOut:nil];
    DCHECK(!window_visible_);
    return;
  }

  DCHECK(wants_to_be_visible_);

  if (!ca_transaction_sync_suppressed_)
    ui::CATransactionCoordinator::Get().Synchronize();

  // If the parent (or an ancestor) is hidden, return and wait for it to become
  // visible.
  for (auto* ancestor = parent_; ancestor; ancestor = ancestor->parent_) {
    if (!ancestor->window_visible_)
      return;
  }

  if (IsWindowModalSheet()) {
    ShowAsModalSheet();
    return;
  }

  if (new_state == WindowVisibilityState::kShowAndActivateWindow) {
    [window_ makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
  } else {
    // ui::SHOW_STATE_INACTIVE is typically used to avoid stealing focus from a
    // parent window. So, if there's a parent, order above that. Otherwise, this
    // will order above all windows at the same level.
    NSInteger parent_window_number = 0;
    if (parent_) {
      // When there's a parent, check if the window is already visible. If
      // ShowInactive() is called on an already-visible window, there should be
      // no effect: the macOS childWindow mechanism should have already raised
      // the window to the right stacking order. More importantly, invoking
      // -[NSWindow orderWindow:] could cause a Space switch, which defeats the
      // point of ShowInactive(), so avoid it. See https://crbug.com/866760.

      // Sanity check: if the window is visible, the prior Show should have
      // hooked it up as a native child window already.
      DCHECK_EQ(window_visible_, !![window_ parentWindow]);
      if (window_visible_)
        return;  // Avoid a Spaces transition.

      parent_window_number = [parent_->ns_window() windowNumber];
    }

    [window_ orderWindow:NSWindowAbove relativeTo:parent_window_number];
  }
  DCHECK(window_visible_);

  // For non-sheet modal types, use the constrained window animations to make
  // the window appear.
  if (ShouldRunCustomAnimationFor(VisibilityTransition::kShow)) {
    show_animation_.reset(
        [[ModalShowAnimationWithLayer alloc] initWithBridgedNativeWidget:this]);

    // The default mode is blocking, which would block the UI thread for the
    // duration of the animation, but would keep it smooth. The window also
    // hasn't yet received a frame from the compositor at this stage, so it is
    // fully transparent until the GPU sends a frame swap IPC. For the blocking
    // option, the animation needs to wait until
    // AcceleratedWidgetCALayerParamsUpdated has been called at least once,
    // otherwise it will animate nothing.
    [show_animation_ setAnimationBlockingMode:NSAnimationNonblocking];
    [show_animation_ startAnimation];
  }
}

void BridgedNativeWidgetImpl::SetTransitionsToAnimate(
    VisibilityTransition transitions) {
  // TODO(tapted): Use scoping to disable native animations at appropriate
  // times as well.
  transitions_to_animate_ = transitions;
}

void BridgedNativeWidgetImpl::AcquireCapture() {
  if (HasCapture())
    return;
  if (!window_visible_)
    return;  // Capture on hidden windows is disallowed.

  mouse_capture_.reset(new CocoaMouseCapture(this));
  host_->OnMouseCaptureActiveChanged(true);

  // Initiating global event capture with addGlobalMonitorForEventsMatchingMask:
  // will reset the mouse cursor to an arrow. Asking the window for an update
  // here will restore what we want. However, it can sometimes cause the cursor
  // to flicker, once, on the initial mouseDown.
  // TODO(tapted): Make this unnecessary by only asking for global mouse capture
  // for the cases that need it (e.g. menus, but not drag and drop).
  [window_ cursorUpdate:[NSApp currentEvent]];
}

void BridgedNativeWidgetImpl::ReleaseCapture() {
  mouse_capture_.reset();
}

bool BridgedNativeWidgetImpl::HasCapture() {
  return mouse_capture_ && mouse_capture_->IsActive();
}

bool BridgedNativeWidgetImpl::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  DCHECK(!HasCapture());
  // https://crbug.com/876493
  CHECK(!window_move_loop_);

  // RunMoveLoop caller is responsible for updating the window to be under the
  // mouse, but it does this using possibly outdated coordinate from the mouse
  // event, and mouse is very likely moved beyound that point.

  // Compensate for mouse drift by shifting the initial mouse position we pass
  // to CocoaWindowMoveLoop, so as it handles incoming move events the window's
  // top left corner will be |drag_offset| from the current mouse position.

  const gfx::Rect frame = gfx::ScreenRectFromNSRect([window_ frame]);
  const gfx::Point mouse_in_screen(frame.x() + drag_offset.x(),
                                   frame.y() + drag_offset.y());
  window_move_loop_.reset(new CocoaWindowMoveLoop(
      this, gfx::ScreenPointToNSPoint(mouse_in_screen)));

  return window_move_loop_->Run();

  // |this| may be destroyed during the RunLoop, causing it to exit early.
  // Even if that doesn't happen, CocoaWindowMoveLoop will clean itself up by
  // calling EndMoveLoop(). So window_move_loop_ will always be null before the
  // function returns. But don't DCHECK since |this| might not be valid.
}

void BridgedNativeWidgetImpl::EndMoveLoop() {
  DCHECK(window_move_loop_);
  window_move_loop_->End();
  window_move_loop_.reset();
}

void BridgedNativeWidgetImpl::SetCursor(NSCursor* cursor) {
  [window_delegate_ setCursor:cursor];
}

void BridgedNativeWidgetImpl::OnWindowWillClose() {
  ui::CATransactionCoordinator::Get().RemovePreCommitObserver(this);
  host_->OnWindowWillClose();

  // Ensure BridgedNativeWidgetImpl does not have capture, otherwise
  // OnMouseCaptureLost() may reference a deleted |host_| when called via
  // ~CocoaMouseCapture() upon the destruction of |mouse_capture_|. See
  // https://crbug.com/622201. Also we do this before setting the delegate to
  // nil, because this may lead to callbacks to bridge which rely on a valid
  // delegate.
  ReleaseCapture();

  if (parent_) {
    parent_->RemoveChildWindow(this);
    parent_ = nullptr;
  }
  [[NSNotificationCenter defaultCenter] removeObserver:window_delegate_];

  [show_animation_ stopAnimation];  // If set, calls OnShowAnimationComplete().
  DCHECK(!show_animation_);

  [window_ setDelegate:nil];
  [window_ setBridgeImpl:nullptr];

  // Ensure that |this| cannot be reached by its id while it is being destroyed.
  size_t erased = GetIdToWidgetImplMap().erase(id_);
  DCHECK_EQ(1u, erased);

  RemoveOrDestroyChildren();
  DCHECK(child_windows_.empty());

  host_->OnWindowHasClosed();
  // Note: |this| and its host will be deleted here.
}

void BridgedNativeWidgetImpl::OnFullscreenTransitionStart(
    bool target_fullscreen_state) {
  // Note: This can fail for fullscreen changes started externally, but a user
  // shouldn't be able to do that if the window is invisible to begin with.
  DCHECK(window_visible_);

  DCHECK_NE(target_fullscreen_state, target_fullscreen_state_);
  target_fullscreen_state_ = target_fullscreen_state;
  in_fullscreen_transition_ = true;

  host_->OnWindowFullscreenTransitionStart(target_fullscreen_state);
}

void BridgedNativeWidgetImpl::OnFullscreenTransitionComplete(
    bool actual_fullscreen_state) {
  in_fullscreen_transition_ = false;

  if (target_fullscreen_state_ == actual_fullscreen_state) {
    host_->OnWindowFullscreenTransitionComplete(actual_fullscreen_state);
    return;
  }

  // The transition completed, but into the wrong state. This can happen when
  // there are calls to change the fullscreen state whilst mid-transition.
  // First update to reflect reality so that OnTargetFullscreenStateChanged()
  // expects the change.
  target_fullscreen_state_ = actual_fullscreen_state;
  ToggleDesiredFullscreenState(true /* async */);
}

void BridgedNativeWidgetImpl::ToggleDesiredFullscreenState(bool async) {
  // If there is currently an animation into or out of fullscreen, then AppKit
  // emits the string "not in fullscreen state" to stdio and does nothing. For
  // this case, schedule a transition back into the desired state when the
  // animation completes.
  if (in_fullscreen_transition_) {
    target_fullscreen_state_ = !target_fullscreen_state_;
    return;
  }

  // Going fullscreen implicitly makes the window visible. AppKit does this.
  // That is, -[NSWindow isVisible] is always true after a call to -[NSWindow
  // toggleFullScreen:]. Unfortunately, this change happens after AppKit calls
  // -[NSWindowDelegate windowWillEnterFullScreen:], and AppKit doesn't send an
  // orderWindow message. So intercepting the implicit change is hard.
  // Luckily, to trigger externally, the window typically needs to be visible in
  // the first place. So we can just ensure the window is visible here instead
  // of relying on AppKit to do it, and not worry that OnVisibilityChanged()
  // won't be called for externally triggered fullscreen requests.
  if (!window_visible_)
    SetVisibilityState(WindowVisibilityState::kShowInactive);

  // Enable fullscreen collection behavior because:
  // 1: -[NSWindow toggleFullscreen:] would otherwise be ignored,
  // 2: the fullscreen button must be enabled so the user can leave fullscreen.
  // This will be reset when a transition out of fullscreen completes.
  gfx::SetNSWindowCanFullscreen(window_, true);

  // Until 10.13, AppKit would obey a call to -toggleFullScreen: made inside
  // OnFullscreenTransitionComplete(). Starting in 10.13, it behaves as though
  // the transition is still in progress and just emits "not in a fullscreen
  // state" when trying to exit fullscreen in the same runloop that entered it.
  // To handle this case, invoke -toggleFullScreen: asynchronously.
  if (async) {
    [window_ performSelector:@selector(toggleFullScreen:)
                  withObject:nil
                  afterDelay:0];
  } else {
    // Suppress synchronous CA transactions during AppKit fullscreen transition
    // since there is no need for updates during such transition.
    // Re-layout and re-paint will be done after the transtion. See
    // https://crbug.com/875707 for potiential problems if we don't suppress.
    // |ca_transaction_sync_suppressed_| will be reset to false when the next
    // frame comes in.
    ca_transaction_sync_suppressed_ = true;
    [window_ toggleFullScreen:nil];
  }
}

void BridgedNativeWidgetImpl::OnSizeChanged() {
  UpdateWindowGeometry();
}

void BridgedNativeWidgetImpl::OnPositionChanged() {
  UpdateWindowGeometry();
}

void BridgedNativeWidgetImpl::OnVisibilityChanged() {
  const bool window_visible = [window_ isVisible];
  if (window_visible_ == window_visible)
    return;

  window_visible_ = window_visible;

  // If arriving via SetVisible(), |wants_to_be_visible_| should already be set.
  // If made visible externally (e.g. Cmd+H), just roll with it. Don't try (yet)
  // to distinguish being *hidden* externally from being hidden by a parent
  // window - we might not need that.
  if (window_visible_) {
    wants_to_be_visible_ = true;

    // Sheets don't need a parentWindow set, and setting one causes graphical
    // glitches (http://crbug.com/605098).
    if (parent_ && ![window_ isSheet])
      [parent_->ns_window() addChildWindow:window_ ordered:NSWindowAbove];
  } else {
    ReleaseCapture();  // Capture on hidden windows is not permitted.

    // When becoming invisible, remove the entry in any parent's childWindow
    // list. Cocoa's childWindow management breaks down when child windows are
    // hidden.
    if (parent_)
      [parent_->ns_window() removeChildWindow:window_];
  }

  // Showing a translucent window after hiding it should trigger shadow
  // invalidation.
  if (window_visible && ![window_ isOpaque])
    invalidate_shadow_on_frame_swap_ = true;

  NotifyVisibilityChangeDown();
  host_->OnVisibilityChanged(window_visible_);

  // Toolkit-views suppresses redraws while not visible. To prevent Cocoa asking
  // for an "empty" draw, disable auto-display while hidden. For example, this
  // prevents Cocoa drawing just *after* a minimize, resulting in a blank window
  // represented in the deminiaturize animation.
  [window_ setAutodisplay:window_visible_];
}

void BridgedNativeWidgetImpl::OnSystemControlTintChanged() {
  host_->OnWindowNativeThemeChanged();
}

void BridgedNativeWidgetImpl::OnBackingPropertiesChanged() {
  UpdateWindowDisplay();
}

void BridgedNativeWidgetImpl::OnWindowKeyStatusChangedTo(bool is_key) {
  host_->OnWindowKeyStatusChanged(
      is_key, [window_ contentView] == [window_ firstResponder],
      [NSApp isFullKeyboardAccessEnabled]);
}

void BridgedNativeWidgetImpl::SetSizeConstraints(const gfx::Size& min_size,
                                                 const gfx::Size& max_size,
                                                 bool is_resizable,
                                                 bool is_maximizable) {
  // Don't modify the size constraints or fullscreen collection behavior while
  // in fullscreen or during a transition. OnFullscreenTransitionComplete will
  // reset these after leaving fullscreen.
  if (target_fullscreen_state_ || in_fullscreen_transition_)
    return;

  bool shows_resize_controls =
      is_resizable && (min_size.IsEmpty() || min_size != max_size);
  bool shows_fullscreen_controls = is_resizable && is_maximizable;

  gfx::ApplyNSWindowSizeConstraints(window_, min_size, max_size,
                                    shows_resize_controls,
                                    shows_fullscreen_controls);
}

void BridgedNativeWidgetImpl::OnShowAnimationComplete() {
  show_animation_.reset();
}

void BridgedNativeWidgetImpl::InitCompositorView() {
  // Use the regular window background for window modal sheets. The layer will
  // still paint over most of it, but the native -[NSApp beginSheet:] animation
  // blocks the UI thread, so there's no way to invalidate the shadow to match
  // the composited layer. This assumes the native window shape is a good match
  // for the composited NonClientFrameView, which should be the case since the
  // native shape is what's most appropriate for displaying sheets on Mac.
  if (is_translucent_window_ && !IsWindowModalSheet()) {
    [window_ setOpaque:NO];
    [window_ setBackgroundColor:[NSColor clearColor]];

    // Don't block waiting for the initial frame of completely transparent
    // windows. This allows us to avoid blocking on the UI thread e.g, while
    // typing in the omnibox. Note window modal sheets _must_ wait: there is no
    // way for a frame to arrive during AppKit's sheet animation.
    // https://crbug.com/712268
    ca_transaction_sync_suppressed_ = true;
  } else {
    DCHECK(!ca_transaction_sync_suppressed_);
  }

  // Send the initial window geometry and screen properties. Any future changes
  // will be forwarded.
  UpdateWindowDisplay();
  UpdateWindowGeometry();
}

void BridgedNativeWidgetImpl::SortSubviews(RankMap rank) {
  // Ignore layer manipulation during a Close(). This can be reached during the
  // orderOut: in Close(), which notifies visibility changes to Views.
  if (!bridged_view_)
    return;
  [bridged_view_ sortSubviewsUsingFunction:&SubviewSorter context:&rank];
}

void BridgedNativeWidgetImpl::SetAnimationEnabled(bool animate) {
  [window_
      setAnimationBehavior:(animate ? NSWindowAnimationBehaviorDocumentWindow
                                    : NSWindowAnimationBehaviorNone)];
}

bool BridgedNativeWidgetImpl::ShouldRunCustomAnimationFor(
    VisibilityTransition transition) const {
  // The logic around this needs to change if new transition types are set.
  // E.g. it would be nice to distinguish "hide" from "close". Mac currently
  // treats "hide" only as "close". Hide (e.g. Cmd+h) should not animate on Mac.
  if (transitions_to_animate_ != transition &&
      transitions_to_animate_ != VisibilityTransition::kBoth) {
    return false;
  }

  // Custom animations are only used for tab-modals.
  bool widget_is_modal = false;
  host_->GetWidgetIsModal(&widget_is_modal);
  if (!widget_is_modal)
    return false;

  // Note this also checks the native animation property. Clearing that will
  // also disable custom animations to ensure that the views::Widget API
  // behaves consistently.
  if ([window_ animationBehavior] == NSWindowAnimationBehaviorNone)
    return false;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableModalAnimations)) {
    return false;
  }

  return true;
}

bool BridgedNativeWidgetImpl::RedispatchKeyEvent(NSEvent* event) {
  NSWindow* window = ns_window();
  DCHECK([window.class conformsToProtocol:@protocol(CommandDispatchingWindow)]);
  NSObject<CommandDispatchingWindow>* command_dispatching_window =
      base::mac::ObjCCastStrict<NSObject<CommandDispatchingWindow>>(window);
  return
      [[command_dispatching_window commandDispatcher] redispatchKeyEvent:event];
}

NSWindow* BridgedNativeWidgetImpl::ns_window() {
  return window_.get();
}

views_bridge_mac::DragDropClient* BridgedNativeWidgetImpl::drag_drop_client() {
  return host_helper_->GetDragDropClient();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetImpl, ui::CATransactionObserver

void BridgedNativeWidgetImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  UpdateWindowDisplay();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetImpl, ui::CATransactionObserver

bool BridgedNativeWidgetImpl::ShouldWaitInPreCommit() {
  if (!window_visible_)
    return false;
  if (ca_transaction_sync_suppressed_)
    return false;
  if (!bridged_view_)
    return false;
  return content_dip_size_ != compositor_frame_dip_size_;
}

base::TimeDelta BridgedNativeWidgetImpl::PreCommitTimeout() {
  return kUIPaintTimeout;
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetImpl, CocoaMouseCaptureDelegate:

void BridgedNativeWidgetImpl::PostCapturedEvent(NSEvent* event) {
  [bridged_view_ processCapturedMouseEvent:event];
}

void BridgedNativeWidgetImpl::OnMouseCaptureLost() {
  host_->OnMouseCaptureActiveChanged(false);
}

NSWindow* BridgedNativeWidgetImpl::GetWindow() const {
  return window_;
}

////////////////////////////////////////////////////////////////////////////////
// TODO(ccameron): Update class names to:
// BridgedNativeWidgetImpl, BridgedNativeWidgetImpl:

void BridgedNativeWidgetImpl::SetVisibleOnAllSpaces(bool always_visible) {
  gfx::SetNSWindowVisibleOnAllWorkspaces(window_, always_visible);
}

void BridgedNativeWidgetImpl::SetFullscreen(bool fullscreen) {
  if (fullscreen == target_fullscreen_state_)
    return;
  ToggleDesiredFullscreenState();
}

void BridgedNativeWidgetImpl::SetMiniaturized(bool miniaturized) {
  if (miniaturized) {
    // Calling performMiniaturize: will momentarily highlight the button, but
    // AppKit will reject it if there is no miniaturize button.
    if ([window_ styleMask] & NSMiniaturizableWindowMask)
      [window_ performMiniaturize:nil];
    else
      [window_ miniaturize:nil];
  } else {
    [window_ deminiaturize:nil];
  }
}

void BridgedNativeWidgetImpl::SetOpacity(float opacity) {
  [window_ setAlphaValue:opacity];
}

void BridgedNativeWidgetImpl::SetContentAspectRatio(
    const gfx::SizeF& aspect_ratio) {
  [window_ setContentAspectRatio:NSMakeSize(aspect_ratio.width(),
                                            aspect_ratio.height())];
}

void BridgedNativeWidgetImpl::SetCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  // Ignore frames arriving "late" for an old size. A frame at the new size
  // should arrive soon.
  gfx::Size frame_dip_size = gfx::ConvertSizeToDIP(ca_layer_params.scale_factor,
                                                   ca_layer_params.pixel_size);
  if (content_dip_size_ != frame_dip_size)
    return;
  compositor_frame_dip_size_ = frame_dip_size;

  // Update the DisplayCALayerTree with the most recent CALayerParams, to make
  // the content display on-screen.
  display_ca_layer_tree_->UpdateCALayerTree(ca_layer_params);

  if (ca_transaction_sync_suppressed_)
    ca_transaction_sync_suppressed_ = false;

  if (invalidate_shadow_on_frame_swap_) {
    invalidate_shadow_on_frame_swap_ = false;
    [window_ invalidateShadow];
  }
}

void BridgedNativeWidgetImpl::MakeFirstResponder() {
  [window_ makeFirstResponder:bridged_view_];
}

void BridgedNativeWidgetImpl::SetWindowTitle(const base::string16& title) {
  NSString* new_title = base::SysUTF16ToNSString(title);
  [window_ setTitle:new_title];
}

void BridgedNativeWidgetImpl::ClearTouchBar() {
  if (@available(macOS 10.12.2, *)) {
    if ([bridged_view_ respondsToSelector:@selector(setTouchBar:)])
      [bridged_view_ setTouchBar:nil];
  }
}

void BridgedNativeWidgetImpl::UpdateTooltip() {
  NSPoint nspoint =
      ui::ConvertPointFromScreenToWindow(window_, [NSEvent mouseLocation]);
  // Note: flip in the view's frame, which matches the window's contentRect.
  gfx::Point point(nspoint.x, NSHeight([bridged_view_ frame]) - nspoint.y);
  [bridged_view_ updateTooltipIfRequiredAt:point];
}

void BridgedNativeWidgetImpl::SetTextInputClient(
    ui::TextInputClient* text_input_client) {
  [bridged_view_ setTextInputClient:text_input_client];
}

void BridgedNativeWidgetImpl::RedispatchKeyEvent(
    uint64_t type,
    uint64_t modifier_flags,
    double timestamp,
    const base::string16& characters,
    const base::string16& characters_ignoring_modifiers,
    uint32_t key_code) {
  NSEvent* event =
      [NSEvent keyEventWithType:static_cast<NSEventType>(type)
                             location:NSZeroPoint
                        modifierFlags:modifier_flags
                            timestamp:timestamp
                         windowNumber:[window_ windowNumber]
                              context:nil
                           characters:base::SysUTF16ToNSString(characters)
          charactersIgnoringModifiers:base::SysUTF16ToNSString(
                                          characters_ignoring_modifiers)
                            isARepeat:NO
                              keyCode:key_code];
  RedispatchKeyEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetImpl, former BridgedNativeWidgetOwner:

gfx::Vector2d BridgedNativeWidgetImpl::GetChildWindowOffset() const {
  return gfx::ScreenRectFromNSRect([window_ frame]).OffsetFromOrigin();
}

void BridgedNativeWidgetImpl::RemoveChildWindow(
    BridgedNativeWidgetImpl* child) {
  auto location =
      std::find(child_windows_.begin(), child_windows_.end(), child);
  DCHECK(location != child_windows_.end());
  child_windows_.erase(location);

  // Note the child is sometimes removed already by AppKit. This depends on OS
  // version, and possibly some unpredictable reference counting. Removing it
  // here should be safe regardless.
  [window_ removeChildWindow:child->window_];
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetImpl, private:

void BridgedNativeWidgetImpl::RemoveOrDestroyChildren() {
  // TODO(tapted): Implement unowned child windows if required.
  while (!child_windows_.empty()) {
    // The NSWindow can only be destroyed after -[NSWindow close] is complete.
    // Retain the window, otherwise the reference count can reach zero when the
    // child calls back into RemoveChildWindow() via its OnWindowWillClose().
    base::scoped_nsobject<NSWindow> child(
        [child_windows_.back()->ns_window() retain]);
    [child close];
  }
}

void BridgedNativeWidgetImpl::NotifyVisibilityChangeDown() {
  // Child windows sometimes like to close themselves in response to visibility
  // changes. That's supported, but only with the asynchronous Widget::Close().
  // Perform a heuristic to detect child removal that would break these loops.
  const size_t child_count = child_windows_.size();
  if (!window_visible_) {
    for (BridgedNativeWidgetImpl* child : child_windows_) {
      if (child->window_visible_)
        [child->ns_window() orderOut:nil];

      DCHECK(!child->window_visible_);
      CHECK_EQ(child_count, child_windows_.size());
    }
    // The orderOut calls above should result in a call to OnVisibilityChanged()
    // in each child. There, children will remove themselves from the NSWindow
    // childWindow list as well as propagate NotifyVisibilityChangeDown() calls
    // to any children of their own. However this is only true for windows
    // managed by the BridgedNativeWidgetImpl i.e. windows which have
    // ViewsNSWindowDelegate as the delegate.
    DCHECK_EQ(0u, CountBridgedWindows([window_ childWindows]));
    return;
  }

  NSUInteger visible_bridged_children = 0;  // For a DCHECK below.
  NSInteger parent_window_number = [window_ windowNumber];
  for (BridgedNativeWidgetImpl* child : child_windows_) {
    // Note: order the child windows on top, regardless of whether or not they
    // are currently visible. They probably aren't, since the parent was hidden
    // prior to this, but they could have been made visible in other ways.
    if (child->wants_to_be_visible_) {
      ++visible_bridged_children;
      // Here -[NSWindow orderWindow:relativeTo:] is used to put the window on
      // screen. However, that by itself is insufficient to guarantee a correct
      // z-order relationship. If this function is being called from a z-order
      // change in the parent, orderWindow turns out to be unreliable (i.e. the
      // ordering doesn't always take effect). What this actually relies on is
      // the resulting call to OnVisibilityChanged() in the child, which will
      // then insert itself into -[NSWindow childWindows] to let Cocoa do its
      // internal layering magic.
      [child->ns_window() orderWindow:NSWindowAbove
                           relativeTo:parent_window_number];
      DCHECK(child->window_visible_);
    }
    CHECK_EQ(child_count, child_windows_.size());
  }
  DCHECK_EQ(visible_bridged_children,
            CountBridgedWindows([window_ childWindows]));
}

void BridgedNativeWidgetImpl::UpdateWindowGeometry() {
  gfx::Rect window_in_screen = gfx::ScreenRectFromNSRect([window_ frame]);
  gfx::Rect content_in_screen = gfx::ScreenRectFromNSRect(
      [window_ contentRectForFrameRect:[window_ frame]]);
  bool content_resized = content_dip_size_ != content_in_screen.size();
  content_dip_size_ = content_in_screen.size();

  host_->OnWindowGeometryChanged(window_in_screen, content_in_screen);

  if (content_resized && !ca_transaction_sync_suppressed_)
    ui::CATransactionCoordinator::Get().Synchronize();

  // For a translucent window, the shadow calculation needs to be carried out
  // after the frame from the compositor arrives.
  if (content_resized && ![window_ isOpaque])
    invalidate_shadow_on_frame_swap_ = true;
}

void BridgedNativeWidgetImpl::UpdateWindowDisplay() {
  host_->OnWindowDisplayChanged(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_.get()));
}

bool BridgedNativeWidgetImpl::IsWindowModalSheet() const {
  return parent_ && modal_type_ == ui::MODAL_TYPE_WINDOW;
}

void BridgedNativeWidgetImpl::ShowAsModalSheet() {
  // -[NSApp beginSheet:] will block the UI thread while the animation runs.
  // So that it doesn't animate a fully transparent window, first wait for a
  // frame. The first step is to pretend that the window is already visible.
  window_visible_ = true;
  host_->OnVisibilityChanged(window_visible_);

  NSWindow* parent_window = parent_->ns_window();
  DCHECK(parent_window);

  // -beginSheet: does not retain |modalDelegate| (and we would not want it to).
  // Since |this| may destroy [window_ delegate], use |window_| itself as the
  // delegate, which will forward to ViewsNSWindowDelegate if |this| is still
  // alive (i.e. it has not set the window delegate to nil).
  [NSApp beginSheet:window_
      modalForWindow:parent_window
       modalDelegate:window_
      didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:)
         contextInfo:nullptr];
}

}  // namespace views
