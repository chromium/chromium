// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#include "base/notreached.h"

namespace display {

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView view) {
  return view;
}

}  // namespace display
