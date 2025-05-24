// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node.h"

#include "base/debug/crash_logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/buildflags.h"

namespace ui {

AXPlatformNode::NativeWindowHandlerCallback&
GetNativeWindowHandlerCallbackValue() {
  static base::NoDestructor<AXPlatformNode::NativeWindowHandlerCallback>
      callback;
  return *callback;
}

// This allows UI menu popups like to act as if they are focused in the
// exposed platform accessibility API, even though actual focus remains in
// underlying content.
gfx::NativeViewAccessible& GetPopupFocusOverrideValue() {
#if BUILDFLAG(IS_APPLE)
  static base::NoDestructor<gfx::NativeViewAccessible> popup_focus_override;
  return *popup_focus_override;
#else
  static constinit gfx::NativeViewAccessible popup_focus_override =
      gfx::NativeViewAccessible();
  return popup_focus_override;
#endif
}

// static
bool AXPlatformNode::allow_ax_mode_changes_ = true;

// static
AXPlatformNode* AXPlatformNode::FromNativeWindow(
    gfx::NativeWindow native_window) {
  if (GetNativeWindowHandlerCallbackValue()) {
    return GetNativeWindowHandlerCallbackValue().Run(native_window);
  }
  return nullptr;
}

#if !BUILDFLAG(HAS_NATIVE_ACCESSIBILITY)
// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return nullptr;
}
#endif  // !BUILDFLAG(HAS_NATIVE_ACCESSIBILITY)

// static
void AXPlatformNode::RegisterNativeWindowHandler(
    AXPlatformNode::NativeWindowHandlerCallback handler) {
  GetNativeWindowHandlerCallbackValue() = handler;
}

// static
void AXPlatformNode::SetAXModeChangeAllowed(bool allow) {
  allow_ax_mode_changes_ = allow;
}

AXPlatformNodeId AXPlatformNode::GetUniqueId() const {
  // Must not be called before `Init()` or after `Destroy()`.
  return GetDelegate()->GetUniqueId();
}

std::string AXPlatformNode::ToString() const {
  // Must not be called before `Init()` or after `Destroy()`.
  return GetDelegate()->ToString();
}

std::string AXPlatformNode::SubtreeToString() const {
  // Must not be called before `Init()` or after `Destroy()`.
  return GetDelegate()->SubtreeToString();
}

std::ostream& operator<<(std::ostream& stream, AXPlatformNode& node) {
  return stream << node.ToString();
}

// static
void AXPlatformNode::SetPopupFocusOverride(
    gfx::NativeViewAccessible popup_focus_override) {
  GetPopupFocusOverrideValue() = popup_focus_override;
}

// static
gfx::NativeViewAccessible AXPlatformNode::GetPopupFocusOverride() {
  return GetPopupFocusOverrideValue();
}

}  // namespace ui
