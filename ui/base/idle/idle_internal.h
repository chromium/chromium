// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_IDLE_INTERNAL_H_
#define UI_BASE_IDLE_IDLE_INTERNAL_H_

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/idle/idle.h"

namespace ui {

// An optional idle state set by tests via a ScopedSetIdleState to override the
// actual idle state of the system.
COMPONENT_EXPORT(UI_BASE_IDLE) absl::optional<IdleState>& IdleStateForTesting();

}  // namespace ui

#endif  // UI_BASE_IDLE_IDLE_INTERNAL_H_
