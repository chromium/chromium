// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_COLOR_MIXERS_H_
#define UI_GTK_GTK_COLOR_MIXERS_H_

#include <optional>

#include "ui/color/color_provider_key.h"

namespace ui {
class ColorProvider;
}  // namespace ui

namespace gtk {

void AddGtkNativeColorMixer(ui::ColorProvider* provider,
                            const ui::ColorProviderKey& key,
                            std::optional<SkColor> accent_color);

}  // namespace gtk

#endif  // UI_GTK_GTK_COLOR_MIXERS_H_
