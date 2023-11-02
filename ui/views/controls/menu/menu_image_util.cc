// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_image_util.h"

#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

namespace views {

gfx::ImageSkia GetMenuCheckImage(SkColor icon_color) {
  return gfx::CreateVectorIcon(kMenuCheckIcon, icon_color);
}

gfx::ImageSkia GetSubmenuArrowImage(SkColor icon_color) {
  return gfx::CreateVectorIcon(vector_icons::kSubmenuArrowIcon, icon_color);
}

}  // namespace views
