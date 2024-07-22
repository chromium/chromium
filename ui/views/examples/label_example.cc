// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/label_example.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/examples_themed_label.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;

namespace views::examples {

namespace {

constexpr auto kAlignments =
    std::to_array<const char* const>({"Left", "Center", "Right", "Head"});

// A Label with a clamped preferred width to demonstrate eliding or wrapping.
class ExamplePreferredSizeLabel : public Label {
  METADATA_HEADER(ExamplePreferredSizeLabel, Label)

 public:
  ExamplePreferredSizeLabel() {
    SetBorder(
        CreateThemedSolidBorder(1, ExamplesColorIds::kColorLabelExampleBorder));
  }

  ExamplePreferredSizeLabel(const ExamplePreferredSizeLabel&) = delete;
  ExamplePreferredSizeLabel& operator=(const ExamplePreferredSizeLabel&) =
      delete;

  ~ExamplePreferredSizeLabel() override = default;

  // Label:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    return gfx::Size(50,
                     Label::CalculatePreferredSize(available_size).height());
  }
};

BEGIN_METADATA(ExamplePreferredSizeLabel)
END_METADATA

// static
constexpr auto kElideBehaviors = std::to_array<const char* const>(
    {"No Elide", "Truncate", "Elide Head", "Elide Middle", "Elide Tail",
     "Elide Email", "Fade Tail"});

}  // namespace

LabelExample::LabelExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_LABEL_SELECT_LABEL).c_str()) {}

LabelExample::~LabelExample() {
  if (textfield_) {
    textfield_->set_controller(nullptr);
  }
  observer_.Reset();
}

void LabelExample::CreateExampleView(View* container) {
  observer_.Observe(container);
  // A very simple label example, followed by additional helpful examples.
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(), 10));
  container->AddChildView(std::make_unique<Label>(u"Hello world!"));

  const char16_t hello_world_hebrew[] =
      u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd!";
  auto label = std::make_unique<Label>(hello_world_hebrew);
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(u"A UTF16 surrogate pair: \x5d0\x5b0");
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  container->AddChildView(std::move(label));

  auto themed_label = std::make_unique<ThemedLabel>();
  themed_label->SetText(u"A left-aligned blue label.");
  themed_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  themed_label->SetEnabledColorId(
      ExamplesColorIds::kColorLabelExampleBlueLabel);
  container->AddChildView(std::move(themed_label));

  label = std::make_unique<Label>(u"Password!");
  label->SetObscured(true);
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(u"A Courier-18 label with shadows.");
  label->SetFontList(gfx::FontList("Courier, 18px"));
  label_ = container->AddChildView(std::move(label));

  label = std::make_unique<ExamplePreferredSizeLabel>();
  label->SetText(
      u"A long label will elide toward its logical end if the text's width "
      u"exceeds the label's available width.");
  container->AddChildView(std::move(label));

  label = std::make_unique<ExamplePreferredSizeLabel>();
  label->SetText(
      u"A multi-line label will wrap onto subsequent lines if the text's width "
      u"exceeds the label's available width, which is helpful for extemely "
      u"long text used to demonstrate line wrapping.");
  label->SetMultiLine(true);
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(u"Label with thick border");
  label->SetBorder(CreateThemedSolidBorder(
      20, ExamplesColorIds::kColorLabelExampleThickBorder));
  container->AddChildView(std::move(label));

  label = std::make_unique<Label>(
      u"A multiline label...\n\n...which supports text selection");
  label->SetSelectable(true);
  label->SetMultiLine(true);
  container->AddChildView(std::move(label));

  AddCustomLabel(container);
}

void LabelExample::MultilineCheckboxPressed() {
  custom_label_->SetMultiLine(multiline_->GetChecked());
}

void LabelExample::ShadowsCheckboxPressed() {
  gfx::ShadowValues shadows;
  if (shadows_->GetChecked()) {
    auto* const cp = custom_label_->GetColorProvider();
    shadows = {
        gfx::ShadowValue(
            gfx::Vector2d(), 1,
            cp->GetColor(ExamplesColorIds::kColorLabelExampleUpperShadow)),
        gfx::ShadowValue(
            gfx::Vector2d(2, 2), 0,
            cp->GetColor(ExamplesColorIds::kColorLabelExampleLowerShadow))};
  }
  custom_label_->SetShadows(shadows);
}

void LabelExample::SelectableCheckboxPressed() {
  custom_label_->SetSelectable(selectable_->GetChecked());
}

void LabelExample::ContentsChanged(Textfield* sender,
                                   const std::u16string& new_contents) {
  custom_label_->SetText(new_contents);
  custom_label_->parent()->parent()->InvalidateLayout();
}

void LabelExample::AddCustomLabel(View* container) {
  std::unique_ptr<View> control_container = std::make_unique<View>();
  control_container->SetBorder(CreateThemedSolidBorder(
      2, ExamplesColorIds::kColorLabelExampleCustomBorder));
  control_container->SetBackground(CreateThemedSolidBackground(
      ExamplesColorIds::kColorLabelExampleCustomBackground));
  control_container->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));

  auto* table = control_container->AddChildView(std::make_unique<View>());
  table->SetLayoutManager(std::make_unique<TableLayout>())
      ->AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStretch,
                  TableLayout::kFixedSize,
                  TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(3, TableLayout::kFixedSize);

  Label* content_label =
      table->AddChildView(std::make_unique<Label>(u"Content: "));
  textfield_ = table->AddChildView(std::make_unique<Textfield>());
  textfield_->SetText(
      u"Use the provided controls to configure the content and presentation of "
      u"this custom label.");
  textfield_->SetEditableSelectionRange(gfx::Range());
  textfield_->set_controller(this);
  textfield_->GetViewAccessibility().SetName(*content_label);

  alignment_ = AddCombobox(table, u"Alignment: ", kAlignments,
                           &LabelExample::AlignmentChanged);
  elide_behavior_ = AddCombobox(table, u"Elide Behavior: ", kElideBehaviors,
                                &LabelExample::ElidingChanged);

  auto* checkboxes =
      control_container->AddChildView(std::make_unique<BoxLayoutView>());
  multiline_ = checkboxes->AddChildView(std::make_unique<Checkbox>(
      u"Multiline", base::BindRepeating(&LabelExample::MultilineCheckboxPressed,
                                        base::Unretained(this))));
  shadows_ = checkboxes->AddChildView(std::make_unique<Checkbox>(
      u"Shadows", base::BindRepeating(&LabelExample::ShadowsCheckboxPressed,
                                      base::Unretained(this))));
  selectable_ = checkboxes->AddChildView(std::make_unique<Checkbox>(
      u"Selectable",
      base::BindRepeating(&LabelExample::SelectableCheckboxPressed,
                          base::Unretained(this))));

  control_container->AddChildView(std::make_unique<View>())
      ->SetPreferredSize(gfx::Size(1, 8));

  custom_label_ = control_container->AddChildView(
      std::make_unique<ExamplePreferredSizeLabel>());
  custom_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  custom_label_->SetElideBehavior(gfx::NO_ELIDE);
  custom_label_->SetText(textfield_->GetText());

  // Disable the text selection checkbox if |custom_label_| does not support
  // text selection.
  selectable_->SetEnabled(custom_label_->IsSelectionSupported());

  container->AddChildView(std::move(control_container));
}

Combobox* LabelExample::AddCombobox(View* parent,
                                    std::u16string name,
                                    base::span<const char* const> items,
                                    void (LabelExample::*function)()) {
  parent->AddChildView(std::make_unique<Label>(name));
  auto* combobox = parent->AddChildView(std::make_unique<Combobox>(
      std::make_unique<ExampleComboboxModel>(items)));
  combobox->SetSelectedIndex(0);
  combobox->GetViewAccessibility().SetName(name);
  combobox->SetCallback(base::BindRepeating(function, base::Unretained(this)));
  return parent->AddChildView(std::move(combobox));
}

void LabelExample::AlignmentChanged() {
  custom_label_->SetHorizontalAlignment(static_cast<gfx::HorizontalAlignment>(
      alignment_->GetSelectedIndex().value()));
}

void LabelExample::ElidingChanged() {
  custom_label_->SetElideBehavior(static_cast<gfx::ElideBehavior>(
      elide_behavior_->GetSelectedIndex().value()));
}

void LabelExample::OnViewThemeChanged(View* observed_view) {
  auto* const cp = observed_view->GetColorProvider();
  gfx::ShadowValues shadows = {
      gfx::ShadowValue(
          gfx::Vector2d(), 1,
          cp->GetColor(ExamplesColorIds::kColorLabelExampleUpperShadow)),
      gfx::ShadowValue(
          gfx::Vector2d(2, 2), 0,
          cp->GetColor(ExamplesColorIds::kColorLabelExampleLowerShadow))};
  label_->SetShadows(shadows);
}

void LabelExample::OnViewIsDeleting(View* observed_view) {
  observer_.Reset();
}

}  // namespace views::examples
