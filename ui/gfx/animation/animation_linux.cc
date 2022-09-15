// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#include "ui/linux/linux_ui.h"

namespace gfx {

namespace {

// Linux toolkits only have a global setting for whether animations should be
// enabled.  So use it for all of the specific settings that Chrome needs.
bool AnimationsEnabled() {
  auto* linux_ui = ui::LinuxUi::instance();
  return !linux_ui || linux_ui->AnimationsEnabled();
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
