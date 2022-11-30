// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_delegate.h"

namespace aura {

bool WindowDelegate::RequiresDoubleTapGestureEvents() const {
  return false;
}

}  // namespace aura
