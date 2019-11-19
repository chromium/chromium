// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_SCOPED_SET_IDLE_STATE_H_
#define UI_BASE_IDLE_SCOPED_SET_IDLE_STATE_H_

#include "base/optional.h"
#include "ui/base/idle/idle.h"

namespace ui {

class ScopedSetIdleState {
 public:
  explicit ScopedSetIdleState(IdleState state);
  ~ScopedSetIdleState();

 private:
  base::Optional<IdleState> previous_state_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetIdleState);
};

}  // namespace ui

#endif  // UI_BASE_IDLE_SCOPED_SET_IDLE_STATE_H_