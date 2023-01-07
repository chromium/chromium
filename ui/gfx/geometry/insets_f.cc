// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets_f.h"

#include "ui/gfx/geometry/outsets_f.h"

namespace gfx {

OutsetsF InsetsF::ToOutsets() const {
  // Conversion from InsetsF to OutsetsF negates all components.
  return OutsetsF()
      .set_left(-left())
      .set_right(-right())
      .set_top(-top())
      .set_bottom(-bottom());
}

}  // namespace gfx
