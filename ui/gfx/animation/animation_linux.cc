// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#include "ui/gfx/animation/animation_settings_provider_linux.h"

namespace gfx {

namespace {

// GTK only has a global setting for whether animations should be enabled.  So
// use it for all of the specific settings that Chrome needs.
bool AnimationsEnabled() {
  auto* provider = AnimationSettingsProviderLinux::GetInstance();
  return !provider || provider->AnimationsEnabled();
}

}  // namespace

// static
bool Animation::ShouldRenderRichAnimationImpl() {
  return AnimationsEnabled();
}

// static
bool Animation::ScrollAnimationsEnabledBySystem() {
  return AnimationsEnabled();
}

// static
void Animation::UpdatePrefersReducedMotion() {
  prefers_reduced_motion_ = !AnimationsEnabled();
}

}  // namespace gfx
