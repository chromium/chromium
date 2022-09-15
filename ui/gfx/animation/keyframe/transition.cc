// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/transition.h"

namespace gfx {

namespace {
static constexpr int kDefaultTransitionDurationMs = 225;
}  // namespace

Transition::Transition()
    : duration(base::Milliseconds(kDefaultTransitionDurationMs)) {}

Transition::Transition(const Transition&) = default;
Transition::Transition(Transition&&) = default;
Transition::~Transition() = default;

}  // namespace gfx
