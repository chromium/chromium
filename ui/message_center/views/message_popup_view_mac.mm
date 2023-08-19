// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/views/message_popup_view.h"

#import <Cocoa/Cocoa.h>

#include "ui/views/widget/widget.h"

namespace message_center {

float MessagePopupView::GetOpacity() const {
  if (!IsWidgetValid()) {
    return 0.f;
  }
  return GetWidget()->GetNativeWindow().GetNativeNSWindow().alphaValue;
}

}  // namespace message_center
