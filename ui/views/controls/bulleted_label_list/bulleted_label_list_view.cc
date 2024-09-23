// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"

#include <algorithm>
#include <vector>
#include <memory>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace views {

namespace {

class BulletView : public View {
 METADATA_HEADER(BulletView, View)

 public:
  BulletView() = default;
  BulletView(const BulletView&) = delete;
  BulletView& operator=(const BulletView&) = delete;

  void OnPaint(gfx::Canvas* canvas) override;
};

void BulletView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  SkScalar radius = std::min(height(), width()) / 8.0;
  gfx::Point center = GetLocalBounds().CenterPoint();

  SkPath path;
  path.addCircle(center.x(), center.y(), radius);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(
      GetColorProvider()->GetColor(views::TypographyProvider::Get().GetColorId(
          views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY)));
  flags.setAntiAlias(true);

  canvas->DrawPath(path, flags);
}

BEGIN_METADATA(BulletView)
END_METADATA

}  // namespace

BulletedLabelListView::BulletedLabelListView(
    const std::vector<std::u16string>& texts,
    style::TextStyle label_text_style) {
  const int width = LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kStretch,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kFixed, width, width)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, 1.0,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  views::TableLayout* layout =
      static_cast<views::TableLayout*>(GetLayoutManager());
  layout->AddRows(texts.size(), views::TableLayout::kFixedSize);

  // Add a label for each of the strings in |texts|.
  for (const auto& text : texts) {
    AddChildView(std::make_unique<BulletView>());
    auto* label = AddChildView(std::make_unique<views::Label>(text));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetTextStyle(label_text_style);
  }
}

BulletedLabelListView::~BulletedLabelListView() = default;

BEGIN_METADATA(BulletedLabelListView)
END_METADATA

}  // namespace views

