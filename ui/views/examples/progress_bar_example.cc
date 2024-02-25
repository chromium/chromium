// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/progress_bar_example.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

ProgressBarExample::ProgressBarExample()
    : ExampleBase(GetStringUTF8(IDS_PROGRESS_SELECT_LABEL).c_str()) {}

ProgressBarExample::~ProgressBarExample() = default;

void ProgressBarExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(LayoutAlignment::kEnd, LayoutAlignment::kCenter,
                  TableLayout::kFixedSize,
                  TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(0, 8)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kCenter, 1.0f,
                 TableLayout::ColumnSize::kFixed, 200, 0)
      .AddPaddingColumn(0, 8)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kCenter,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, 10)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, 10)
      .AddRows(1, TableLayout::kFixedSize);

  container->AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ProgressBarExample::ButtonPressed,
                          base::Unretained(this), -0.1),
      u"-"));
  progress_bar_ = container->AddChildView(std::make_unique<ProgressBar>());
  container->AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ProgressBarExample::ButtonPressed,
                          base::Unretained(this), 0.1),
      u"+"));

  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_PROGRESS_LOADER_LABEL)));
  container->AddChildView(std::make_unique<ProgressBar>())->SetValue(-1);
  container->AddChildView(std::make_unique<View>());

  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_PROGRESS_LOADER_SHORT_LABEL)));
  auto* short_bar = container->AddChildView(std::make_unique<ProgressBar>());
  short_bar->SetValue(-1);
  short_bar->SetPreferredHeight(2);
}

void ProgressBarExample::ButtonPressed(double step) {
  current_percent_ = std::clamp(current_percent_ + step, 0.0, 1.0);
  progress_bar_->SetValue(current_percent_);
}

}  // namespace views::examples
