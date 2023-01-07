// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_LOCK_STATE_H_
#define UI_BASE_WIN_LOCK_STATE_H_

#include "base/component_export.h"

namespace ui {

// Returns true if the screen is currently locked.
COMPONENT_EXPORT(UI_BASE) bool IsWorkstationLocked();

}  // namespace ui

#endif  // UI_BASE_WIN_LOCK_STATE_H_
