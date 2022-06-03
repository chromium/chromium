// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_
#define UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_

#include <map>

#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/linux_ui/nav_button_provider.h"

namespace gtk {

class NavButtonProviderGtk : public views::NavButtonProvider {
 public:
  NavButtonProviderGtk();
  ~NavButtonProviderGtk() override;

  // views::NavButtonProvider:
  void RedrawImages(int top_area_height, bool maximized, bool active) override;
  gfx::ImageSkia GetImage(views::NavButtonProvider::FrameButtonDisplayType type,
                          views::Button::ButtonState state) const override;
  gfx::Insets GetNavButtonMargin(
      views::NavButtonProvider::FrameButtonDisplayType type) const override;
  gfx::Insets GetTopAreaSpacing() const override;
  int GetInterNavButtonSpacing() const override;

 private:
  std::map<views::NavButtonProvider::FrameButtonDisplayType,
           gfx::ImageSkia[views::Button::STATE_COUNT]>
      button_images_;
  std::map<views::NavButtonProvider::FrameButtonDisplayType, gfx::Insets>
      button_margins_;
  gfx::Insets top_area_spacing_;
  int inter_button_spacing_;
};

}  // namespace gtk

#endif  // UI_GTK_NAV_BUTTON_PROVIDER_GTK_H_
