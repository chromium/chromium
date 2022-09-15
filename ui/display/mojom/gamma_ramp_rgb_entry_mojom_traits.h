// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_GAMMA_RAMP_RGB_ENTRY_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_GAMMA_RAMP_RGB_ENTRY_MOJOM_TRAITS_H_

#include "ui/display/mojom/gamma_ramp_rgb_entry.mojom.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"

namespace mojo {

template <>
struct StructTraits<display::mojom::GammaRampRGBEntryDataView,
                    display::GammaRampRGBEntry> {
  static uint16_t r(const display::GammaRampRGBEntry& gamma_ramp_rgb_entry) {
    return gamma_ramp_rgb_entry.r;
  }

  static uint16_t g(const display::GammaRampRGBEntry& gamma_ramp_rgb_entry) {
    return gamma_ramp_rgb_entry.g;
  }

  static uint16_t b(const display::GammaRampRGBEntry& gamma_ramp_rgb_entry) {
    return gamma_ramp_rgb_entry.b;
  }

  static bool Read(display::mojom::GammaRampRGBEntryDataView data,
                   display::GammaRampRGBEntry* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_GAMMA_RAMP_RGB_ENTRY_MOJOM_TRAITS_H_
