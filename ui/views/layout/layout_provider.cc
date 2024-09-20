// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_provider.h"

#include <algorithm>

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/views_delegate.h"

namespace views {

namespace {

LayoutProvider* g_layout_delegate = nullptr;

}  // namespace

LayoutProvider::LayoutProvider() {
  g_layout_delegate = this;
}

LayoutProvider::~LayoutProvider() {
  if (this == g_layout_delegate)
    g_layout_delegate = nullptr;
}

// static
LayoutProvider* LayoutProvider::Get() {
  return g_layout_delegate;
}

// static
int LayoutProvider::GetControlHeightForFont(int context,
                                            int style,
                                            const gfx::FontList& font) {
  return std::max(TypographyProvider::Get().GetLineHeight(context, style),
                  font.GetHeight()) +
         Get()->GetDistanceMetric(DISTANCE_CONTROL_VERTICAL_TEXT_PADDING) * 2;
}

gfx::Insets LayoutProvider::GetInsetsMetric(int metric) const {
  DCHECK_GE(metric, VIEWS_INSETS_START);
  DCHECK_LT(metric, VIEWS_INSETS_MAX);
  switch (metric) {
    case InsetsMetric::INSETS_DIALOG:
    case InsetsMetric::INSETS_DIALOG_SUBSECTION:
    case InsetsMetric::INSETS_DIALOG_FOOTNOTE:
      return gfx::Insets(13);
    case InsetsMetric::INSETS_DIALOG_BUTTON_ROW: {
      const gfx::Insets dialog_insets = GetInsetsMetric(INSETS_DIALOG);
      return gfx::Insets::TLBR(0, dialog_insets.left(), dialog_insets.bottom(),
                               dialog_insets.right());
    }
    case InsetsMetric::INSETS_DIALOG_TITLE: {
      const gfx::Insets dialog_insets = GetInsetsMetric(INSETS_DIALOG);
      return gfx::Insets::TLBR(dialog_insets.top(), dialog_insets.left(), 0,
                               dialog_insets.right());
    }
    case InsetsMetric::INSETS_TOOLTIP_BUBBLE:
      return gfx::Insets(8);
    case InsetsMetric::INSETS_CHECKBOX_RADIO_BUTTON:
      return gfx::Insets::VH(5, 6);
    case InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(4);
    case InsetsMetric::INSETS_LABEL_BUTTON:
      return gfx::Insets::VH(5, 6);
    case InsetsMetric::INSETS_ICON_BUTTON:
      return gfx::Insets(2);
  }
  NOTREACHED();
}

int LayoutProvider::GetDistanceMetric(int metric) const {
  DCHECK_GE(metric, VIEWS_DISTANCE_START);
  DCHECK_LT(metric, VIEWS_DISTANCE_END);

  switch (static_cast<DistanceMetric>(metric)) {
    case DISTANCE_BUBBLE_PREFERRED_WIDTH:
      return kSmallDialogWidth;
    case DISTANCE_BUTTON_HORIZONTAL_PADDING:
      return 16;
    case DISTANCE_BUTTON_MAX_LINKABLE_WIDTH:
      return 112;
    case DISTANCE_CLOSE_BUTTON_MARGIN:
      return 20;
    case DISTANCE_CONTROL_VERTICAL_TEXT_PADDING:
      return 10;
    case DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH:
      // Minimum label size plus padding.
      return 32 + 2 * GetDistanceMetric(DISTANCE_BUTTON_HORIZONTAL_PADDING);
    case DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL:
      return 24;
    case DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT:
      // This is reduced so there is about the same amount of visible
      // whitespace, compensating for the text's internal leading.
      return GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL) -
             8;
    case DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL:
      return 16;
    case DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT:
      // See the comment in DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT above.
      return GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL) - 8;
    case DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING:
      return 8;
    case DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN:
      return 12;
    case DISTANCE_DROPDOWN_BUTTON_LEFT_MARGIN:
      return 16;
    case DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH:
      return kMediumDialogWidth;
    case DISTANCE_RELATED_BUTTON_HORIZONTAL:
      return 8;
    case DISTANCE_RELATED_CONTROL_HORIZONTAL:
      return 16;
    case DISTANCE_RELATED_CONTROL_VERTICAL:
      return 8;
    case DISTANCE_RELATED_LABEL_HORIZONTAL:
      return 12;
    case DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT:
      return 192;
    case DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT:
      return 448;
    case DISTANCE_TABLE_CELL_HORIZONTAL_MARGIN:
      return 12;
    case DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING:
      return 10;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL:
      return 16;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL:
      return 16;
    case DISTANCE_VECTOR_ICON_PADDING:
      return 4;
    case VIEWS_DISTANCE_END:
    case VIEWS_DISTANCE_MAX:
      NOTREACHED();
  }
  NOTREACHED();
}

const TypographyProvider& LayoutProvider::GetTypographyProvider() const {
  return typography_provider_;
}

int LayoutProvider::GetSnappedDialogWidth(int min_width) const {
  // TODO(pbos): Move snapping logic from ChromeLayoutProvider and update
  // unittests to pass with snapping points (instead of exact preferred width).

  // This is an arbitrary value, but it's a good arbitrary value. Some dialogs
  // have very small widths for their contents views, which causes ugly
  // title-wrapping where a two-word title is split across multiple lines or
  // similar. To prevent that, forbid any snappable dialog from being narrower
  // than this value. In principle it's possible to factor in the title width
  // here, but it is not really worth the complexity.
  return std::max(min_width, 320);
}

gfx::Insets LayoutProvider::GetDialogInsetsForContentType(
    DialogContentType leading,
    DialogContentType trailing) const {
  const int top_margin =
      leading == DialogContentType::kControl
          ? GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL)
          : GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT);
  const int bottom_margin =
      trailing == DialogContentType::kControl
          ? GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL)
          : GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT);
  const gfx::Insets dialog_insets = GetInsetsMetric(INSETS_DIALOG);
  return gfx::Insets::TLBR(top_margin, dialog_insets.left(), bottom_margin,
                           dialog_insets.right());
}

int LayoutProvider::GetCornerRadiusMetric(Emphasis emphasis,
                                          const gfx::Size& size) const {
  switch (emphasis) {
    case Emphasis::kNone:
      return 0;
    case Emphasis::kLow:
    case Emphasis::kMedium:
      return 4;
    case Emphasis::kHigh:
      return 8;
    case Emphasis::kMaximum:
      return std::min(size.width(), size.height()) / 2;
  }
}

ShapeSysTokens GetShapeSysToken(ShapeContextTokens id) {
  static constexpr auto shape_token_map =
      base::MakeFixedFlatMap<ShapeContextTokens, ShapeSysTokens>({
          {ShapeContextTokens::kBadgeRadius, ShapeSysTokens::kXSmall},
          {ShapeContextTokens::kButtonRadius, ShapeSysTokens::kFull},
          {ShapeContextTokens::kComboboxRadius, ShapeSysTokens::kSmall},
          {ShapeContextTokens::kDialogRadius, ShapeSysTokens::kMediumSmall},
          {ShapeContextTokens::kFindBarViewRadius, ShapeSysTokens::kSmall},
          {ShapeContextTokens::kMenuRadius, ShapeSysTokens::kMediumSmall},
          {ShapeContextTokens::kMenuAuxRadius, ShapeSysTokens::kMediumSmall},
          {ShapeContextTokens::kMenuTouchRadius, ShapeSysTokens::kMediumSmall},
          {ShapeContextTokens::kOmniboxExpandedRadius, ShapeSysTokens::kMedium},
          {ShapeContextTokens::kTextfieldRadius, ShapeSysTokens::kSmall},
          {ShapeContextTokens::kSidePanelContentRadius,
           ShapeSysTokens::kMedium},
          {ShapeContextTokens::kSidePanelPageContentRadius,
           ShapeSysTokens::kSmall},
      });
  const auto it = shape_token_map.find(id);
  return it == shape_token_map.end() ? ShapeSysTokens::kDefault : it->second;
}

int LayoutProvider::GetCornerRadiusMetric(ShapeContextTokens id,
                                          const gfx::Size& size) const {
  ShapeSysTokens token = GetShapeSysToken(id);
  DCHECK_NE(token, ShapeSysTokens::kDefault)
      << "kDefault token means there is a missing mapping between shape tokens";
  switch (token) {
    case ShapeSysTokens::kXSmall:
      return 4;
    case ShapeSysTokens::kSmall:
      return 8;
    case ShapeSysTokens::kMediumSmall:
      return 12;
    case ShapeSysTokens::kMedium:
      return 16;
    case ShapeSysTokens::kLarge:
      return 24;
    case ShapeSysTokens::kFull:
      return std::min(size.width(), size.height()) / 2;
    default:
      return 0;
  }
}

int LayoutProvider::GetShadowElevationMetric(Emphasis emphasis) const {
  switch (emphasis) {
    case Emphasis::kNone:
      return 0;
    case Emphasis::kLow:
      return 1;
    case Emphasis::kMedium:
      return 2;
    case Emphasis::kHigh:
      return 3;
    case Emphasis::kMaximum:
      return 16;
  }
}

}  // namespace views
