// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_QUARTZ_UTIL_H_
#define UI_BASE_COCOA_QUARTZ_UTIL_H_

#include "base/component_export.h"

namespace ui {

// Calls +[CATransaction begin].
COMPONENT_EXPORT(UI_BASE) void BeginCATransaction();

// Calls +[CATransaction commit].
COMPONENT_EXPORT(UI_BASE) void CommitCATransaction();

}  // namespace ui

#endif  // UI_BASE_COCOA_QUARTZ_UTIL_H_
