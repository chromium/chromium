// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/scoped_set_idle_state.h"

#include "ui/base/idle/idle_internal.h"

namespace ui {

ScopedSetIdleState::ScopedSetIdleState(IdleState state)
    : previous_state_(IdleStateForTesting()) {
  IdleStateForTesting() = state;
}

ScopedSetIdleState::~ScopedSetIdleState() {
  IdleStateForTesting() = previous_state_;
}

}  // namespace ui
