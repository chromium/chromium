// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_ended_reason.h"

#include <ostream>

#include "base/notreached.h"

namespace views {

std::string ToString(InkDropAnimationEndedReason reason) {
  switch (reason) {
    case InkDropAnimationEndedReason::SUCCESS:
      return "SUCCESS";
    case InkDropAnimationEndedReason::PRE_EMPTED:
      return "PRE_EMPTED";
  }
  NOTREACHED();
}

}  // namespace views
