// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_
#define UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_

#include <map>

#include "base/containers/flat_map.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/linux/nav_button_provider.h"

namespace gtk {

class NavButtonProviderGtk : public ui::NavButtonProvider {
 public:
  NavButtonProviderGtk();
  ~NavButtonProviderGtk() override;

  // ui::NavButtonProvider:
  void RedrawImages(int top_area_height, bool maximized, bool active) override;
  gfx::ImageSkia GetImage(ui::NavButtonProvider::FrameButtonDisplayType type,
                          ButtonState state) const override;
  gfx::Insets GetNavButtonMargin(
      ui::NavButtonProvider::FrameButtonDisplayType type) const override;
  gfx::Insets GetTopAreaSpacing() const override;
  int GetInterNavButtonSpacing() const override;

 private:
  std::map<ui::NavButtonProvider::FrameButtonDisplayType,
           base::flat_map<ui::NavButtonProvider::ButtonState, gfx::ImageSkia>>
      button_images_;
  std::map<ui::NavButtonProvider::FrameButtonDisplayType, gfx::Insets>
      button_margins_;
  gfx::Insets top_area_spacing_;
  int inter_button_spacing_;
};

}  // namespace gtk

#endif  // UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_
