// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_activation_delegate.h"

#include "base/check.h"
#include "base/check_op.h"

namespace views {
namespace {

WidgetActivationDelegate* widget_activation_delegate = nullptr;

}  // namespace

// static
WidgetActivationDelegate* WidgetActivationDelegate::Get() {
  return widget_activation_delegate;
}

WidgetActivationDelegate::WidgetActivationDelegate() {
  CHECK(!widget_activation_delegate);
  widget_activation_delegate = this;
}

WidgetActivationDelegate::~WidgetActivationDelegate() {
  CHECK_EQ(widget_activation_delegate, this);
  widget_activation_delegate = nullptr;
}

}  // namespace views
