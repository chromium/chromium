//, Copyright 2023 The Chromium Authors
//, Use of this source code is governed by a BSD-style license that can be
//, found in the LICENSE file.

#include "ui/views/test/mock_native_widget.h"

using testing::Return;

namespace views {

MockNativeWidget::MockNativeWidget(Widget* widget) : widget_(widget) {}

MockNativeWidget::~MockNativeWidget() {
  widget_->OnNativeWidgetDestroyed();
}

base::WeakPtr<internal::NativeWidgetPrivate> MockNativeWidget::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace views
