// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#include "base/notreached.h"

namespace display {

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView view) {
  return view;
}

Screen* CreateNativeScreen() {
  NOTREACHED() << "Implementation should be installed at higher level.";
  return NULL;
}

}  // namespace display
