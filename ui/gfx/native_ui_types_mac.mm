// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_ui_types.h"

#include <AppKit/AppKit.h>

#include "base/strings/sys_string_conversions.h"

namespace gfx {

NativeView::NativeView() = default;

NativeView::NativeView(NSView* ns_view) : base::apple::WeakNSView(ns_view) {}

NSView* NativeView::GetNativeNSView() const {
  return static_cast<const base::apple::WeakNSView*>(this)->Get();
}

std::string NativeView::ToString() const {
  return base::SysNSStringToUTF8(GetNativeNSView().description);
}

NativeWindow::NativeWindow() = default;

NativeWindow::NativeWindow(NSWindow* ns_window)
    : base::apple::WeakNSWindow(ns_window),
      pointer_bits_(reinterpret_cast<uintptr_t>(ns_window)) {}

NSWindow* NativeWindow::GetNativeNSWindow() const {
  return static_cast<const base::apple::WeakNSWindow*>(this)->Get();
}

bool NativeWindow::operator<(const NativeWindow& other) const {
  return pointer_bits_ < other.pointer_bits_;
}

std::string NativeWindow::ToString() const {
  return base::SysNSStringToUTF8(GetNativeNSWindow().description);
}

}  // namespace gfx
