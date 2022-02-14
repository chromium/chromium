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

void AddGtkNativeCoreColorMixer(ui::ColorProvider* provider,
                                const ui::ColorProviderManager::Key& key) {
  if (key.system_theme == ui::ColorProviderManager::SystemTheme::kDefault)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();

  // TODO(pkasting): These don't generally belong in kColorSetNative or in a
  // "native core color mixer" as they're cross-platform UI color concepts.
  // Furthermore, now that NativeThemeGtk doesn't need to define these colors,
  // they should be moved into this file and systematized.
  ui::ColorSet::ColorMap color_map;
  for (ui::ColorId id = ui::kUiColorsStart; id < ui::kUiColorsEnd; ++id) {
    // Add GTK color definitions to the map if they exist.
    absl::optional<SkColor> color = gtk::SkColorFromColorId(id);
    if (color)
      color_map[id] = *color;
  }
  mixer.AddSet({ui::kColorSetNative, std::move(color_map)});

  mixer[ui::kColorEndpointBackground] =
      ui::GetColorWithMaxContrast(ui::kColorEndpointForeground);
  mixer[ui::kColorEndpointForeground] =
      ui::GetColorWithMaxContrast(ui::kColorWindowBackground);

  mixer[ui::kColorNativeButtonBorder] = {GetBorderColor("GtkButton#button")};
}

}  // namespace gtk
