// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/progress_bar_example.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/numerics/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

ProgressBarExample::ProgressBarExample()
    : ExampleBase(GetStringUTF8(IDS_PROGRESS_SELECT_LABEL).c_str()) {}

ProgressBarExample::~ProgressBarExample() = default;

void ProgressBarExample::CreateExampleView(View* container) {
  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::ColumnSize::kFixed, 200, 0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ProgressBarExample::ButtonPressed,
                          base::Unretained(this), -0.1),
      base::ASCIIToUTF16("-")));
  progress_bar_ = layout->AddView(std::make_unique<ProgressBar>());
  layout->AddView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ProgressBarExample::ButtonPressed,
                          base::Unretained(this), 0.1),
      base::ASCIIToUTF16("+")));

  layout->StartRowWithPadding(0, 0, 0, 10);
  layout->AddView(
      std::make_unique<Label>(GetStringUTF16(IDS_PROGRESS_LOADER_LABEL)));
  auto infinite_bar = std::make_unique<ProgressBar>();
  infinite_bar->SetValue(-1);
  layout->AddView(std::move(infinite_bar));

  layout->StartRowWithPadding(0, 0, 0, 10);
  layout->AddView(
      std::make_unique<Label>(GetStringUTF16(IDS_PROGRESS_LOADER_SHORT_LABEL)));
  auto shorter_bar = std::make_unique<ProgressBar>(2);
  shorter_bar->SetValue(-1);
  layout->AddView(std::move(shorter_bar));
}

void ProgressBarExample::ButtonPressed(double step) {
  current_percent_ = base::ClampToRange(current_percent_ + step, 0.0, 1.0);
  progress_bar_->SetValue(current_percent_);
}

}  // namespace examples
}  // namespace views
