// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#import "ui/accessibility/platform/ax_platform_node_mac.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

void EnsureNativeViewHasNoChildWidgets(NSView* native_view) {
  DCHECK(native_view);
  // Mac's NativeViewHost has no support for hosting its own child widgets.
  // This check is probably overly restrictive, since the Widget containing the
  // NativeViewHost _is_ allowed child Widgets. However, we don't know yet
  // whether those child Widgets need to be distinguished from Widgets that code
  // might want to associate with the hosted NSView instead.
  {
    Widget::Widgets child_widgets;
    Widget::GetAllChildWidgets(native_view, &child_widgets);
    CHECK_GE(1u, child_widgets.size());  // 1 (itself) or 0 if detached.
  }
}

}  // namespace

NativeViewHostMac::NativeViewHostMac(NativeViewHost* host) : host_(host) {
  // Ensure that |host_| have its own ui::Layer and that it draw nothing.
  host_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
}

NativeViewHostMac::~NativeViewHostMac() {
}

NativeWidgetMacNSWindowHost* NativeViewHostMac::GetNSWindowHost() const {
  return NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      host_->GetWidget()->GetNativeWindow());
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostMac, ViewsHostableView::Host implementation:

ui::Layer* NativeViewHostMac::GetUiLayer() const {
  return host_->layer();
}

remote_cocoa::mojom::Application* NativeViewHostMac::GetRemoteCocoaApplication()
    const {
  if (auto* window_host = GetNSWindowHost()) {
    if (auto* application_host = window_host->application_host())
      return application_host->GetApplication();
  }
  return nullptr;
}

uint64_t NativeViewHostMac::GetNSViewId() const {
  auto* window_host = GetNSWindowHost();
  if (window_host)
    return window_host->GetRootViewNSViewId();
  return 0;
}

void NativeViewHostMac::OnHostableViewDestroying() {
  DCHECK(native_view_hostable_);
  host_->NativeViewDestroyed();
  DCHECK(!native_view_hostable_);
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostMac, NativeViewHostWrapper implementation:

void NativeViewHostMac::AttachNativeView() {
  DCHECK(host_->native_view());
  DCHECK(!native_view_);
  native_view_.reset([host_->native_view().GetNativeNSView() retain]);
  if ([native_view_ conformsToProtocol:@protocol(ViewsHostable)]) {
    id hostable = native_view_;
    native_view_hostable_ = [hostable viewsHostableView];
  }
  EnsureNativeViewHasNoChildWidgets(native_view_);

  auto* window_host = GetNSWindowHost();
  CHECK(window_host);

  // TODO(https://crbug.com/933679): This is lifted out the ViewsHostableAttach
  // call below because of crashes being observed in the field.
  NSView* superview =
      window_host->native_widget_mac()->GetNativeView().GetNativeNSView();
  [superview addSubview:native_view_];

  if (native_view_hostable_) {
    native_view_hostable_->ViewsHostableAttach(this);
    // Initially set the parent to match the views::Views parent. Note that this
    // may be overridden (e.g, by views::WebView).
    native_view_hostable_->ViewsHostableSetParentAccessible(
        host_->parent()->GetNativeViewAccessible());
  }

  window_host->OnNativeViewHostAttach(host_, native_view_);
}

void NativeViewHostMac::NativeViewDetaching(bool destroyed) {
  // |destroyed| is only true if this class calls host_->NativeViewDestroyed(),
  // which is called if a hosted WebContentsView about to be destroyed (note
  // that its corresponding NSView may still exist).

  // |native_view_| can be nil here if RemovedFromWidget() is called before
  // NativeViewHost::Detach().
  NSView* host_native_view = host_->native_view().GetNativeNSView();
  if (!native_view_) {
    DCHECK(![host_native_view superview]);
    return;
  }

  DCHECK(native_view_ == host_native_view);
  if (native_view_hostable_) {
    native_view_hostable_->ViewsHostableDetach();
    native_view_hostable_ = nullptr;
  } else {
    [native_view_ setHidden:YES];
    [native_view_ removeFromSuperview];
  }

  EnsureNativeViewHasNoChildWidgets(native_view_);
  auto* window_host = GetNSWindowHost();
  // NativeWidgetNSWindowBridge can be null when Widget is closing.
  if (window_host)
    window_host->OnNativeViewHostDetach(host_);

  native_view_.reset();
}

void NativeViewHostMac::AddedToWidget() {
  if (!host_->native_view())
    return;

  AttachNativeView();
  host_->Layout();
}

void NativeViewHostMac::RemovedFromWidget() {
  if (!host_->native_view())
    return;

  NativeViewDetaching(false);
}

bool NativeViewHostMac::SetCustomMask(std::unique_ptr<ui::LayerOwner> mask) {
  NOTIMPLEMENTED();
  return false;
}

void NativeViewHostMac::SetHitTestTopInset(int top_inset) {
  NOTIMPLEMENTED();
}

int NativeViewHostMac::GetHitTestTopInset() const {
  NOTIMPLEMENTED();
  return 0;
}

void NativeViewHostMac::InstallClip(int x, int y, int w, int h) {
  NOTIMPLEMENTED();
}

bool NativeViewHostMac::HasInstalledClip() {
  return false;
}

void NativeViewHostMac::UninstallClip() {
  NOTIMPLEMENTED();
}

void NativeViewHostMac::ShowWidget(int x,
                                   int y,
                                   int w,
                                   int h,
                                   int native_w,
                                   int native_h) {
  // TODO(https://crbug.com/415024): Implement host_->fast_resize().

  if (native_view_hostable_) {
    native_view_hostable_->ViewsHostableSetBounds(gfx::Rect(x, y, w, h));
    native_view_hostable_->ViewsHostableSetVisible(true);
  } else {
    // Coordinates will be from the top left of the parent Widget. The
    // NativeView is already in the same NSWindow, so just flip to get Cooca
    // coordinates and then convert to the containing view.
    NSRect window_rect = NSMakeRect(
        x, host_->GetWidget()->GetClientAreaBoundsInScreen().height() - y - h,
        w, h);

    // Convert window coordinates to the hosted view's superview, since that's
    // how coordinates of the hosted view's frame is based.
    NSRect container_rect = [[native_view_ superview] convertRect:window_rect
                                                         fromView:nil];
    [native_view_ setFrame:container_rect];
    [native_view_ setHidden:NO];
  }
}

void NativeViewHostMac::HideWidget() {
  if (native_view_hostable_)
    native_view_hostable_->ViewsHostableSetVisible(false);
  else
    [native_view_ setHidden:YES];
}

void NativeViewHostMac::SetFocus() {
  if (native_view_hostable_) {
    native_view_hostable_->ViewsHostableMakeFirstResponder();
  } else {
    if ([native_view_ acceptsFirstResponder])
      [[native_view_ window] makeFirstResponder:native_view_];
  }
}

gfx::NativeView NativeViewHostMac::GetNativeViewContainer() const {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::NativeViewAccessible NativeViewHostMac::GetNativeViewAccessible() {
  if (native_view_hostable_)
    return native_view_hostable_->ViewsHostableGetAccessibilityElement();
  else
    return native_view_;
}

gfx::NativeCursor NativeViewHostMac::GetCursor(int x, int y) {
  // Intentionally not implemented: Not required on non-aura Mac because OSX
  // will query the native view for the cursor directly. For NativeViewHostMac
  // in practice, OSX will retrieve the cursor that was last set by
  // -[RenderWidgetHostViewCocoa updateCursor:] whenever the pointer is over the
  // hosted view. With some plumbing, NativeViewHostMac could return that same
  // cursor here, but it doesn't achieve anything. The implications of returning
  // null simply mean that the "fallback" cursor on the window itself will be
  // cleared (see -[NativeWidgetMacNSWindow cursorUpdate:]). However, while the
  // pointer is over a RenderWidgetHostViewCocoa, OSX won't ask for the fallback
  // cursor.
  return gfx::kNullCursor;
}

void NativeViewHostMac::SetVisible(bool visible) {
  if (native_view_hostable_)
    native_view_hostable_->ViewsHostableSetVisible(visible);
  else
    [native_view_ setHidden:!visible];
}

void NativeViewHostMac::SetParentAccessible(
    gfx::NativeViewAccessible parent_accessibility_element) {
  if (native_view_hostable_) {
    native_view_hostable_->ViewsHostableSetParentAccessible(
        parent_accessibility_element);
  } else {
    // It is not easy to force a generic NSView to return a different
    // accessibility parent. Fortunately, this interface is only ever used
    // in practice to host a WebContentsView.
  }
}

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostMac(host);
}

}  // namespace views
