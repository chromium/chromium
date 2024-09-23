// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/scoped_animation_disabler.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/base/class_property.h"

namespace wm {

ScopedAnimationDisabler::ScopedAnimationDisabler(aura::Window* window)
    : window_(window) {
  DCHECK(window_);
  was_animation_enabled_ =
      !window_->GetProperty(aura::client::kAnimationsDisabledKey);
  if (was_animation_enabled_)
    window_->SetProperty(aura::client::kAnimationsDisabledKey, true);
}

ScopedAnimationDisabler::~ScopedAnimationDisabler() {
  if (was_animation_enabled_) {
    DCHECK_EQ(window_->GetProperty(aura::client::kAnimationsDisabledKey), true);
    window_->ClearProperty(aura::client::kAnimationsDisabledKey);
  }
}

}  // namespace wm
