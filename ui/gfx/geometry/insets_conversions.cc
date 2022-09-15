// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets_conversions.h"

#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"

namespace gfx {

Insets ToFlooredInsets(const InsetsF& insets) {
  return Insets::TLBR(
      base::ClampFloor(insets.top()), base::ClampFloor(insets.left()),
      base::ClampFloor(insets.bottom()), base::ClampFloor(insets.right()));
}

Insets ToCeiledInsets(const InsetsF& insets) {
  return Insets::TLBR(
      base::ClampCeil(insets.top()), base::ClampCeil(insets.left()),
      base::ClampCeil(insets.bottom()), base::ClampCeil(insets.right()));
}

Insets ToRoundedInsets(const InsetsF& insets) {
  return Insets::TLBR(
      base::ClampRound(insets.top()), base::ClampRound(insets.left()),
      base::ClampRound(insets.bottom()), base::ClampRound(insets.right()));
}

}  // namespace gfx
