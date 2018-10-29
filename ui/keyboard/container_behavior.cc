// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_behavior.h"

namespace keyboard {

ContainerBehavior::ContainerBehavior(Delegate* delegate)
    : delegate_(delegate) {}

ContainerBehavior::~ContainerBehavior() = default;

}  // namespace keyboard
