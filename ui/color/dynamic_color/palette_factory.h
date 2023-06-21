// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_DYNAMIC_COLOR_PALETTE_FACTORY_H_
#define UI_COLOR_DYNAMIC_COLOR_PALETTE_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/dynamic_color/palette.h"

namespace ui {

// For the desired `seed_color` and `variant`, generates the correct type of
// `Palette`.
COMPONENT_EXPORT(DYNAMIC_COLOR)
std::unique_ptr<Palette> GeneratePalette(
    SkColor seed_color,
    ColorProviderKey::SchemeVariant variant);

}  // namespace ui

#endif  // UI_COLOR_DYNAMIC_COLOR_PALETTE_FACTORY_H_
