// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_LIST_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_LIST_MOJOM_TRAITS_H_

#include "ui/display/display_list.h"
#include "ui/display/mojom/display_list.mojom-shared.h"

namespace display {
class Display;
}  // namespace display

namespace mojo {

template <>
struct StructTraits<display::mojom::DisplayListDataView, display::DisplayList> {
  static const std::vector<display::Display>& displays(
      const display::DisplayList& r) {
    DCHECK(r.IsValid());
    return r.displays();
  }

  static int64_t primary_id(const display::DisplayList& r) {
    DCHECK(r.IsValid());
    return r.primary_id();
  }

  static int64_t current_id(const display::DisplayList& r) {
    DCHECK(r.IsValid());
    return r.current_id();
  }

  static bool Read(display::mojom::DisplayListDataView r,
                   display::DisplayList* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_LIST_MOJOM_TRAITS_H_
