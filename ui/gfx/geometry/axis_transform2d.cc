// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/axis_transform2d.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/decomposed_transform.h"

namespace gfx {

DecomposedTransform AxisTransform2d::Decompose() const {
  DecomposedTransform decomp;

  decomp.translate[0] = translation_.x();
  decomp.translate[1] = translation_.y();

  if (scale_.x() >= 0 || scale_.y() >= 0) {
    decomp.scale[0] = scale_.x();
    decomp.scale[1] = scale_.y();
  } else {
    // If both scales are negative, decompose to positive scales with a 180deg
    // rotation.
    decomp.scale[0] = -scale_.x();
    decomp.scale[1] = -scale_.y();
    decomp.quaternion.set_z(1);
    decomp.quaternion.set_w(0);
  }
  return decomp;
}

std::string AxisTransform2d::ToString() const {
  return base::StringPrintf("[%s, %s]", scale_.ToString().c_str(),
                            translation_.ToString().c_str());
}

}  // namespace gfx
