// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "ui/views/animation/ink_drop_animation_ended_reason.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_state.h"

namespace views {

void PrintTo(InkDropState ink_drop_state, ::std::ostream* os) {
  *os << ToString(ink_drop_state);
}

void PrintTo(InkDropHighlight::AnimationType animation_type,
             ::std::ostream* os) {
  *os << ToString(animation_type);
}

void PrintTo(InkDropAnimationEndedReason reason, ::std::ostream* os) {
  *os << ToString(reason);
}

}  // namespace views
