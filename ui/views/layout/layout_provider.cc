// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_provider.h"

#include "base/logging.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/font_list.h"
#include "ui/views/style/typography.h"
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
  return std::max(views::style::GetLineHeight(context, style),
                  font.GetHeight()) +
         Get()->GetDistanceMetric(DISTANCE_CONTROL_VERTICAL_TEXT_PADDING) * 2;
}

gfx::Insets LayoutProvider::GetInsetsMetric(int metric) const {
  DCHECK_GE(metric, VIEWS_INSETS_START);
  DCHECK_LT(metric, VIEWS_INSETS_MAX);
  switch (metric) {
    case InsetsMetric::INSETS_DIALOG:
    case InsetsMetric::INSETS_DIALOG_SUBSECTION:
      return gfx::Insets(13, 13);
    case InsetsMetric::INSETS_DIALOG_BUTTON_ROW: {
      const gfx::Insets dialog_insets = GetInsetsMetric(INSETS_DIALOG);
      return gfx::Insets(0, dialog_insets.left(), dialog_insets.bottom(),
                         dialog_insets.right());
    }
    case InsetsMetric::INSETS_DIALOG_TITLE: {
      const gfx::Insets dialog_insets = GetInsetsMetric(INSETS_DIALOG);
      return gfx::Insets(dialog_insets.top(), dialog_insets.left(), 0,
                         dialog_insets.right());
    }
    case InsetsMetric::INSETS_TOOLTIP_BUBBLE:
      return gfx::Insets(8);
    case InsetsMetric::INSETS_CHECKBOX_RADIO_BUTTON:
      return gfx::Insets(5, 6);
    case InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(4);
    case InsetsMetric::INSETS_LABEL_BUTTON:
      return gfx::Insets(5, 6);
  }
  NOTREACHED();
  return gfx::Insets();
}

int LayoutProvider::GetDistanceMetric(int metric) const {
  DCHECK_GE(metric, VIEWS_DISTANCE_START);
  DCHECK_LT(metric, VIEWS_DISTANCE_MAX);
  switch (metric) {
    case DistanceMetric::DISTANCE_BUTTON_HORIZONTAL_PADDING:
      return 16;
    case DistanceMetric::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH:
      return 0;
    case DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN:
      return 7;
    case DistanceMetric::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING:
      return 4;
    case DistanceMetric::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH:
      return 75;
    case DistanceMetric::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL:
    case DistanceMetric::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT:
    case DistanceMetric::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL:
    case DistanceMetric::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT:
      return 13;
    case DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL:
      return 6;
    case DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL:
      return 8;
    case DistanceMetric::DISTANCE_RELATED_CONTROL_VERTICAL:
      return 8;
    case DistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL:
      return 10;
    case DistanceMetric::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT:
      return 160;
    case DistanceMetric::DISTANCE_TABLE_CELL_HORIZONTAL_MARGIN:
      return 10;
    case DistanceMetric::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING:
      return 4;
    case DistanceMetric::DISTANCE_UNRELATED_CONTROL_VERTICAL:
      return 20;
  }
  NOTREACHED();
  return 0;
}

const TypographyProvider& LayoutProvider::GetTypographyProvider() const {
  return typography_provider_;
}

int LayoutProvider::GetSnappedDialogWidth(int min_width) const {
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
      leading == CONTROL
          ? GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL)
          : GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT);
  const int bottom_margin =
      trailing == CONTROL
          ? GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL)
          : GetDistanceMetric(DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT);
  const gfx::Insets dialog_insets = GetInsetsMetric(INSETS_DIALOG);
  return gfx::Insets(top_margin, dialog_insets.left(), bottom_margin,
                     dialog_insets.right());
}

int LayoutProvider::GetCornerRadiusMetric(EmphasisMetric emphasis_metric,
                                          const gfx::Size& size) const {
  switch (emphasis_metric) {
    case views::EMPHASIS_NONE:
      return 0;
    case EMPHASIS_LOW:
    case EMPHASIS_MEDIUM:
      return 4;
    case EMPHASIS_HIGH:
      return 8;
    case EMPHASIS_MAXIMUM:
      return std::min(size.width(), size.height()) / 2;
  }
}

int LayoutProvider::GetShadowElevationMetric(
    EmphasisMetric emphasis_metric) const {
  switch (emphasis_metric) {
    case views::EMPHASIS_NONE:
      return 0;
    case views::EMPHASIS_LOW:
      return 1;
    case views::EMPHASIS_MEDIUM:
      return 2;
    case views::EMPHASIS_HIGH:
      return 3;
    case views::EMPHASIS_MAXIMUM:
      return 16;
  }
}

gfx::ShadowValues LayoutProvider::MakeShadowValues(int elevation,
                                                   SkColor color) const {
  return gfx::ShadowValue::MakeMdShadowValues(elevation, color);
}

}  // namespace views
