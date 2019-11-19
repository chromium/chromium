// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/label_example.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace views {
namespace examples {

namespace {

const char* kAlignments[] = { "Left", "Center", "Right", "Head" };

// A Label with a clamped preferred width to demonstrate eliding or wrapping.
class ExamplePreferredSizeLabel : public Label {
 public:
  ExamplePreferredSizeLabel() { SetBorder(CreateSolidBorder(1, SK_ColorGRAY)); }
  ~ExamplePreferredSizeLabel() override = default;

  // Label:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(50, Label::CalculatePreferredSize().height());
  }

  static const char* kElideBehaviors[];

 private:
  DISALLOW_COPY_AND_ASSIGN(ExamplePreferredSizeLabel);
};

// static
const char* ExamplePreferredSizeLabel::kElideBehaviors[] = {
    "No Elide",   "Truncate",    "Elide Head", "Elide Middle",
    "Elide Tail", "Elide Email", "Fade Tail"};

}  // namespace

LabelExample::LabelExample() : ExampleBase("Label") {}

LabelExample::~LabelExample() = default;

void LabelExample::CreateExampleView(View* container) {
  // A very simple label example, followed by additional helpful examples.
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(), 10));
  container->AddChildView(
      std::make_unique<Label>(ASCIIToUTF16("Hello world!")));

  const wchar_t hello_world_hebrew[] =
      L"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd!";
  auto label = std::make_unique<Label>(WideToUTF16(hello_world_hebrew));
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(
      WideToUTF16(L"A UTF16 surrogate pair: \x5d0\x5b0"));
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(ASCIIToUTF16("A left-aligned blue label."));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SK_ColorBLUE);
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(WideToUTF16(L"Password!"));
  label->SetObscured(true);
  container->AddChildView(std::move(label));

  label =
      std::make_unique<Label>(ASCIIToUTF16("A Courier-18 label with shadows."));
  label->SetFontList(gfx::FontList("Courier, 18px"));
  gfx::ShadowValues shadows(1,
                            gfx::ShadowValue(gfx::Vector2d(), 1, SK_ColorRED));
  constexpr gfx::ShadowValue shadow(gfx::Vector2d(2, 2), 0, SK_ColorGRAY);
  shadows.push_back(shadow);
  label->SetShadows(shadows);
  container->AddChildView(std::move(label));

  label = std::make_unique<ExamplePreferredSizeLabel>();
  label->SetText(ASCIIToUTF16("A long label will elide toward its logical end "
      "if the text's width exceeds the label's available width."));
  container->AddChildView(std::move(label));

  label = std::make_unique<ExamplePreferredSizeLabel>();
  label->SetText(ASCIIToUTF16("A multi-line label will wrap onto subsequent "
    "lines if the text's width exceeds the label's available width, which is "
    "helpful for extemely long text used to demonstrate line wrapping."));
  label->SetMultiLine(true);
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(ASCIIToUTF16("Label with thick border"));
  label->SetBorder(CreateSolidBorder(20, SK_ColorRED));
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(
      ASCIIToUTF16("A multiline label...\n\n...which supports text selection"));
  label->SetSelectable(true);
  label->SetMultiLine(true);
  container->AddChildView(std::move(label));

  AddCustomLabel(container);
}

void LabelExample::ButtonPressed(Button* button, const ui::Event& event) {
  if (button == multiline_) {
    custom_label_->SetMultiLine(multiline_->GetChecked());
  } else if (button == shadows_) {
    gfx::ShadowValues shadows;
    if (shadows_->GetChecked()) {
      shadows.push_back(gfx::ShadowValue(gfx::Vector2d(), 1, SK_ColorRED));
      shadows.push_back(gfx::ShadowValue(gfx::Vector2d(2, 2), 0, SK_ColorGRAY));
    }
    custom_label_->SetShadows(shadows);
  } else if (button == selectable_) {
    custom_label_->SetSelectable(selectable_->GetChecked());
  }
  custom_label_->parent()->parent()->InvalidateLayout();
  custom_label_->SchedulePaint();
}

void LabelExample::OnPerformAction(Combobox* combobox) {
  if (combobox == alignment_) {
    custom_label_->SetHorizontalAlignment(
        static_cast<gfx::HorizontalAlignment>(combobox->GetSelectedIndex()));
  } else if (combobox == elide_behavior_) {
    custom_label_->SetElideBehavior(
        static_cast<gfx::ElideBehavior>(combobox->GetSelectedIndex()));
  }
}

void LabelExample::ContentsChanged(Textfield* sender,
                                   const base::string16& new_contents) {
  custom_label_->SetText(new_contents);
  custom_label_->parent()->parent()->InvalidateLayout();
}

void LabelExample::AddCustomLabel(View* container) {
  std::unique_ptr<View> control_container = std::make_unique<View>();
  control_container->SetBorder(CreateSolidBorder(2, SK_ColorGRAY));
  control_container->SetBackground(CreateSolidBackground(SK_ColorLTGRAY));
  GridLayout* layout = control_container->SetLayoutManager(
      std::make_unique<views::GridLayout>());

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                        0.0f, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        1.0f, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(std::make_unique<Label>(ASCIIToUTF16("Content: ")));
  auto textfield = std::make_unique<Textfield>();
  textfield->SetText(
      ASCIIToUTF16("Use the provided controls to configure the "
                   "content and presentation of this custom label."));
  textfield->SetEditableSelectionRange(gfx::Range());
  textfield->set_controller(this);
  textfield_ = layout->AddView(std::move(textfield));

  alignment_ =
      AddCombobox(layout, "Alignment: ", kAlignments, base::size(kAlignments));
  elide_behavior_ = AddCombobox(
      layout, "Elide Behavior: ", ExamplePreferredSizeLabel::kElideBehaviors,
      base::size(ExamplePreferredSizeLabel::kElideBehaviors));

  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                        0, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                        0, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 1);
  multiline_ = layout->AddView(
      std::make_unique<Checkbox>(base::ASCIIToUTF16("Multiline"), this));
  shadows_ = layout->AddView(
      std::make_unique<Checkbox>(base::ASCIIToUTF16("Shadows"), this));
  selectable_ = layout->AddView(
      std::make_unique<Checkbox>(base::ASCIIToUTF16("Selectable"), this));
  layout->AddPaddingRow(0, 8);

  column_set = layout->AddColumnSet(2);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        1, GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 2);
  auto custom_label = std::make_unique<ExamplePreferredSizeLabel>();
  custom_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  custom_label->SetElideBehavior(gfx::NO_ELIDE);
  custom_label->SetText(textfield_->GetText());
  custom_label_ = layout->AddView(std::move(custom_label));

  // Disable the text selection checkbox if |custom_label_| does not support
  // text selection.
  selectable_->SetEnabled(custom_label_->IsSelectionSupported());

  container->AddChildView(std::move(control_container));
}

Combobox* LabelExample::AddCombobox(GridLayout* layout,
                                    const char* name,
                                    const char** strings,
                                    int count) {
  layout->StartRow(0, 0);
  layout->AddView(std::make_unique<Label>(base::ASCIIToUTF16(name)));
  auto combobox = std::make_unique<Combobox>(
      std::make_unique<ExampleComboboxModel>(strings, count));
  combobox->SetSelectedIndex(0);
  combobox->set_listener(this);
  return layout->AddView(std::move(combobox));
}

}  // namespace examples
}  // namespace views
