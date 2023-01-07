// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/renderer_base.h"

#include "base/numerics/math_constants.h"

namespace ui {

namespace {
const int kAnimationSteps = 240;
}  // namespace

RendererBase::RendererBase(gfx::AcceleratedWidget widget, const gfx::Size& size)
    : widget_(widget), size_(size) {
}

RendererBase::~RendererBase() {
}

float RendererBase::CurrentFraction() const {
  float fraction =
      (sinf(iteration_ * 2 * base::kPiFloat / kAnimationSteps) + 1) / 2;
  return fraction;
}

float RendererBase::NextFraction() {
  float fraction = CurrentFraction();
  iteration_++;
  iteration_ %= kAnimationSteps;
  return fraction;
}

}  // namespace ui
