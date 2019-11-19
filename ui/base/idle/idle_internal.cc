// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle_internal.h"

#include "base/no_destructor.h"

namespace ui {

base::Optional<IdleState>& IdleStateForTesting() {
  static base::NoDestructor<base::Optional<IdleState>> idle_state;
  return *idle_state;
}

}  // namespace ui
