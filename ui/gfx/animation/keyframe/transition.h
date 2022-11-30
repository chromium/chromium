// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_TRANSITION_H_
#define UI_GFX_ANIMATION_KEYFRAME_TRANSITION_H_

#include <set>

#include "base/time/time.h"
#include "ui/gfx/animation/keyframe/keyframe_animation_export.h"

namespace gfx {

struct GFX_KEYFRAME_ANIMATION_EXPORT Transition {
  Transition();
  Transition(const Transition&);
  Transition(Transition&&);
  ~Transition();

  Transition& operator=(const Transition&) = default;
  Transition& operator=(Transition&&) = default;

  base::TimeDelta duration;
  std::set<int> target_properties;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_TRANSITION_H_
