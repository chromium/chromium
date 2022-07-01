// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation_settings_provider_linux.h"

namespace gfx {

// static
AnimationSettingsProviderLinux* AnimationSettingsProviderLinux::instance_ =
    nullptr;

// static
AnimationSettingsProviderLinux* AnimationSettingsProviderLinux::GetInstance() {
  return instance_;
}

// static
void AnimationSettingsProviderLinux::SetInstance(
    AnimationSettingsProviderLinux* instance) {
  instance_ = instance;
}

AnimationSettingsProviderLinux::AnimationSettingsProviderLinux() = default;

AnimationSettingsProviderLinux::~AnimationSettingsProviderLinux() = default;

}  // namespace gfx
