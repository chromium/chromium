// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/layout_example_base.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
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

namespace views::examples {

namespace {

constexpr int kLayoutExampleVerticalSpacing = 3;
constexpr int kLayoutExampleLeftPadding = 8;
constexpr gfx::Size kLayoutExampleDefaultChildSize(180, 90);

class LayoutPanel : public View {
  METADATA_HEADER(LayoutPanel, View)

 protected:
  void OnThemeChanged() override {
    View::OnThemeChanged();
    SetBorder(CreateSolidBorder(
        1, GetColorProvider()->GetColor(ui::kColorFocusableBorderUnfocused)));
  }
};

BEGIN_METADATA(LayoutPanel)
END_METADATA

std::unique_ptr<Textfield> CreateCommonTextfield(
    TextfieldController* container) {
  auto textfield = std::make_unique<Textfield>();
  textfield->SetDefaultWidthInChars(3);
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
  textfield->SetText(u"0");
  textfield->set_controller(container);
  return textfield;
}

std::unique_ptr<Textfield> CreateCommonTextfieldWithAXName(
    TextfieldController* container,
    std::u16string name) {
  auto text_field = CreateCommonTextfield(container);
  text_field->GetViewAccessibility().SetName(name);
  return text_field;
}

}  // namespace

void LayoutExampleBase::InsetTextfields::ResetControllers() {
  if (left) {
    left->set_controller(nullptr);
  }
  if (top) {
    top->set_controller(nullptr);
  }
  if (right) {
    right->set_controller(nullptr);
  }
  if (bottom) {
    bottom->set_controller(nullptr);
  }
}

LayoutExampleBase::ChildPanel::ChildPanel(LayoutExampleBase* example)
    : example_(example) {
  margin_.left = CreateTextfield(u"Left margin");
  margin_.top = CreateTextfield(u"Top margin");
  margin_.right = CreateTextfield(u"Right margin");
  margin_.bottom = CreateTextfield(u"Bottom margin");
  flex_ = CreateTextfield(u"Flex");
  flex_->SetText(std::u16string());
  SetLayoutManager(std::make_unique<DelegatingLayoutManager>(this));
}

LayoutExampleBase::ChildPanel::~ChildPanel() = default;

bool LayoutExampleBase::ChildPanel::OnMousePressed(
    const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton())
    SetSelected(true);
  return true;
}

ProposedLayout LayoutExampleBase::ChildPanel::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layout;
  if (!size_bounds.is_fully_bounded()) {
    layout.host_size = gfx::Size();
    return layout;
  } else {
    layout.host_size =
        gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  }

  constexpr int kSpacing = 2;
  if (selected_) {
    gfx::Size preferred_size = margin_.top->GetPreferredSize({});
    layout.child_layouts.emplace_back(
        margin_.top.get(), margin_.top->GetVisible(),
        gfx::Rect((layout.host_size.width() - preferred_size.width()) / 2,
                  kSpacing, preferred_size.width(), preferred_size.height()));
    preferred_size = margin_.left->GetPreferredSize({});
    layout.child_layouts.emplace_back(
        margin_.left.get(), margin_.left->GetVisible(),
        gfx::Rect(kSpacing,
                  (layout.host_size.height() - preferred_size.height()) / 2,
                  preferred_size.width(), preferred_size.height()));
    preferred_size = margin_.bottom->GetPreferredSize({});
    layout.child_layouts.emplace_back(
        margin_.bottom.get(), margin_.bottom->GetVisible(),
        gfx::Rect(
            (layout.host_size.width() - preferred_size.width()) / 2,
            layout.host_size.height() - preferred_size.height() - kSpacing,
            preferred_size.width(), preferred_size.height()));
    preferred_size = margin_.right->GetPreferredSize({});
    layout.child_layouts.emplace_back(
        margin_.right.get(), margin_.right->GetVisible(),
        gfx::Rect(layout.host_size.width() - preferred_size.width() - kSpacing,
                  (layout.host_size.height() - preferred_size.height()) / 2,
                  preferred_size.width(), preferred_size.height()));
    preferred_size = flex_->GetPreferredSize({});
    layout.child_layouts.emplace_back(
        flex_.get(), flex_->GetVisible(),
        gfx::Rect((layout.host_size.width() - preferred_size.width()) / 2,
                  (layout.host_size.height() - preferred_size.height()) / 2,
                  preferred_size.width(), preferred_size.height()));
  }
  return layout;
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
      1, GetColorProvider()->GetColor(
             selected_ ? ui::kColorFocusableBorderFocused
                       : ui::kColorFocusableBorderUnfocused)));
}

void LayoutExampleBase::ChildPanel::ContentsChanged(
    Textfield* sender,
    const std::u16string& new_contents) {
  const gfx::Insets margins = LayoutExampleBase::TextfieldsToInsets(margin_);
  if (!margins.IsEmpty())
    SetProperty(kMarginsKey, margins);
  else
    ClearProperty(kMarginsKey);
  example_->RefreshLayoutPanel(sender == flex_);
}

Textfield* LayoutExampleBase::ChildPanel::CreateTextfield(
    const std::u16string& name) {
  return AddChildView(CreateCommonTextfieldWithAXName(this, name));
}

BEGIN_METADATA(LayoutExampleBase, ChildPanel)
END_METADATA

LayoutExampleBase::LayoutExampleBase(const char* title) : ExampleBase(title) {}

LayoutExampleBase::~LayoutExampleBase() {
  if (layout_panel_) {
    layout_panel_->RemoveAllChildViews();
  }
  if (preferred_width_view_) {
    preferred_width_view_->set_controller(nullptr);
  }
  if (preferred_height_view_) {
    preferred_height_view_->set_controller(nullptr);
  }
}

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
  return gfx::Insets::TLBR(std::max(0, top), std::max(0, left),
                           std::max(0, bottom), std::max(0, right));
}

Combobox* LayoutExampleBase::CreateAndAddCombobox(
    const std::u16string& label_text,
    base::span<const char* const> items,
    base::RepeatingClosure combobox_callback) {
  auto* const row = control_panel_->AddChildView(std::make_unique<View>());
  row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLayoutExampleVerticalSpacing));
  row->AddChildView(std::make_unique<Label>(label_text));
  auto* const combobox = row->AddChildView(std::make_unique<Combobox>(
      std::make_unique<ExampleComboboxModel>(items)));
  combobox->SetCallback(std::move(combobox_callback));
  combobox->GetViewAccessibility().SetName(label_text);
  return combobox;
}

Textfield* LayoutExampleBase::CreateAndAddTextfield(
    const std::u16string& label_text) {
  auto* const row = control_panel_->AddChildView(std::make_unique<View>());
  row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLayoutExampleVerticalSpacing));
  auto* label = row->AddChildView(std::make_unique<Label>(label_text));
  auto* text_field = row->AddChildView(CreateCommonTextfield(this));
  text_field->GetViewAccessibility().SetName(*label);
  return text_field;
}

void LayoutExampleBase::CreateMarginsTextFields(const std::u16string& label,
                                                InsetTextfields* textfields) {
  auto* const row = control_panel_->AddChildView(std::make_unique<View>());
  row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLayoutExampleVerticalSpacing));
  row->AddChildView(std::make_unique<Label>(label));

  auto* const container = row->AddChildView(std::make_unique<View>());
  container
      ->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::Orientation::kVertical, gfx::Insets(),
          kLayoutExampleVerticalSpacing))
      ->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  textfields->top = container->AddChildView(CreateCommonTextfieldWithAXName(
      this,
      label + u" " + l10n_util::GetStringUTF16(IDS_LAYOUT_BASE_TOP_LABEL)));
  auto* const middle_row = container->AddChildView(std::make_unique<View>());
  middle_row->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLayoutExampleVerticalSpacing));
  textfields->left = middle_row->AddChildView(CreateCommonTextfieldWithAXName(
      this,
      label + u" " + l10n_util::GetStringUTF16(IDS_LAYOUT_BASE_LEFT_LABEL)));
  textfields->right = middle_row->AddChildView(CreateCommonTextfieldWithAXName(
      this,
      label + u" " + l10n_util::GetStringUTF16(IDS_LAYOUT_BASE_RIGHT_LABEL)));
  textfields->bottom = container->AddChildView(CreateCommonTextfieldWithAXName(
      this,
      label + u" " + l10n_util::GetStringUTF16(IDS_LAYOUT_BASE_BOTTOM_LABEL)));
}

Checkbox* LayoutExampleBase::CreateAndAddCheckbox(
    const std::u16string& label_text,
    base::RepeatingClosure checkbox_callback) {
  return control_panel_->AddChildView(
      std::make_unique<Checkbox>(label_text, std::move(checkbox_callback)));
}

void LayoutExampleBase::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  View* full_panel = container->AddChildView(std::make_unique<View>());

  auto* const manager = full_panel->SetLayoutManager(
      std::make_unique<BoxLayout>(views::BoxLayout::Orientation::kHorizontal));
  layout_panel_ = full_panel->AddChildView(std::make_unique<LayoutPanel>());
  // Expand the layout panel as much as possible.
  manager->SetFlexForView(layout_panel_, 1);

  control_panel_ = full_panel->AddChildView(std::make_unique<View>());
  // Used the preferred width for control panel.
  manager->SetFlexForView(control_panel_, 0);
  control_panel_
      ->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::Orientation::kVertical,
          gfx::Insets::VH(kLayoutExampleVerticalSpacing,
                          kLayoutExampleLeftPadding),
          kLayoutExampleVerticalSpacing))
      ->set_cross_axis_alignment(LayoutAlignment::kStart);

  auto* const row = control_panel_->AddChildView(std::make_unique<View>());
  row->SetLayoutManager(std::make_unique<BoxLayout>(
                            BoxLayout::Orientation::kHorizontal, gfx::Insets(),
                            kLayoutExampleVerticalSpacing))
      ->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  add_button_ = row->AddChildView(std::make_unique<MdTextButton>(
      base::BindRepeating(&LayoutExampleBase::AddButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_LAYOUT_BASE_ADD_LABEL)));

  preferred_width_view_ = row->AddChildView(CreateCommonTextfieldWithAXName(
      this, l10n_util::GetStringUTF16(IDS_LAYOUT_BASE_PREFERRED_WIDTH_LABEL)));
  preferred_width_view_->SetText(
      base::NumberToString16(kLayoutExampleDefaultChildSize.width()));

  preferred_height_view_ = row->AddChildView(CreateCommonTextfieldWithAXName(
      this, l10n_util::GetStringUTF16(IDS_LAYOUT_BASE_PREFERRED_HEIGHT_LABEL)));
  preferred_height_view_->SetText(
      base::NumberToString16(kLayoutExampleDefaultChildSize.height()));

  CreateAdditionalControls();
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

void LayoutExampleBase::AddButtonPressed() {
  auto* const panel =
      layout_panel_->AddChildView(std::make_unique<ChildPanel>(this));
  panel->SetPreferredSize(GetNewChildPanelPreferredSize());
  constexpr int kChildPanelGroup = 100;
  panel->SetGroup(kChildPanelGroup);
  RefreshLayoutPanel(false);
}

}  // namespace views::examples
