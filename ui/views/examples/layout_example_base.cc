// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/layout_example_base.h"

#include <algorithm>
#include <memory>

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace views {
namespace examples {

namespace {

constexpr int kLayoutExampleVerticalSpacing = 3;
constexpr int kLayoutExampleLeftPadding = 8;
constexpr gfx::Size kLayoutExampleDefaultChildSize(180, 90);

// This View holds two other views which consists of a view on the left onto
// which the BoxLayout is attached for demonstrating its features. The view
// on the right contains all the various controls which allow the user to
// interactively control the various features/properties of BoxLayout. Layout()
// will ensure the left view takes 75% and the right view fills the remaining
// 25%.
class FullPanel : public View {
 public:
  FullPanel() = default;
  ~FullPanel() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FullPanel);
};

}  // namespace

LayoutExampleBase::ChildPanel::ChildPanel(LayoutExampleBase* example)
    : example_(example) {
  SetBorder(CreateSolidBorder(1, SK_ColorGRAY));
  margin_.left = CreateTextfield();
  margin_.top = CreateTextfield();
  margin_.right = CreateTextfield();
  margin_.bottom = CreateTextfield();
  flex_ = CreateTextfield();
  flex_->SetText(base::ASCIIToUTF16(""));
}

LayoutExampleBase::ChildPanel::~ChildPanel() = default;

bool LayoutExampleBase::ChildPanel::OnMousePressed(
    const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    SetSelected(true);
  return true;
}

void LayoutExampleBase::ChildPanel::Layout() {
  const int kSpacing = 2;
  if (selected_) {
    gfx::Rect client = GetContentsBounds();
    margin_.top->SetPosition(
        gfx::Point((client.width() - margin_.top->width()) / 2, kSpacing));
    margin_.left->SetPosition(
        gfx::Point(kSpacing, (client.height() - margin_.left->height()) / 2));
    margin_.bottom->SetPosition(
        gfx::Point((client.width() - margin_.bottom->width()) / 2,
                   client.height() - margin_.bottom->height() - kSpacing));
    margin_.right->SetPosition(
        gfx::Point(client.width() - margin_.right->width() - kSpacing,
                   (client.height() - margin_.right->height()) / 2));
    flex_->SetPosition(gfx::Point((client.width() - flex_->width()) / 2,
                                  (client.height() - flex_->height()) / 2));
  }
}

void LayoutExampleBase::ChildPanel::SetSelected(bool value) {
  if (value != selected_) {
    selected_ = value;
    SetBorder(CreateSolidBorder(1, selected_ ? SK_ColorBLACK : SK_ColorGRAY));
    if (selected_ && parent()) {
      for (View* child : parent()->children()) {
        if (child != this && child->GetGroup() == GetGroup())
          static_cast<ChildPanel*>(child)->SetSelected(false);
      }
    }
    margin_.left->SetVisible(selected_);
    margin_.top->SetVisible(selected_);
    margin_.right->SetVisible(selected_);
    margin_.bottom->SetVisible(selected_);
    flex_->SetVisible(selected_);
    InvalidateLayout();
    example_->RefreshLayoutPanel(false);
  }
}

int LayoutExampleBase::ChildPanel::GetFlex() {
  int flex;
  if (base::StringToInt(flex_->GetText(), &flex))
    return flex;
  return -1;
}

void LayoutExampleBase::ChildPanel::ContentsChanged(
    Textfield* sender,
    const base::string16& new_contents) {
  const gfx::Insets margins = LayoutExampleBase::TextfieldsToInsets(margin_);
  if (!margins.IsEmpty())
    this->SetProperty(kMarginsKey, margins);
  else
    this->ClearProperty(kMarginsKey);
  example_->RefreshLayoutPanel(sender == flex_);
}

Textfield* LayoutExampleBase::ChildPanel::CreateTextfield() {
  Textfield* textfield = new Textfield();
  textfield->SetDefaultWidthInChars(3);
  textfield->SizeToPreferredSize();
  textfield->SetText(base::ASCIIToUTF16("0"));
  textfield->set_controller(this);
  textfield->SetVisible(false);
  AddChildView(textfield);
  return textfield;
}

LayoutExampleBase::LayoutExampleBase(const char* title) : ExampleBase(title) {}

LayoutExampleBase::~LayoutExampleBase() = default;

Combobox* LayoutExampleBase::CreateCombobox(const base::string16& label_text,
                                            const char* const* items,
                                            int count,
                                            int* vertical_pos) {
  Label* label = new Label(label_text);
  label->SetPosition(gfx::Point(kLayoutExampleLeftPadding, *vertical_pos));
  label->SizeToPreferredSize();
  Combobox* combo_box =
      new Combobox(std::make_unique<ExampleComboboxModel>(items, count));
  combo_box->SetPosition(
      gfx::Point(label->x() + label->width() + kLayoutExampleVerticalSpacing,
                 *vertical_pos));
  combo_box->SizeToPreferredSize();
  combo_box->set_listener(this);
  label->SetSize(gfx::Size(label->width(), combo_box->height()));
  control_panel_->AddChildView(label);
  control_panel_->AddChildView(combo_box);
  *vertical_pos += combo_box->height() + kLayoutExampleVerticalSpacing;
  return combo_box;
}

Textfield* LayoutExampleBase::CreateRawTextfield(int vertical_pos,
                                                 bool add,
                                                 int* horizontal_pos) {
  Textfield* text_field = new Textfield();
  text_field->SetPosition(gfx::Point(*horizontal_pos, vertical_pos));
  text_field->SetDefaultWidthInChars(3);
  text_field->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
  text_field->SizeToPreferredSize();
  text_field->SetText(base::ASCIIToUTF16("0"));
  text_field->set_controller(this);
  *horizontal_pos += text_field->width() + kLayoutExampleVerticalSpacing;
  if (add)
    control_panel_->AddChildView(text_field);
  return text_field;
}

Textfield* LayoutExampleBase::CreateTextfield(const base::string16& label_text,
                                              int* vertical_pos) {
  Label* label = new Label(label_text);
  label->SetPosition(gfx::Point(kLayoutExampleLeftPadding, *vertical_pos));
  label->SizeToPreferredSize();
  int horizontal_pos =
      label->x() + label->width() + kLayoutExampleVerticalSpacing;
  Textfield* text_field =
      CreateRawTextfield(*vertical_pos, false, &horizontal_pos);
  label->SetSize(gfx::Size(label->width(), text_field->height()));
  control_panel_->AddChildView(label);
  control_panel_->AddChildView(text_field);
  *vertical_pos += text_field->height() + kLayoutExampleVerticalSpacing;
  return text_field;
}

void LayoutExampleBase::CreateMarginsTextFields(
    const base::string16& label_text,
    Textfield* textfields[4],
    int* vertical_pos) {
  textfields[0] = CreateTextfield(label_text, vertical_pos);
  int center = textfields[0]->x() + textfields[0]->width() / 2;
  int horizontal_pos = std::max(
      0, center - (textfields[0]->width() + kLayoutExampleVerticalSpacing / 2));
  textfields[1] = CreateRawTextfield(*vertical_pos, true, &horizontal_pos);
  textfields[3] = CreateRawTextfield(*vertical_pos, true, &horizontal_pos);
  *vertical_pos = textfields[1]->y() + textfields[1]->height() +
                  kLayoutExampleVerticalSpacing;
  horizontal_pos = textfields[0]->x();
  textfields[2] = CreateRawTextfield(*vertical_pos, true, &horizontal_pos);
  *vertical_pos = textfields[2]->y() + textfields[2]->height() +
                  kLayoutExampleVerticalSpacing;
}

void LayoutExampleBase::CreateMarginsTextFields(
    const base::string16& label_text,
    InsetTextfields* textfields,
    int* vertical_pos) {
  textfields->top = CreateTextfield(label_text, vertical_pos);
  int center = textfields->top->x() + textfields->top->width() / 2;
  int horizontal_pos = std::max(
      0,
      center - (textfields->top->width() + kLayoutExampleVerticalSpacing / 2));
  textfields->left = CreateRawTextfield(*vertical_pos, true, &horizontal_pos);
  textfields->right = CreateRawTextfield(*vertical_pos, true, &horizontal_pos);
  *vertical_pos = textfields->left->y() + textfields->left->height() +
                  kLayoutExampleVerticalSpacing;
  horizontal_pos = textfields->top->x();
  textfields->bottom = CreateRawTextfield(*vertical_pos, true, &horizontal_pos);
  *vertical_pos = textfields->bottom->y() + textfields->bottom->height() +
                  kLayoutExampleVerticalSpacing;
}

Checkbox* LayoutExampleBase::CreateCheckbox(const base::string16& label_text,
                                            int* vertical_pos) {
  Checkbox* checkbox = new Checkbox(label_text, this);
  checkbox->SetPosition(gfx::Point(kLayoutExampleLeftPadding, *vertical_pos));
  checkbox->SizeToPreferredSize();
  control_panel_->AddChildView(checkbox);
  *vertical_pos += checkbox->height() + kLayoutExampleVerticalSpacing;
  return checkbox;
}

void LayoutExampleBase::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  View* full_panel = new FullPanel();
  container->AddChildView(full_panel);

  auto* manager = full_panel->SetLayoutManager(
      std::make_unique<BoxLayout>(views::BoxLayout::Orientation::kHorizontal));
  layout_panel_ = new View();
  layout_panel_->SetBorder(CreateSolidBorder(1, SK_ColorLTGRAY));
  full_panel->AddChildView(layout_panel_);
  manager->SetFlexForView(layout_panel_, 3);
  control_panel_ = new View();
  full_panel->AddChildView(control_panel_);
  manager->SetFlexForView(control_panel_, 1);

  int vertical_pos = kLayoutExampleVerticalSpacing;
  int horizontal_pos = kLayoutExampleLeftPadding;
  auto add_button =
      MdTextButton::CreateSecondaryUiButton(this, base::ASCIIToUTF16("Add"));
  add_button->SetPosition(gfx::Point(horizontal_pos, vertical_pos));
  add_button->SizeToPreferredSize();
  add_button_ = control_panel_->AddChildView(std::move(add_button));
  horizontal_pos += add_button_->width() + kLayoutExampleVerticalSpacing;

  preferred_width_view_ =
      CreateRawTextfield(vertical_pos, true, &horizontal_pos);
  preferred_width_view_->SetY(
      vertical_pos +
      (add_button_->height() - preferred_width_view_->height()) / 2);
  preferred_width_view_->SetText(
      base::NumberToString16(kLayoutExampleDefaultChildSize.width()));

  preferred_height_view_ =
      CreateRawTextfield(vertical_pos, true, &horizontal_pos);
  preferred_height_view_->SetY(
      vertical_pos +
      (add_button_->height() - preferred_height_view_->height()) / 2);
  preferred_height_view_->SetText(
      base::NumberToString16(kLayoutExampleDefaultChildSize.height()));

  CreateAdditionalControls(add_button_->y() + add_button_->height() +
                           kLayoutExampleVerticalSpacing);
}

void LayoutExampleBase::ButtonPressed(Button* sender, const ui::Event& event) {
  constexpr int kChildPanelGroup = 100;

  if (sender == add_button_) {
    std::unique_ptr<ChildPanel> panel = std::make_unique<ChildPanel>(this);
    panel->SetPreferredSize(GetNewChildPanelPreferredSize());
    panel->SetGroup(kChildPanelGroup);
    layout_panel_->AddChildView(std::move(panel));
    RefreshLayoutPanel(false);
  } else {
    ButtonPressedImpl(sender);
  }
}

// Default implementation is to do nothing.
void LayoutExampleBase::ButtonPressedImpl(Button* sender) {}

void LayoutExampleBase::RefreshLayoutPanel(bool update_layout) {
  if (update_layout)
    UpdateLayoutManager();
  layout_panel_->InvalidateLayout();
  layout_panel_->SchedulePaint();
}

gfx::Size LayoutExampleBase::GetNewChildPanelPreferredSize() {
  int width;
  if (!base::StringToInt(preferred_width_view_->GetText(), &width))
    width = kLayoutExampleDefaultChildSize.width();

  int height;
  if (!base::StringToInt(preferred_height_view_->GetText(), &height))
    height = kLayoutExampleDefaultChildSize.height();

  return gfx::Size(std::max(0, width), std::max(0, height));
}

gfx::Insets LayoutExampleBase::TextfieldsToInsets(
    const InsetTextfields& textfields,
    const gfx::Insets& default_insets) {
  int top;
  int left;
  int bottom;
  int right;
  if (!base::StringToInt(textfields.top->GetText(), &top))
    top = default_insets.top();
  if (!base::StringToInt(textfields.left->GetText(), &left))
    left = default_insets.left();
  if (!base::StringToInt(textfields.bottom->GetText(), &bottom))
    bottom = default_insets.bottom();
  if (!base::StringToInt(textfields.right->GetText(), &right))
    right = default_insets.right();

  return gfx::Insets(std::max(0, top), std::max(0, left), std::max(0, bottom),
                     std::max(0, right));
}

}  // namespace examples
}  // namespace views
