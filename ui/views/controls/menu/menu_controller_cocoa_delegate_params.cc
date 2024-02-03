// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller_cocoa_delegate_params.h"

#include "base/numerics/safe_conversions.h"
#include "components/remote_cocoa/common/menu.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/views/badge_painter.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace views {

remote_cocoa::mojom::MenuControllerParamsPtr MenuControllerParamsForWidget(
    Widget* widget) {
  auto* color_provider = widget->GetColorProvider();
  auto params = remote_cocoa::mojom::MenuControllerParams::New();

  // The preferred font is slightly smaller and slightly more bold than the
  // menu font. The size change is required to make it look correct in the
  // badge; we add a small degree of bold to prevent color smearing/blurring
  // due to font smoothing. This ensures readability on all platforms and in
  // both light and dark modes.
  params->badge_font =
      BadgePainter::GetBadgeFont(MenuConfig::instance().font_list)
          .GetPrimaryFont();
  params->badge_color =
      color_provider->GetColor(ui::kColorBadgeInCocoaMenuBackground);
  params->badge_text_color =
      color_provider->GetColor(ui::kColorBadgeInCocoaMenuForeground);
  params->badge_horizontal_margin = BadgePainter::kBadgeHorizontalMargin;
  params->badge_internal_padding = BadgePainter::kBadgeInternalPadding;
  params->badge_min_height = BadgePainter::kBadgeMinHeightCocoa;
  params->badge_radius =
      base::checked_cast<uint32_t>(LayoutProvider::Get()->GetCornerRadiusMetric(
          ShapeContextTokens::kBadgeRadius));
  params->iph_dot_color =
      color_provider->GetColor(ui::kColorButtonBackgroundProminent);
  return params;
}

}  // namespace views
