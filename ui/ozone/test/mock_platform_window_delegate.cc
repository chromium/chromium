// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/test/mock_platform_window_delegate.h"

namespace ui {

MockPlatformWindowDelegate::MockPlatformWindowDelegate() {}

MockPlatformWindowDelegate::~MockPlatformWindowDelegate() {}

bool operator==(const PlatformWindowDelegate::BoundsChange& a,
                const PlatformWindowDelegate::BoundsChange& b) {
  return a.origin_changed == b.origin_changed;
}

}  // namespace ui
