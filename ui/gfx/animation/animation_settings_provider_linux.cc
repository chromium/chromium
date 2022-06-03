// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation_settings_provider_linux.h"

#include "base/check_op.h"

namespace gfx {

// static
AnimationSettingsProviderLinux* AnimationSettingsProviderLinux::instance_ =
    nullptr;

// static
AnimationSettingsProviderLinux* AnimationSettingsProviderLinux::GetInstance() {
  return instance_;
}

AnimationSettingsProviderLinux::AnimationSettingsProviderLinux() {
  DCHECK(!instance_);
  instance_ = this;
}

AnimationSettingsProviderLinux::~AnimationSettingsProviderLinux() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

}  // namespace gfx
