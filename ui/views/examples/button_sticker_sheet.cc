// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/button_sticker_sheet.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/grid_layout.h"

namespace views {
namespace examples {

namespace {

// The id of the only ColumnSet in a stretchy grid (see below).
const int kStretchyGridColumnSetId = 0;

// Creates a stretchy grid layout: there are |ncols| columns, separated from
// each other by padding columns, and all non-padding columns have equal flex
// weight and will flex in either dimension as needed. The resulting grid layout
// has only one ColumnSet (numbered kStretchyGridColumnSetId).
GridLayout* MakeStretchyGridLayout(View* host, int ncols) {
  const float kPaddingResizesEqually = 1.0;
  const int kPaddingWidth = 30;
  const GridLayout::Alignment kColumnStretchesHorizontally = GridLayout::FILL;
  const GridLayout::Alignment kColumnStretchesVertically = GridLayout::FILL;
  const float kColumnDoesNotResize = 0.0;
  const GridLayout::SizeType kColumnUsesFixedSize = GridLayout::FIXED;
  const int kColumnWidth = 96;

  GridLayout* layout =
      host->SetLayoutManager(std::make_unique<views::GridLayout>());
  ColumnSet* columns = layout->AddColumnSet(kStretchyGridColumnSetId);
  for (int i = 0; i < ncols; ++i) {
    if (i != 0)
      columns->AddPaddingColumn(kPaddingResizesEqually, kPaddingWidth);
    columns->AddColumn(kColumnStretchesHorizontally, kColumnStretchesVertically,
                       kColumnDoesNotResize, kColumnUsesFixedSize, kColumnWidth,
                       kColumnWidth);
  }
  return layout;
}

std::unique_ptr<View> MakePlainLabel(const std::string& text) {
  return std::make_unique<Label>(base::ASCIIToUTF16(text));
}

// Add a row containing a label whose text is |label_text| and then all the
// views in |views| to the supplied GridLayout, with padding between rows.
template <typename T>
void AddLabeledRowToGridLayout(GridLayout* layout,
                               const std::string& label_text,
                               std::vector<std::unique_ptr<T>> views) {
  const float kRowDoesNotResizeVertically = 0.0;
  const int kPaddingRowHeight = 8;
  layout->StartRow(kRowDoesNotResizeVertically, kStretchyGridColumnSetId);
  layout->AddView(MakePlainLabel(label_text));
  for (auto& view : views)
    layout->AddView(std::move(view));
  // This gets added extraneously after the last row, but it doesn't hurt and
  // means there's no need to keep track of whether to add it or not.
  layout->AddPaddingRow(kRowDoesNotResizeVertically, kPaddingRowHeight);
}

// Constructs a pair of MdTextButtons in the specified |state| with the
// specified |listener|, and returns them in |*primary| and |*secondary|. The
// button in |*primary| is a call-to-action button, and the button in
// |*secondary| is a regular button.
std::vector<std::unique_ptr<MdTextButton>> MakeButtonsInState(
    ButtonListener* listener,
    Button::ButtonState state) {
  std::vector<std::unique_ptr<MdTextButton>> buttons;
  const base::string16 button_text = base::ASCIIToUTF16("Button");
  auto primary = MdTextButton::Create(listener, button_text);
  primary->SetProminent(true);
  primary->SetState(state);
  buttons.push_back(std::move(primary));

  auto secondary = MdTextButton::Create(listener, button_text);
  secondary->SetState(state);
  buttons.push_back(std::move(secondary));
  return buttons;
}

}  // namespace

ButtonStickerSheet::ButtonStickerSheet()
    : ExampleBase("Button (Sticker Sheet)") {}

ButtonStickerSheet::~ButtonStickerSheet() = default;

void ButtonStickerSheet::CreateExampleView(View* container) {
  GridLayout* layout = MakeStretchyGridLayout(container, 3);

  // The title row has an empty row label.
  std::vector<std::unique_ptr<View>> plainLabel;
  plainLabel.push_back(MakePlainLabel("Primary"));
  plainLabel.push_back(MakePlainLabel("Secondary"));
  AddLabeledRowToGridLayout(layout, std::string(), std::move(plainLabel));

  AddLabeledRowToGridLayout(layout, "Default",
                            MakeButtonsInState(this, Button::STATE_NORMAL));
  AddLabeledRowToGridLayout(layout, "Normal",
                            MakeButtonsInState(this, Button::STATE_NORMAL));
  AddLabeledRowToGridLayout(layout, "Hovered",
                            MakeButtonsInState(this, Button::STATE_HOVERED));
  AddLabeledRowToGridLayout(layout, "Pressed",
                            MakeButtonsInState(this, Button::STATE_PRESSED));
  AddLabeledRowToGridLayout(layout, "Disabled",
                            MakeButtonsInState(this, Button::STATE_DISABLED));
}

void ButtonStickerSheet::ButtonPressed(Button* button, const ui::Event& event) {
  // Ignore button presses.
}

}  // namespace examples
}  // namespace views
