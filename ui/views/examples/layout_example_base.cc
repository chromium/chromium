// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/layout_example_base.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/examples/grit/views_examples_resources.h"
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
  FullPanel(const FullPanel&) = delete;
  FullPanel& operator=(const FullPanel&) = delete;
  ~FullPanel() override = default;
};

std::unique_ptr<Textfield> CreateCommonTextfield(
    TextfieldController* container) {
  auto textfield = std::make_unique<Textfield>();
  textfield->SetDefaultWidthInChars(3);
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
  textfield->SetText(base::ASCIIToUTF16("0"));
  textfield->set_controller(container);
  return textfield;
}

}  // namespace

LayoutExampleBase::ChildPanel::ChildPanel(LayoutExampleBase* example)
    : example_(example) {
  margin_.left = CreateTextfield();
  margin_.top = CreateTextfield();
  margin_.right = CreateTextfield();
  margin_.bottom = CreateTextfield();
  flex_ = CreateTextfield();
  flex_->SetText(base::string16());
}

LayoutExampleBase::ChildPanel::~ChildPanel() = default;

void LayoutExampleBase::ChildPanel::Layout() {
  constexpr int kSpacing = 2;
  if (selected_) {
    const gfx::Rect client = GetContentsBounds();
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

bool LayoutExampleBase::ChildPanel::OnMousePressed(
    const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    SetSelected(true);
  return true;
}

void LayoutExampleBase::ChildPanel::SetSelected(bool value) {
  if (value != selected_) {
    selected_ = value;
    OnThemeChanged();
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

int LayoutExampleBase::ChildPanel::GetFlex() const {
  int flex;
  return base::StringToInt(flex_->GetText(), &flex) ? flex : -1;
}

void LayoutExampleBase::ChildPanel::OnThemeChanged() {
  View::OnThemeChanged();
  SetBorder(CreateSolidBorder(
      1, GetNativeTheme()->GetSystemColor(
             selected_ ? ui::NativeTheme::kColorId_FocusedBorderColor
                       : ui::NativeTheme::kColorId_UnfocusedBorderColor)));
}

void LayoutExampleBase::ChildPanel::ContentsChanged(
    Textfield* sender,
    const base::string16& new_contents) {
  const gfx::Insets margins = LayoutExampleBase::TextfieldsToInsets(margin_);
  if (!margins.IsEmpty())
    SetProperty(kMarginsKey, margins);
  else
    ClearProperty(kMarginsKey);
  example_->RefreshLayoutPanel(sender == flex_);
}

Textfield* LayoutExampleBase::ChildPanel::CreateTextfield() {
  auto textfield = std::make_unique<Textfield>();
  textfield->SetDefaultWidthInChars(3);
  textfield->SizeToPreferredSize();
  textfield->SetText(base::ASCIIToUTF16("0"));
  textfield->set_controller(this);
  textfield->SetVisible(false);
  return AddChildView(std::move(textfield));
}

LayoutExampleBase::LayoutExampleBase(const char* title) : ExampleBase(title) {}

LayoutExampleBase::~LayoutExampleBase() = default;

void LayoutExampleBase::RefreshLayoutPanel(bool update_layout) {
  if (update_layout)
    UpdateLayoutManager();
  layout_panel_->InvalidateLayout();
  layout_panel_->SchedulePaint();
}

gfx::Insets LayoutExampleBase::TextfieldsToInsets(
    const InsetTextfields& textfields,
    const gfx::Insets& default_insets) {
  int top, left, bottom, right;
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

Combobox* LayoutExampleBase::CreateAndAddCombobox(
    const base::string16& label_text,
    const char* const* items,
    int count,
    base::RepeatingClosure combobox_callback) {
  auto* const row = control_panel_->AddChildView(std::make_unique<View>());
  row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLayoutExampleVerticalSpacing));
  row->AddChildView(std::make_unique<Label>(label_text));
  auto* const combobox = row->AddChildView(std::make_unique<Combobox>(
      std::make_unique<ExampleComboboxModel>(items, count)));
  combobox->set_callback(std::move(combobox_callback));
  return combobox;
}

Textfield* LayoutExampleBase::CreateAndAddTextfield(
    const base::string16& label_text) {
  auto* const row = control_panel_->AddChildView(std::make_unique<View>());
  row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLayoutExampleVerticalSpacing));
  row->AddChildView(std::make_unique<Label>(label_text));
  return row->AddChildView(CreateCommonTextfield(this));
}

void LayoutExampleBase::CreateMarginsTextFields(
    const base::string16& label_text,
    InsetTextfields* textfields) {
  auto* const row = control_panel_->AddChildView(std::make_unique<View>());
  row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLayoutExampleVerticalSpacing));
  row->AddChildView(std::make_unique<Label>(label_text));

  auto* const container = row->AddChildView(std::make_unique<View>());
  container
      ->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::Orientation::kVertical, gfx::Insets(),
          kLayoutExampleVerticalSpacing))
      ->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  textfields->top = container->AddChildView(CreateCommonTextfield(this));
  auto* const middle_row = container->AddChildView(std::make_unique<View>());
  middle_row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLayoutExampleVerticalSpacing));
  textfields->left = middle_row->AddChildView(CreateCommonTextfield(this));
  textfields->right = middle_row->AddChildView(CreateCommonTextfield(this));
  textfields->bottom = container->AddChildView(CreateCommonTextfield(this));
}

Checkbox* LayoutExampleBase::CreateAndAddCheckbox(
    const base::string16& label_text) {
  return control_panel_->AddChildView(
      std::make_unique<Checkbox>(label_text, this));
}

void LayoutExampleBase::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  View* full_panel = container->AddChildView(std::make_unique<FullPanel>());

  auto* const manager = full_panel->SetLayoutManager(
      std::make_unique<BoxLayout>(views::BoxLayout::Orientation::kHorizontal));
  layout_panel_ = full_panel->AddChildView(std::make_unique<View>());
  layout_panel_->SetBorder(CreateSolidBorder(
      1, layout_panel_->GetNativeTheme()->GetSystemColor(
             ui::NativeTheme::kColorId_UnfocusedBorderColor)));
  manager->SetFlexForView(layout_panel_, 3);

  control_panel_ = full_panel->AddChildView(std::make_unique<View>());
  manager->SetFlexForView(control_panel_, 1);
  control_panel_->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical,
      gfx::Insets(kLayoutExampleVerticalSpacing, kLayoutExampleLeftPadding),
      kLayoutExampleVerticalSpacing));

  auto* const row = control_panel_->AddChildView(std::make_unique<View>());
  row->SetLayoutManager(std::make_unique<BoxLayout>(
                            BoxLayout::Orientation::kHorizontal, gfx::Insets(),
                            kLayoutExampleVerticalSpacing))
      ->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  add_button_ = row->AddChildView(std::make_unique<MdTextButton>(
      this, l10n_util::GetStringUTF16(IDS_LAYOUT_BASE_ADD_LABEL)));

  preferred_width_view_ = row->AddChildView(CreateCommonTextfield(this));
  preferred_width_view_->SetText(
      base::NumberToString16(kLayoutExampleDefaultChildSize.width()));

  preferred_height_view_ = row->AddChildView(CreateCommonTextfield(this));
  preferred_height_view_->SetText(
      base::NumberToString16(kLayoutExampleDefaultChildSize.height()));

  CreateAdditionalControls();
}

void LayoutExampleBase::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == add_button_) {
    auto* const panel =
        layout_panel_->AddChildView(std::make_unique<ChildPanel>(this));
    panel->SetPreferredSize(GetNewChildPanelPreferredSize());
    constexpr int kChildPanelGroup = 100;
    panel->SetGroup(kChildPanelGroup);
    RefreshLayoutPanel(false);
  } else {
    ButtonPressedImpl(sender);
  }
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

}  // namespace examples
}  // namespace views
