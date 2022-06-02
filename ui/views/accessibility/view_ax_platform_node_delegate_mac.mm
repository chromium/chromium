// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_mac.h"

#include <memory>

#include "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  auto result = std::make_unique<ViewAXPlatformNodeDelegateMac>(view);
  result->Init();
  return result;
}

ViewAXPlatformNodeDelegateMac::ViewAXPlatformNodeDelegateMac(View* view)
    : ViewAXPlatformNodeDelegate(view) {}

ViewAXPlatformNodeDelegateMac::~ViewAXPlatformNodeDelegateMac() = default;

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateMac::GetNSWindow() {
  auto* widget = view()->GetWidget();
  if (!widget)
    return nil;

  auto* window_host = NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      widget->GetNativeWindow());
  if (!window_host)
    return nil;

  return window_host->GetNativeViewAccessibleForNSWindow();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateMac::GetParent() const {
  if (view()->parent())
    return ViewAXPlatformNodeDelegate::GetParent();

  auto* widget = view()->GetWidget();
  if (!widget)
    return nil;

  auto* window_host = NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      view()->GetWidget()->GetNativeWindow());
  if (!window_host)
    return nil;

  return window_host->GetNativeViewAccessibleForNSView();
}

const std::string& ViewAXPlatformNodeDelegateMac::GetName() const {
  // By default, the kDialog name is the title of the window. NSAccessibility
  // then applies that name to the native NSWindow. This causes VoiceOver to
  // double-speak the name. In the case of some dialogs, such as the one
  // associated with a JavaScript alert, we set the accessible description
  // to the message contents. For screen readers which prefer the description
  // over the displayed text, this causes both the title and message to be
  // presented to the user. At the present time, VoiceOver is not one of those
  // screen readers. Therefore if we have a dialog whose name is the same as
  // the window title, and we also have an explicitly-provided description, set
  // the name of the dialog to that description. This causes VoiceOver to read
  // both the title and the displayed text. Note that in order for this to
  // work, it is necessary for the View to also call OverrideNativeWindowTitle.
  // Otherwise, NSAccessibility will set the window title to the message text.
  const std::string& name = ViewAXPlatformNodeDelegate::GetName();
  if (!ui::IsDialog(GetRole()) ||
      !HasStringAttribute(ax::mojom::StringAttribute::kDescription))
    return name;

  if (auto* widget = view()->GetWidget()) {
    if (auto* widget_delegate = widget->widget_delegate()) {
      if (base::UTF16ToUTF8(widget_delegate->GetWindowTitle()) == name)
        return GetStringAttribute(ax::mojom::StringAttribute::kDescription);
    }
  }
  return name;
}

void ViewAXPlatformNodeDelegateMac::OverrideNativeWindowTitle(
    const std::string& title) {
  if (gfx::NativeViewAccessible ax_window = GetNSWindow()) {
    [ax_window setAccessibilityLabel:base::SysUTF8ToNSString(title)];
  }
}

}  // namespace views
