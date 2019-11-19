// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RANGE_MOJOM_RANGE_MOJOM_TRAITS_H_
#define UI_GFX_RANGE_MOJOM_RANGE_MOJOM_TRAITS_H_

#include "ui/gfx/range/mojom/range.mojom-shared.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/range/range_f.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::RangeDataView, gfx::Range> {
  static uint32_t start(const gfx::Range& r) { return r.start(); }
  static uint32_t end(const gfx::Range& r) { return r.end(); }
  static bool Read(gfx::mojom::RangeDataView data, gfx::Range* out) {
    out->set_start(data.start());
    out->set_end(data.end());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::RangeFDataView, gfx::RangeF> {
  static float start(const gfx::RangeF& r) { return r.start(); }
  static float end(const gfx::RangeF& r) { return r.end(); }
  static bool Read(gfx::mojom::RangeFDataView data, gfx::RangeF* out) {
    out->set_start(data.start());
    out->set_end(data.end());
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_RANGE_MOJOM_RANGE_MOJOM_TRAITS_H_
