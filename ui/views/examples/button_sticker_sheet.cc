// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/button_sticker_sheet.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/table_layout.h"

namespace views::examples {

namespace {

// Creates a stretchy table layout: there are |ncols| columns, separated from
// each other by padding columns, and all non-padding columns have equal flex
// weight and will flex in either dimension as needed.
TableLayout* MakeStretchyTableLayout(View* host, int ncols) {
  const float kPaddingResizesEqually = 1.0;
  const int kPaddingWidth = 30;
  const int kColumnWidth = 96;

  auto* layout = host->SetLayoutManager(std::make_unique<views::TableLayout>());
  for (int i = 0; i < ncols; ++i) {
    if (i != 0)
      layout->AddPaddingColumn(kPaddingResizesEqually, kPaddingWidth);
    layout->AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                      TableLayout::kFixedSize, TableLayout::ColumnSize::kFixed,
                      kColumnWidth, kColumnWidth);
  }
  return layout;
}

std::unique_ptr<View> MakePlainLabel(const std::string& text) {
  return std::make_unique<Label>(base::ASCIIToUTF16(text));
}

// Adds a label whose text is |label_text| and then all the views in |views|.
template <typename T>
void AddLabeledRow(View* parent,
                   const std::string& label_text,
                   std::vector<std::unique_ptr<T>> views) {
  parent->AddChildView(MakePlainLabel(label_text));
  for (auto& view : views)
    parent->AddChildView(std::move(view));
}

// Constructs a pair of MdTextButtons in the specified |state| with the
// specified |listener|, and returns them in |*primary| and |*secondary|. The
// button in |*primary| is a call-to-action button, and the button in
// |*secondary| is a regular button.
std::vector<std::unique_ptr<MdTextButton>> MakeButtonsInState(
    Button::ButtonState state) {
  std::vector<std::unique_ptr<MdTextButton>> buttons;
  const std::u16string button_text = u"Button";
  auto primary = std::make_unique<views::MdTextButton>(
      Button::PressedCallback(), button_text);
  primary->SetStyle(ui::ButtonStyle::kProminent);
  primary->SetState(state);
  buttons.push_back(std::move(primary));

  auto secondary = std::make_unique<views::MdTextButton>(
      Button::PressedCallback(), button_text);
  secondary->SetState(state);
  buttons.push_back(std::move(secondary));
  return buttons;
}

}  // namespace

ButtonStickerSheet::ButtonStickerSheet()
    : ExampleBase("Button (Sticker Sheet)") {}

ButtonStickerSheet::~ButtonStickerSheet() = default;

void ButtonStickerSheet::CreateExampleView(View* container) {
  TableLayout* layout = MakeStretchyTableLayout(container, 3);
  for (int i = 0; i < 6; ++i) {
    const int kPaddingRowHeight = 8;
    layout->AddRows(1, TableLayout::kFixedSize)
        .AddPaddingRow(TableLayout::kFixedSize, kPaddingRowHeight);
  }

  // The title row has an empty row label.
  std::vector<std::unique_ptr<View>> plainLabel;
  plainLabel.push_back(MakePlainLabel("Primary"));
  plainLabel.push_back(MakePlainLabel("Secondary"));
  AddLabeledRow(container, std::string(), std::move(plainLabel));

  AddLabeledRow(container, "Default", MakeButtonsInState(Button::STATE_NORMAL));
  AddLabeledRow(container, "Normal", MakeButtonsInState(Button::STATE_NORMAL));
  AddLabeledRow(container, "Hovered",
                MakeButtonsInState(Button::STATE_HOVERED));
  AddLabeledRow(container, "Pressed",
                MakeButtonsInState(Button::STATE_PRESSED));
  AddLabeledRow(container, "Disabled",
                MakeButtonsInState(Button::STATE_DISABLED));
}

}  // namespace views::examples
