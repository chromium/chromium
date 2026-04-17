// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_
#define UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_

#include <map>
#include <optional>

#include "base/containers/flat_map.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/linux/nav_button_provider.h"

typedef struct _GtkParamSpec GtkParamSpec;
typedef struct _GtkSettings GtkSettings;

namespace gtk {

class NavButtonProviderGtk : public ui::NavButtonProvider {
 public:
  explicit NavButtonProviderGtk(ui::FrameType frame_type);
  ~NavButtonProviderGtk() override;

  // ui::NavButtonProvider:
  void RedrawImages(int top_area_height, bool maximized, bool active) override;
  gfx::ImageSkia GetImage(ui::NavButtonProvider::FrameButtonDisplayType type,
                          ButtonState state) const override;
  gfx::Insets GetNavButtonMargin(
      ui::NavButtonProvider::FrameButtonDisplayType type) const override;
  gfx::Insets GetTopAreaSpacing() const override;
  int GetNavButtonHeight(bool maximized) const override;
  int GetInterNavButtonSpacing() const override;

 private:
  void OnThemeChanged(GtkSettings* settings, GtkParamSpec* param);

  const ui::FrameType frame_type_;

  std::map<ui::NavButtonProvider::FrameButtonDisplayType,
           base::flat_map<ui::NavButtonProvider::ButtonState, gfx::ImageSkia>>
      button_images_;
  std::map<ui::NavButtonProvider::FrameButtonDisplayType, gfx::Insets>
      button_margins_;
  gfx::Insets top_area_spacing_;
  int inter_button_spacing_;

  // Cached button height per maximized state, invalidated on theme change.
  mutable std::optional<int> nav_button_height_restored_;
  mutable std::optional<int> nav_button_height_maximized_;

  ScopedGSignal theme_name_signal_;
  ScopedGSignal prefer_dark_signal_;
};

}  // namespace gtk

#endif  // UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_
