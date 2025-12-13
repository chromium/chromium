// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_observer.h"

#include "base/check.h"

namespace ui {

NativeThemeObserver::~NativeThemeObserver() {
  CHECK(!IsInObserverList());
}

}  // namespace ui
