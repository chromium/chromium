// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

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
  explicit BulletView(size_t line_height);
  BulletView(const BulletView&) = delete;
  BulletView& operator=(const BulletView&) = delete;

  void OnPaint(gfx::Canvas* canvas) override;

  size_t line_height_dp_ = 0;
};

BulletView::BulletView(size_t line_height) : line_height_dp_(line_height) {}

void BulletView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  SkScalar radius = std::min(height(), width()) / 8.0;
  gfx::Point top_center = GetLocalBounds().top_center();

  SkPath path;
  path.addCircle(top_center.x(),
                 static_cast<size_t>(top_center.y()) + (line_height_dp_ / 2),
                 radius);

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
    auto label = std::make_unique<views::Label>(text);
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetTextStyle(label_text_style);
    AddChildView(std::make_unique<BulletView>(label->GetLineHeight()));
    AddChildView(std::move(label));
  }
}

BulletedLabelListView::~BulletedLabelListView() = default;

BEGIN_METADATA(BulletedLabelListView)
END_METADATA

}  // namespace views

