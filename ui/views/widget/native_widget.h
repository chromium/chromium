// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_H_

#include "ui/views/views_export.h"

namespace views {

class Widget;

namespace internal {
class NativeWidgetPrivate;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidget interface
//
//  An interface that serves as the public API base for the
//  internal::NativeWidget interface that Widget uses to communicate with a
//  backend-specific native widget implementation. This is the only component of
//  this interface that is publicly visible, and exists solely for exposure via
//  Widget's native_widget() accessor, which code occasionally static_casts to
//  a known implementation in platform-specific code.
//
class VIEWS_EXPORT NativeWidget {
 public:
  virtual ~NativeWidget() = default;

 private:
  friend class Widget;

  virtual internal::NativeWidgetPrivate* AsNativeWidgetPrivate() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_H_
