// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/axis_transform2d.h"

#include "base/strings/stringprintf.h"

namespace gfx {

std::string AxisTransform2d::ToString() const {
  return base::StringPrintf("[%s, %s]", scale_.ToString().c_str(),
                            translation_.ToString().c_str());
}

}  // namespace gfx