// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/typography_example.h"

#include <memory>
#include <utility>

#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_class_properties.h"

namespace views::examples {

TypographyExample::TypographyExample()
    : ExampleBase(
          l10n_util::GetStringUTF8(IDS_TYPOGRAPHY_SELECT_LABEL).c_str()) {}

TypographyExample::~TypographyExample() = default;

void TypographyExample::CreateExampleView(View* container) {
  container->SetUseDefaultFillLayout(true);

  std::u16string headline_text =
      l10n_util::GetStringUTF16(IDS_TYPOGRAPHY_HEADLINE_PLACEHOLDER_TEXT);
  std::u16string body_text =
      l10n_util::GetStringUTF16(IDS_TYPOGRAPHY_BODY_PLACEHOLDER_TEXT);

  auto headlines =
      Builder<TableLayoutView>()
          .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                     TableLayout::kFixedSize,
                     TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .AddPaddingColumn(TableLayout::kFixedSize, 4)
          .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                     TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed,
                     600, 0)
          .AddRows(6, TableLayout::kFixedSize, 0)
          .AddChildren(
              Builder<Label>().SetText(u"HeadLine1"),
              Builder<Label>()
                  .SetText(headline_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_HEADLINE_1),
              Builder<Label>().SetText(u"HeadLine2"),
              Builder<Label>()
                  .SetText(headline_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_HEADLINE_2),
              Builder<Label>().SetText(u"HeadLine3"),
              Builder<Label>()
                  .SetText(headline_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_HEADLINE_3),
              Builder<Label>().SetText(u"HeadLine4"),
              Builder<Label>()
                  .SetText(headline_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_HEADLINE_4),
              Builder<Label>().SetText(u"HeadLine4Bold"),
              Builder<Label>()
                  .SetText(headline_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_HEADLINE_4_BOLD),
              Builder<Label>().SetText(u"HeadLine5"),
              Builder<Label>()
                  .SetText(headline_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_HEADLINE_5))
          .Build();

  headlines->SetProperty(kMarginsKey, gfx::Insets().set_bottom(10));

  auto bodies =
      Builder<TableLayoutView>()
          .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                     TableLayout::kFixedSize,
                     TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .AddPaddingColumn(TableLayout::kFixedSize, 4)
          .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                     TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed,
                     220, 0)
          .AddPaddingColumn(TableLayout::kFixedSize, 4)
          .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                     TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed,
                     220, 0)
          .AddPaddingColumn(TableLayout::kFixedSize, 4)
          .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStart,
                     TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed,
                     220, 0)
          .AddRows(9, TableLayout::kFixedSize, 0)
          .AddChildren(
              Builder<View>(), Builder<Label>().SetText(u"Regular"),
              Builder<Label>().SetText(u"Medium"),
              Builder<Label>().SetText(u"Bold"),
              Builder<Label>().SetText(u"Body1"),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_1),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_1_MEDIUM),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_1_BOLD),
              Builder<Label>().SetText(u"Body2"),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_2),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_2_MEDIUM),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_2_BOLD),
              Builder<Label>().SetText(u"Body3"),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_3),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_3_MEDIUM),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_3_BOLD),
              Builder<Label>().SetText(u"Body4"),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_4),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_4_MEDIUM),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_4_BOLD),
              Builder<Label>().SetText(u"Body5"),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_5),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_5_MEDIUM),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_BODY_5_BOLD),
              Builder<Label>().SetText(u"Caption"),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_CAPTION),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_CAPTION_MEDIUM),
              Builder<Label>()
                  .SetText(body_text)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetTextStyle(style::STYLE_CAPTION_BOLD))
          .Build();

  auto wrapper = std::make_unique<BoxLayoutView>();
  wrapper->SetOrientation(BoxLayout::Orientation::kVertical);
  wrapper->AddChildView(std::move(headlines));
  wrapper->AddChildView(std::move(bodies));

  auto scroll_view = std::make_unique<ScrollView>();
  scroll_view->SetContents(std::move(wrapper));
  // TODO(crbug.com/40280756): no calling ClipHeightTo() will result in 0
  // height.
  scroll_view->ClipHeightTo(0, 0);
  container->AddChildView(std::move(scroll_view));
}

}  // namespace views::examples
