// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_COLOR_MODE_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_COLOR_MODE_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "ui/base/mojom/color_mode.mojom.h"
#include "ui/color/color_provider_key.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::ColorMode, ui::ColorProviderKey::ColorMode> {
  static ui::mojom::ColorMode ToMojom(
      ui::ColorProviderKey::ColorMode disposition) {
    switch (disposition) {
      case ui::ColorProviderKey::ColorMode::kLight:
        return ui::mojom::ColorMode::kLight;
      case ui::ColorProviderKey::ColorMode::kDark:
        return ui::mojom::ColorMode::kDark;
    }
    NOTREACHED();
  }

  static bool FromMojom(ui::mojom::ColorMode disposition,
                        ui::ColorProviderKey::ColorMode* out) {
    switch (disposition) {
      case ui::mojom::ColorMode::kLight:
        *out = ui::ColorProviderKey::ColorMode::kLight;
        return true;
      case ui::mojom::ColorMode::kDark:
        *out = ui::ColorProviderKey::ColorMode::kDark;
        return true;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_COLOR_MODE_MOJOM_TRAITS_H_
