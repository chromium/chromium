// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_fullscreen_behavior.h"

#include "ui/aura/window.h"

namespace keyboard {

ContainerFullscreenBehavior::ContainerFullscreenBehavior(Delegate* delegate)
    : ContainerFullWidthBehavior(delegate) {}

ContainerFullscreenBehavior::~ContainerFullscreenBehavior() {}

gfx::Rect ContainerFullscreenBehavior::AdjustSetBoundsRequest(
    const gfx::Rect& display_bounds,
    const gfx::Rect& requested_bounds_in_screen_coords) {
  return display_bounds;
}

void ContainerFullscreenBehavior::SetCanonicalBounds(
    aura::Window* container,
    const gfx::Rect& display_bounds) {
  container->SetBounds(display_bounds);
}

gfx::Rect ContainerFullscreenBehavior::GetOccludedBounds(
    const gfx::Rect& visual_bounds_in_screen) const {
  return occluded_bounds_;
}

ContainerType ContainerFullscreenBehavior::GetType() const {
  return ContainerType::FULLSCREEN;
}

void ContainerFullscreenBehavior::SetOccludedBounds(
    const gfx::Rect& occluded_bounds) {
  occluded_bounds_ = occluded_bounds;
}

}  //  namespace keyboard
