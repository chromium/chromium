// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_SCOPED_SET_IDLE_STATE_H_
#define UI_BASE_IDLE_SCOPED_SET_IDLE_STATE_H_

#include <optional>

#include "ui/base/idle/idle.h"

namespace ui {

class ScopedSetIdleState {
 public:
  explicit ScopedSetIdleState(IdleState state);

  ScopedSetIdleState(const ScopedSetIdleState&) = delete;
  ScopedSetIdleState& operator=(const ScopedSetIdleState&) = delete;

  ~ScopedSetIdleState();

 private:
  std::optional<IdleState> previous_state_;
};

}  // namespace ui

#endif  // UI_BASE_IDLE_SCOPED_SET_IDLE_STATE_H_