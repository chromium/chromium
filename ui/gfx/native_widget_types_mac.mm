// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_widget_types.h"

#include <AppKit/AppKit.h>

#include "base/strings/sys_string_conversions.h"

namespace gfx {

std::string NativeView::ToString() const {
  return base::SysNSStringToUTF8(ns_view_.description);
}

std::string NativeWindow::ToString() const {
  return base::SysNSStringToUTF8(ns_window_.description);
}

}  // namespace gfx
