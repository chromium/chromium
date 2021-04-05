// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_color_mixers.h"

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_set.h"
#include "ui/color/color_transform.h"
#include "ui/gtk/gtk_util.h"

namespace gtk {

// TODO(tluk): The current GTK color mixer lifts the existing color definitions
// out of NativeThemeGtk. This mixer should leverage the hierarchical nature of
// color pipeline to reduce duplication of color definitions in
// `SkColorFromColorId()`.
void AddGtkNativeCoreColorMixer(
    ui::ColorProvider* provider,
    ui::ColorProviderManager::ColorMode color_mode,
    ui::ColorProviderManager::ContrastMode contrast_mode) {
  ui::ColorMixer& mixer = provider->AddMixer();

  ui::ColorSet::ColorMap color_map;
  for (ui::ColorId id = ui::kUiColorsStart; id < ui::kUiColorsEnd; ++id) {
    // Add GTK color definitions to the map if they exist.
    base::Optional<SkColor> color = gtk::SkColorFromColorId(id);
    if (color)
      color_map[id] = *color;
  }
  mixer.AddSet({ui::kColorSetNative, std::move(color_map)});

  mixer[ui::kColorEndpointBackground] =
      ui::GetColorWithMaxContrast(ui::kColorEndpointForeground);
  mixer[ui::kColorEndpointForeground] =
      ui::GetColorWithMaxContrast(ui::kColorWindowBackground);
}

}  // namespace gtk
