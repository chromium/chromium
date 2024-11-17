// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_model_host.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_utils.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_password_combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/theme_tracking_image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace views {
namespace {

// Extra margin to be added to contents view when it's inside a scroll view.
constexpr int kScrollViewVerticalMargin = 2;

BubbleDialogModelHost::FieldType GetFieldTypeForField(
    ui::DialogModelField* field) {
  DCHECK(field);
  switch (field->type()) {
    case ui::DialogModelField::kParagraph:
      return BubbleDialogModelHost::FieldType::kText;
    case ui::DialogModelField::kCheckbox:
      return BubbleDialogModelHost::FieldType::kControl;
    case ui::DialogModelField::kTextfield:
      return BubbleDialogModelHost::FieldType::kControl;
    case ui::DialogModelField::kPasswordField:
      return BubbleDialogModelHost::FieldType::kControl;
    case ui::DialogModelField::kCombobox:
      return BubbleDialogModelHost::FieldType::kControl;
    case ui::DialogModelField::kMenuItem:
      return BubbleDialogModelHost::FieldType::kMenuItem;
    case ui::DialogModelField::kTitleItem:
      // No need to handle titles.
      NOTREACHED();
    case ui::DialogModelField::kSection:
      // TODO(pbos): Handle nested/multiple sections.
      NOTREACHED();
    case ui::DialogModelField::kSeparator:
      return BubbleDialogModelHost::FieldType::kMenuItem;
    case ui::DialogModelField::kCustom:
      return static_cast<BubbleDialogModelHost::CustomView*>(
                 field->AsCustomField()->field())
          ->field_type();
  }
}

int GetFieldTopMargin(LayoutProvider* layout_provider,
                      const BubbleDialogModelHost::FieldType& field_type,
                      const BubbleDialogModelHost::FieldType& last_field_type) {
  DCHECK(layout_provider);
  // Menu item preceded by non-menu item should have margin
  if (field_type == BubbleDialogModelHost::FieldType::kMenuItem &&
      last_field_type == BubbleDialogModelHost::FieldType::kMenuItem) {
    return 0;
  }
  if (field_type == BubbleDialogModelHost::FieldType::kControl &&
      last_field_type == BubbleDialogModelHost::FieldType::kControl) {
    // TODO(pbos): Move DISTANCE_CONTROL_LIST_VERTICAL to views::LayoutProvider
    // and replace "12" here.
    return 12;
  }
  return layout_provider->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_VERTICAL);
}

int GetDialogTopMargins(LayoutProvider* layout_provider,
                        ui::DialogModelField* first_field) {
  CHECK(first_field);

  const BubbleDialogModelHost::FieldType field_type =
      GetFieldTypeForField(first_field);
  switch (field_type) {
    case BubbleDialogModelHost::FieldType::kMenuItem:
      return 0;
    case BubbleDialogModelHost::FieldType::kControl:
      return layout_provider->GetDistanceMetric(
          DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL);
    case BubbleDialogModelHost::FieldType::kText:
      return layout_provider->GetDistanceMetric(
          DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT);
  }
}

int GetDialogBottomMargins(LayoutProvider* layout_provider,
                           ui::DialogModelField* last_field,
                           bool has_buttons) {
  CHECK(last_field);
  const BubbleDialogModelHost::FieldType field_type =
      GetFieldTypeForField(last_field);
  switch (field_type) {
    case BubbleDialogModelHost::FieldType::kMenuItem:
      return has_buttons ? layout_provider->GetDistanceMetric(
                               DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL)
                         : 0;
    case BubbleDialogModelHost::FieldType::kControl:
      return layout_provider->GetDistanceMetric(
          DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL);
    case BubbleDialogModelHost::FieldType::kText:
      return layout_provider->GetDistanceMetric(
          DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT);
  }
}

// A subclass of Checkbox that allows using an external Label/StyledLabel view
// instead of LabelButton's internal label. This is required for the
// Label/StyledLabel to be clickable, while supporting Links which requires a
// StyledLabel.
class CheckboxControl : public Checkbox {
  METADATA_HEADER(CheckboxControl, Checkbox)

 public:
  CheckboxControl(std::unique_ptr<View> label, int label_line_height)
      : label_line_height_(label_line_height) {
    auto* layout = SetLayoutManager(std::make_unique<BoxLayout>());
    layout->set_between_child_spacing(LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);

    // TODO(accessibility): There is no `SetAccessibilityProperties` which takes
    // a labelling view to set the accessible name.
    GetViewAccessibility().SetName(*label.get());

    AddChildView(std::move(label));
  }

  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    // Skip LabelButton to use LayoutManager.
    return View::CalculatePreferredSize(available_size);
  }

  void OnThemeChanged() override {
    Checkbox::OnThemeChanged();
    // This offsets the image to align with the first line of text. See
    // LabelButton::Layout().
    image_container_view()->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(
        (label_line_height_ -
         image_container_view()->GetPreferredSize({}).height()) /
            2,
        0, 0, 0)));
  }

  const int label_line_height_;
};

BEGIN_METADATA(CheckboxControl)
END_METADATA

struct DialogModelHostField {
  raw_ptr<ui::DialogModelField> dialog_model_field = nullptr;

  // View representing the entire field.
  raw_ptr<View> field_view = nullptr;

  // Child view to |field_view|, if any, that's used for focus. For instance,
  // a textfield row would be a container that contains both a
  // views::Textfield and a descriptive label. In this case |focusable_view|
  // would refer to the views::Textfield which is also what would gain focus.
  raw_ptr<View> focusable_view = nullptr;
};

View* GetTargetView(const DialogModelHostField& field_view_info) {
  return field_view_info.focusable_view ? field_view_info.focusable_view.get()
                                        : field_view_info.field_view.get();
}

// TODO(pbos): Consider externalizing this functionality into a different
// format that could feasibly be adopted by LayoutManagers. This is used for
// BoxLayouts (but could be others) to agree on columns' preferred width as a
// replacement for using GridLayout.
class LayoutConsensusGroup {
 public:
  LayoutConsensusGroup() = default;
  ~LayoutConsensusGroup() { DCHECK(children_.empty()); }

  void AddView(View* view) {
    children_.insert(view);
    // Because this may change the max preferred/min size, invalidate all child
    // layouts.
    for (View* child : children_) {
      child->InvalidateLayout();
    }
  }

  void RemoveView(View* view) { children_.erase(view); }

  // Get the union of all preferred sizes within the group.
  gfx::Size GetMaxPreferredSize(const SizeBounds& available_size) const {
    gfx::Size size;
    for (View* child : children_) {
      DCHECK_EQ(1u, child->children().size());
      size.SetToMax(
          child->children().front()->GetPreferredSize(available_size));
    }
    return size;
  }

  // Get the union of all minimum sizes within the group.
  gfx::Size GetMaxMinimumSize() const {
    gfx::Size size;
    for (View* child : children_) {
      DCHECK_EQ(1u, child->children().size());
      size.SetToMax(child->children().front()->GetMinimumSize());
    }
    return size;
  }

 private:
  base::flat_set<raw_ptr<View, CtnExperimental>> children_;
};

class LayoutConsensusView : public View {
  METADATA_HEADER(LayoutConsensusView, View)

 public:
  LayoutConsensusView(LayoutConsensusGroup* group, std::unique_ptr<View> view)
      : group_(group) {
    group->AddView(this);
    SetLayoutManager(std::make_unique<FillLayout>());
    AddChildView(std::move(view));
  }

  ~LayoutConsensusView() override { group_->RemoveView(this); }

  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    const gfx::Size group_preferred_size =
        group_->GetMaxPreferredSize(available_size);
    DCHECK_EQ(1u, children().size());
    const gfx::Size child_preferred_size =
        children()[0]->GetPreferredSize(available_size);
    // TODO(pbos): This uses the max width, but could be configurable to use
    // either direction.
    return gfx::Size(group_preferred_size.width(),
                     child_preferred_size.height());
  }

  gfx::Size GetMinimumSize() const override {
    const gfx::Size group_minimum_size = group_->GetMaxMinimumSize();
    DCHECK_EQ(1u, children().size());
    const gfx::Size child_minimum_size = children()[0]->GetMinimumSize();
    // TODO(pbos): This uses the max width, but could be configurable to use
    // either direction.
    return gfx::Size(group_minimum_size.width(), child_minimum_size.height());
  }

 private:
  const raw_ptr<LayoutConsensusGroup> group_;
};

BEGIN_METADATA(LayoutConsensusView)
END_METADATA

}  // namespace

BubbleDialogModelHost::CustomView::CustomView(std::unique_ptr<View> view,
                                              FieldType field_type,
                                              View* focusable_view)
    : view_(std::move(view)),
      field_type_(field_type),
      focusable_view_(focusable_view) {}

BubbleDialogModelHost::CustomView::~CustomView() = default;

std::unique_ptr<View> BubbleDialogModelHost::CustomView::TransferView() {
  DCHECK(view_);
  return std::move(view_);
}

class BubbleDialogModelHostContentsView final : public DialogModelSectionHost {
  METADATA_HEADER(BubbleDialogModelHostContentsView, DialogModelSectionHost)

 public:
  // TODO(pbos): Break this dependency on BubbleDialogModelHost once most of
  // OnFieldAdded etc. has moved into this class.
  BubbleDialogModelHostContentsView(
      ui::DialogModelSection* contents,
      ui::ElementIdentifier initially_focused_field_id)
      : contents_(contents),
        initially_focused_field_id_(initially_focused_field_id),
        on_field_added_subscription_(
            contents->AddOnFieldAddedCallback(base::BindRepeating(
                &BubbleDialogModelHostContentsView::OnFieldAdded,
                base::Unretained(this)))),
        on_field_changed_subscription_(
            contents->AddOnFieldChangedCallback(base::BindRepeating(
                &BubbleDialogModelHostContentsView::OnFieldChanged,
                base::Unretained(this)))) {
    // Note that between-child spacing is manually handled using kMarginsKey.
    SetOrientation(views::BoxLayout::Orientation::kVertical);

    // Add all fields from the model.
    for (const auto& field : contents_->fields()) {
      OnFieldAdded(field.get());
    }
  }

  [[nodiscard]] base::CallbackListSubscription AddOnContentsChangedCallback(
      base::RepeatingClosure on_contents_changed) {
    return on_contents_changed_.Add(std::move(on_contents_changed));
  }

  // TODO(pbos): Move (most) of the host's OnFieldAdded stuff into here. If
  // anything remains try to have BubbleDialogModelHost observe it directly.
  void OnFieldAdded(ui::DialogModelField* field) {
    switch (field->type()) {
      case ui::DialogModelField::kParagraph:
        AddOrUpdateParagraph(field->AsParagraph());
        break;
      case ui::DialogModelField::kCheckbox:
        AddOrUpdateCheckbox(field->AsCheckbox());
        break;
      case ui::DialogModelField::kCombobox:
        AddOrUpdateCombobox(field->AsCombobox());
        break;
      case ui::DialogModelField::kMenuItem:
        AddOrUpdateMenuItem(field->AsMenuItem());
        break;
      case ui::DialogModelField::kTitleItem:
        // No need to handle titles.
        NOTREACHED();
      case ui::DialogModelField::kSection:
        // TODO(pbos): Handle nested/multiple sections.
        NOTREACHED();
      case ui::DialogModelField::kSeparator:
        AddOrUpdateSeparator(field);
        break;
      case ui::DialogModelField::kTextfield:
        AddOrUpdateTextfield(field->AsTextfield());
        break;
      case ui::DialogModelField::kPasswordField:
        AddOrUpdatePasswordField(field->AsPasswordField());
        break;
      case ui::DialogModelField::kCustom:
        AddOrUpdateCustomField(field->AsCustomField());
        break;
    }
    OnFieldChanged(field);
  }

  void OnFieldChanged(ui::DialogModelField* field) {
    const DialogModelHostField host_field = FindDialogModelHostField(field);

    if (host_field.field_view) {
      host_field.field_view->SetVisible(field->is_visible());
    }
    on_contents_changed_.Notify();
  }

  // TODO(pbos): Remove the need for this method by making sure the host always
  // outlives us. Currently we do outlive them. Widget, WidgetDelegate and
  // RootView lifetimes are complicated.
  void Detach() {
    fields_.clear();
    RemoveAllChildViews();
    contents_ = nullptr;
  }

  void AddOrUpdateParagraph(ui::DialogModelParagraph* model_field) {
    // TODO(pbos): Handle updating existing field.

    std::unique_ptr<View> view =
        model_field->header().empty()
            ? CreateViewForLabel(model_field->label())
            : CreateViewForParagraphWithHeader(model_field->label(),
                                               model_field->header());
    DialogModelHostField info{model_field, view.get(), nullptr};
    view->SetProperty(kElementIdentifierKey, model_field->id());
    AddDialogModelHostField(std::move(view), info);
  }

  void AddOrUpdateCheckbox(ui::DialogModelCheckbox* model_field) {
    // TODO(pbos): Handle updating existing field.

    std::unique_ptr<CheckboxControl> checkbox;
    if (DialogModelLabelRequiresStyledLabel(model_field->label())) {
      auto label = CreateStyledLabelForDialogModelLabel(model_field->label());
      const int line_height = label->GetLineHeight();
      checkbox =
          std::make_unique<CheckboxControl>(std::move(label), line_height);
    } else {
      auto label = CreateLabelForDialogModelLabel(model_field->label());
      const int line_height = label->GetLineHeight();
      checkbox =
          std::make_unique<CheckboxControl>(std::move(label), line_height);
    }
    checkbox->SetChecked(model_field->is_checked());

    checkbox->SetCallback(base::BindRepeating(
        [](ui::DialogModelCheckbox* model_field,
           base::PassKey<DialogModelFieldHost> pass_key, Checkbox* checkbox,
           const ui::Event& event) {
          model_field->OnChecked(pass_key, checkbox->GetChecked());
        },
        model_field, GetPassKey(), checkbox.get()));

    DialogModelHostField info{model_field, checkbox.get(), nullptr};
    AddDialogModelHostField(std::move(checkbox), info);
  }
  void AddOrUpdateCombobox(ui::DialogModelCombobox* model_field) {
    // TODO(pbos): Handle updating existing field.

    auto combobox = std::make_unique<Combobox>(model_field->combobox_model());
    combobox->GetViewAccessibility().SetName(
        model_field->accessible_name().empty()
            ? model_field->label()
            : model_field->accessible_name());
    combobox->SetCallback(base::BindRepeating(
        [](ui::DialogModelCombobox* model_field,
           base::PassKey<DialogModelFieldHost> pass_key,
           Combobox* combobox) { model_field->OnPerformAction(pass_key); },
        model_field, GetPassKey(), combobox.get()));

    combobox->SetSelectedIndex(model_field->selected_index());
    property_changed_subscriptions_.push_back(
        combobox->AddSelectedIndexChangedCallback(base::BindRepeating(
            [](ui::DialogModelCombobox* model_field,
               base::PassKey<DialogModelFieldHost> pass_key,
               Combobox* combobox) {
              model_field->OnSelectedIndexChanged(
                  pass_key, combobox->GetSelectedIndex().value());
            },
            model_field, GetPassKey(), combobox.get())));
    AddViewForLabelAndField(model_field, model_field->label(),
                            std::move(combobox));
  }

  void AddOrUpdateMenuItem(ui::DialogModelMenuItem* model_field) {
    // TODO(pbos): Handle updating existing field.

    // TODO(crbug.com/40224983): Implement this for enabled items. Sorry!
    DCHECK(!model_field->is_enabled());

    auto item = std::make_unique<LabelButton>(
        base::BindRepeating(
            [](ui::DialogModelMenuItem* model_field,
               base::PassKey<DialogModelFieldHost> pass_key,
               const ui::Event& event) {
              model_field->OnActivated(pass_key, event.flags());
            },
            model_field, GetPassKey()),
        model_field->label());
    item->SetImageModel(Button::STATE_NORMAL, model_field->icon());
    // TODO(pbos): Move DISTANCE_CONTROL_LIST_VERTICAL to
    // views::LayoutProvider and replace "12" here. See below for another "12"
    // use that also needs to be replaced.
    item->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(12 / 2, LayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_BUTTON_HORIZONTAL_PADDING))));

    item->SetEnabled(model_field->is_enabled());
    item->SetProperty(kElementIdentifierKey, model_field->id());

    DialogModelHostField info{model_field, item.get(), nullptr};
    AddDialogModelHostField(std::move(item), info);
  }

  void AddOrUpdatePasswordField(ui::DialogModelPasswordField* model_field) {
    // TODO(pbos): Support updates to the existing model.

    auto view = std::make_unique<BoxLayoutView>();
    view->SetOrientation(BoxLayout::Orientation::kVertical);

    std::unique_ptr<Label> error_label;
    if (!model_field->incorrect_password_text().empty()) {
      // TODO(droger): Implement the supporting text directly in Textfield.
      error_label = std::make_unique<Label>(
          model_field->incorrect_password_text(),
          style::CONTEXT_TEXTFIELD_SUPPORTING_TEXT, style::STYLE_INVALID);
      error_label->SetMultiLine(true);
      error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      error_label->SetVisible(false);
    }

    // Use a combobox with empty model, rather than a Textfield, so that it
    // supports the "reveal" functionality (eye icon).
    // TODO(droger): add better support for passwords in the regular Textfield.
    auto password_combobox = std::make_unique<views::EditablePasswordCombobox>(
        std::make_unique<ui::SimpleComboboxModel>(
            std::vector<ui::SimpleComboboxModel::Item>()),
        views::style::CONTEXT_TEXTFIELD, style::STYLE_PRIMARY_MONOSPACED,
        false);
    password_combobox->SetPasswordIconTooltips(
        l10n_util::GetStringUTF16(IDS_SETTINGS_PASSWORD_SHOW),
        l10n_util::GetStringUTF16(IDS_SETTINGS_PASSWORD_HIDE));
    password_combobox->GetViewAccessibility().SetName(
        model_field->accessible_name().empty()
            ? model_field->label()
            : model_field->accessible_name());

    password_combobox->SetCallback(base::BindRepeating(
        [](ui::DialogModelPasswordField* model_field,
           base::PassKey<DialogModelFieldHost> pass_key,
           EditablePasswordCombobox* combobox, Label* error_label) {
          model_field->OnTextChanged(pass_key, combobox->GetText());
          combobox->SetInvalid(false);
          if (error_label) {
            error_label->SetVisible(false);
          }
        },
        model_field, GetPassKey(), password_combobox.get(), error_label.get()));

    property_changed_subscriptions_.push_back(
        model_field->AddOnInvalidateCallback(
            GetPassKey(),
            base::BindRepeating(
                [](EditablePasswordCombobox* combobox, Label* error_label) {
                  combobox->SetText(std::u16string());
                  combobox->SetInvalid(true);
                  if (error_label) {
                    error_label->SetVisible(true);
                  }
                },
                password_combobox.get(), error_label.get())));

    view->AddChildView(std::move(password_combobox));
    if (error_label) {
      view->AddChildView(std::move(error_label));
    }

    AddViewForLabelAndField(model_field, model_field->label(), std::move(view));
  }

  void AddOrUpdateTextfield(ui::DialogModelTextfield* model_field) {
    // TODO(pbos): Support updates to the existing model.

    auto textfield = std::make_unique<Textfield>();
    textfield->GetViewAccessibility().SetName(
        model_field->accessible_name().empty()
            ? model_field->label()
            : model_field->accessible_name());
    textfield->SetText(model_field->text());

    // If this textfield is initially focused the text should be initially
    // selected as well.
    // TODO(pbos): Fix this for non-unique IDs. This should not select all text
    // for all textfields with that ID.
    if (initially_focused_field_id_ &&
        model_field->id() == initially_focused_field_id_) {
      textfield->SelectAll(true);
    }

    property_changed_subscriptions_.push_back(
        textfield->AddTextChangedCallback(base::BindRepeating(
            [](ui::DialogModelTextfield* model_field,
               base::PassKey<DialogModelFieldHost> pass_key,
               Textfield* textfield) {
              model_field->OnTextChanged(pass_key, textfield->GetText());
            },
            model_field, GetPassKey(), textfield.get())));

    AddViewForLabelAndField(model_field, model_field->label(),
                            std::move(textfield));
  }

  void AddOrUpdateCustomField(ui::DialogModelCustomField* model_field) {
    auto* custom_view =
        static_cast<BubbleDialogModelHost::CustomView*>(model_field->field());
    std::unique_ptr<View> view = custom_view->TransferView();
    View* focusable_view = custom_view->TransferFocusableView();
    DCHECK(view);
    view->SetProperty(kElementIdentifierKey, model_field->id());
    DialogModelHostField info{model_field, view.get(), focusable_view};
    AddDialogModelHostField(std::move(view), info);
  }

  void AddOrUpdateSeparator(ui::DialogModelField* model_field) {
    DCHECK_EQ(ui::DialogModelField::Type::kSeparator, model_field->type());
    // TODO(pbos): Support updates to the existing model.

    auto separator = std::make_unique<Separator>();
    DialogModelHostField info{model_field, separator.get(), nullptr};
    AddDialogModelHostField(std::move(separator), info);
  }

  void AddViewForLabelAndField(ui::DialogModelField* model_field,
                               const std::u16string& label_text,
                               std::unique_ptr<View> field) {
    auto box_layout = std::make_unique<BoxLayoutView>();

    box_layout->SetBetweenChildSpacing(LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_CONTROL_HORIZONTAL));

    DialogModelHostField info{model_field, box_layout.get(), field.get()};

    auto label = std::make_unique<Label>(label_text, style::CONTEXT_LABEL,
                                         style::STYLE_PRIMARY);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    box_layout->AddChildView(std::make_unique<LayoutConsensusView>(
        &textfield_first_column_group_, std::move(label)));
    box_layout->SetFlexForView(
        box_layout->AddChildView(std::make_unique<LayoutConsensusView>(
            &textfield_second_column_group_, std::move(field))),
        1);

    AddDialogModelHostField(std::move(box_layout), info);
  }

  static bool DialogModelLabelRequiresStyledLabel(
      const ui::DialogModelLabel& dialog_label) {
    return !dialog_label.replacements().empty();
  }

  static std::unique_ptr<View> CreateViewForLabel(
      const ui::DialogModelLabel& dialog_label) {
    if (DialogModelLabelRequiresStyledLabel(dialog_label)) {
      return CreateStyledLabelForDialogModelLabel(dialog_label);
    }
    return CreateLabelForDialogModelLabel(dialog_label);
  }

  static std::unique_ptr<StyledLabel> CreateStyledLabelForDialogModelLabel(
      const ui::DialogModelLabel& dialog_label) {
    DCHECK(DialogModelLabelRequiresStyledLabel(dialog_label));
    const std::vector<ui::DialogModelLabel::TextReplacement>& replacements =
        dialog_label.replacements();

    // Retrieve the replacements strings to create the text.
    std::vector<std::u16string> string_replacements;
    for (auto replacement : replacements) {
      string_replacements.push_back(replacement.text());
    }
    std::vector<size_t> offsets;
    const std::u16string text = l10n_util::GetStringFUTF16(
        dialog_label.message_id(), string_replacements, &offsets);

    auto styled_label = std::make_unique<StyledLabel>();
    styled_label->SetText(text);
    styled_label->SetDefaultTextStyle(dialog_label.is_secondary()
                                          ? style::STYLE_SECONDARY
                                          : style::STYLE_PRIMARY);

    // Style the replacements as needed.
    DCHECK_EQ(string_replacements.size(), offsets.size());
    for (size_t i = 0; i < replacements.size(); ++i) {
      auto replacement = replacements[i];
      // No styling needed if replacement is neither a link nor emphasized text.
      if (!replacement.callback().has_value() && !replacement.is_emphasized()) {
        continue;
      }

      StyledLabel::RangeStyleInfo style_info;
      if (replacement.callback().has_value()) {
        style_info = StyledLabel::RangeStyleInfo::CreateForLink(
            replacement.callback().value());
        style_info.accessible_name = replacement.accessible_name().value();
      } else if (replacement.is_emphasized()) {
        style_info.text_style = views::style::STYLE_EMPHASIZED;
      }

      auto offset = offsets[i];
      styled_label->AddStyleRange(
          gfx::Range(offset, offset + replacement.text().length()), style_info);
    }

    return styled_label;
  }

  static std::unique_ptr<Label> CreateLabelForDialogModelLabel(
      const ui::DialogModelLabel& dialog_label) {
    DCHECK(!DialogModelLabelRequiresStyledLabel(dialog_label));

    auto text_label = std::make_unique<Label>(
        dialog_label.GetString(), style::CONTEXT_DIALOG_BODY_TEXT,
        dialog_label.is_secondary() ? style::STYLE_SECONDARY
                                    : style::STYLE_PRIMARY);
    text_label->SetMultiLine(true);
    text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    return text_label;
  }

  std::unique_ptr<View> CreateViewForParagraphWithHeader(
      const ui::DialogModelLabel& dialog_label,
      const std::u16string& header) {
    auto view = std::make_unique<BoxLayoutView>();
    view->SetOrientation(BoxLayout::Orientation::kVertical);

    auto* header_label = view->AddChildView(std::make_unique<Label>(
        header, style::CONTEXT_DIALOG_BODY_TEXT, style::STYLE_PRIMARY));
    header_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    view->AddChildView(CreateViewForLabel(dialog_label));
    return view;
  }

  void AddDialogModelHostField(std::unique_ptr<View> view,
                               const DialogModelHostField& field_view_info) {
    DCHECK_EQ(view.get(), field_view_info.field_view);

    AddChildView(std::move(view));

    DCHECK(field_view_info.dialog_model_field);
    DCHECK(field_view_info.field_view);
#if DCHECK_IS_ON()
    // Make sure none of the info is already in use.
    for (const auto& info : fields_) {
      DCHECK_NE(info.field_view, field_view_info.field_view);
      DCHECK_NE(info.dialog_model_field, field_view_info.dialog_model_field);
      if (info.focusable_view) {
        DCHECK_NE(info.focusable_view, field_view_info.focusable_view);
      }
    }
#endif  // DCHECK_IS_ON()
    fields_.push_back(field_view_info);
    View* const target = GetTargetView(field_view_info);
    target->SetProperty(kElementIdentifierKey,
                        field_view_info.dialog_model_field->id());
    for (const auto& accelerator :
         field_view_info.dialog_model_field->accelerators()) {
      target->AddAccelerator(accelerator);
    }
  }

  DialogModelHostField FindDialogModelHostField(ui::DialogModelField* field) {
    for (const auto& info : fields_) {
      if (info.dialog_model_field == field) {
        return info;
      }
    }
    // TODO(pbos): `field` could correspond to a button.
    return {};
  }

  DialogModelHostField FindDialogModelHostField(View* view) {
    for (const auto& info : fields_) {
      if (info.field_view == view) {
        return info;
      }
    }
    NOTREACHED();
  }

 private:
  // TODO(pbos): These should be const as we should never outlive our parent or
  // DialogModelSection. Currently this isn't true because WidgetDelegate gets
  // destroyed by Widget in OnNativeWidgetDestroyed before the content gets
  // destroyed inside DestroyRootView in ~Widget.
  raw_ptr<ui::DialogModelSection> contents_;

  const ui::ElementIdentifier initially_focused_field_id_;

  const base::CallbackListSubscription on_field_added_subscription_;
  const base::CallbackListSubscription on_field_changed_subscription_;

  std::vector<DialogModelHostField> fields_;
  // TODO(pbos): Try to work away this list if the parent can observe enough
  // things as a ViewObserver of us.
  base::RepeatingClosureList on_contents_changed_;

  std::vector<base::CallbackListSubscription> property_changed_subscriptions_;

  LayoutConsensusGroup textfield_first_column_group_;
  LayoutConsensusGroup textfield_second_column_group_;
};

BEGIN_METADATA(BubbleDialogModelHostContentsView)
END_METADATA

std::unique_ptr<DialogModelSectionHost> DialogModelSectionHost::Create(
    ui::DialogModelSection* section,
    ui::ElementIdentifier initially_focused_field_id) {
  return std::make_unique<BubbleDialogModelHostContentsView>(
      section, initially_focused_field_id);
}

BEGIN_METADATA(DialogModelSectionHost)
END_METADATA

BubbleDialogModelHost::ThemeChangedObserver::ThemeChangedObserver(
    BubbleDialogModelHost* parent,
    BubbleDialogModelHostContentsView* contents_view)
    : parent_(parent) {
  observation_.Observe(contents_view);
}
BubbleDialogModelHost::ThemeChangedObserver::~ThemeChangedObserver() = default;

void BubbleDialogModelHost::ThemeChangedObserver::OnViewThemeChanged(View*) {
  parent_->UpdateWindowIcon();
}

BubbleDialogModelHost::BubbleDialogModelHost(
    std::unique_ptr<ui::DialogModel> model,
    View* anchor_view,
    BubbleBorder::Arrow arrow,
    bool autosize)
    : BubbleDialogModelHost(base::PassKey<BubbleDialogModelHost>(),
                            std::move(model),
                            anchor_view,
                            arrow,
                            ui::mojom::ModalType::kNone,
                            autosize) {}

BubbleDialogModelHost::BubbleDialogModelHost(
    base::PassKey<BubbleDialogModelHost>,
    std::unique_ptr<ui::DialogModel> model,
    View* anchor_view,
    BubbleBorder::Arrow arrow,
    ui::mojom::ModalType modal_type,
    bool autosize)
    : BubbleDialogDelegate(anchor_view,
                           arrow,
                           views::BubbleBorder::DIALOG_SHADOW,
                           autosize),
      model_(std::move(model)),
      // Make sure the modal type is set before calling InitContentsView which
      // uses IsModalDialog().
      contents_view_(
          (SetModalType(modal_type), InitContentsView(model_->contents()))),
      on_contents_changed_subscription_(
          contents_view_->AddOnContentsChangedCallback(
              base::BindRepeating(&BubbleDialogModelHost::OnContentsViewChanged,
                                  base::Unretained(this)))),
      theme_observer_(this, contents_view_) {
  model_->set_host(DialogModelHost::GetPassKey(), this);

  // Dialog callbacks can safely refer to |model_|, they can't be called after
  // Widget::Close() calls WidgetWillClose() synchronously so there shouldn't
  // be any dangling references after model removal.
  SetAcceptCallbackWithClose(base::BindRepeating(
      &ui::DialogModel::OnDialogAcceptAction, base::Unretained(model_.get()),
      DialogModelHost::GetPassKey()));
  SetCancelCallbackWithClose(base::BindRepeating(
      &ui::DialogModel::OnDialogCancelAction, base::Unretained(model_.get()),
      DialogModelHost::GetPassKey()));
  SetCloseCallback(base::BindOnce(&ui::DialogModel::OnDialogCloseAction,
                                  base::Unretained(model_.get()),
                                  DialogModelHost::GetPassKey()));

  // WindowClosingCallback happens on native widget destruction which is after
  // |model_| reset. Hence routing this callback through |this| so that we
  // only forward the call to DialogModel::OnWindowClosing if we haven't
  // already been closed.
  RegisterWindowClosingCallback(base::BindOnce(
      &BubbleDialogModelHost::OnWindowClosing, base::Unretained(this)));

  int button_mask = static_cast<int>(ui::mojom::DialogButton::kNone);
  auto* ok_button = model_->ok_button(DialogModelHost::GetPassKey());
  if (ok_button) {
    button_mask |= static_cast<int>(ui::mojom::DialogButton::kOk);
    ConfigureBubbleButtonForParams(*this, /*button_view=*/nullptr,
                                   ui::mojom::DialogButton::kOk, *ok_button);
  }

  auto* cancel_button = model_->cancel_button(DialogModelHost::GetPassKey());
  if (cancel_button) {
    button_mask |= static_cast<int>(ui::mojom::DialogButton::kCancel);
    ConfigureBubbleButtonForParams(*this, /*button_view=*/nullptr,
                                   ui::mojom::DialogButton::kCancel,
                                   *cancel_button);
  }

  // TODO(pbos): Consider refactoring ::SetExtraView() so it can be called
  // after the Widget is created and still be picked up. Moving this to
  // OnWidgetInitialized() will not work until then.
  if (ui::DialogModel::Button* extra_button =
          model_->extra_button(DialogModelHost::GetPassKey())) {
    DCHECK(!model_->extra_link(DialogModelHost::GetPassKey()));
    auto builder =
        views::Builder<MdTextButton>()
            .SetCallback(base::BindRepeating(
                &ui::DialogModel::Button::OnPressed,
                base::Unretained(extra_button), DialogModelHost::GetPassKey()))
            .SetText(extra_button->label());
    if (extra_button->style()) {
      builder.SetStyle(extra_button->style().value());
    }
    builder.SetEnabled(extra_button->is_enabled());
    SetExtraView(std::move(builder).Build());
  } else if (ui::DialogModelLabel::TextReplacement* extra_link =
                 model_->extra_link(DialogModelHost::GetPassKey())) {
    DCHECK(extra_link->callback().has_value());
    auto link = std::make_unique<views::Link>(extra_link->text());
    link->SetCallback(extra_link->callback().value());
    SetExtraView(std::move(link));
  }

  SetButtons(button_mask);

  if (model_->override_default_button(DialogModelHost::GetPassKey())) {
    SetDefaultButton(static_cast<int>(
        model_->override_default_button(DialogModelHost::GetPassKey())
            .value()));
  }

  SetTitle(model_->title(DialogModelHost::GetPassKey()));

  if (!model_->accessible_title(DialogModelHost::GetPassKey()).empty()) {
    SetAccessibleTitle(model_->accessible_title(DialogModelHost::GetPassKey()));
  }

  SetSubtitle(model_->subtitle(DialogModelHost::GetPassKey()));
  // This is added due to crbug.com/1518993 which adds a subtitle that needs
  // line wrapping. If any future dialogs need the subtitle to not line wrap,
  // this behavior will need to be configurable via the builder.
  SetSubtitleAllowCharacterBreak(true);

  if (!model_->main_image(DialogModelHost::GetPassKey()).IsEmpty()) {
    SetMainImage(model_->main_image(DialogModelHost::GetPassKey()));
  }

  if (model_->override_show_close_button(DialogModelHost::GetPassKey())) {
    SetShowCloseButton(
        *model_->override_show_close_button(DialogModelHost::GetPassKey()));
  } else {
    SetShowCloseButton(!IsModalDialog());
  }

  if (!model_->icon(DialogModelHost::GetPassKey()).IsEmpty()) {
    SetIcon(model_->icon(DialogModelHost::GetPassKey()));
    SetShowIcon(true);
  }

  if (model_->is_alert_dialog(DialogModelHost::GetPassKey())) {
#if BUILDFLAG(IS_WIN)
    // This is taken from LocationBarBubbleDelegateView. See
    // GetAccessibleRoleForReason(). crbug.com/1125118: Windows ATs only
    // announce these bubbles if the alert role is used, despite it not being
    // the most appropriate choice.
    // TODO(accessibility): review the role mappings for alerts and dialogs,
    // making sure they are translated to the best candidate in each flatform
    // without resorting to hacks like this.
    SetAccessibleWindowRole(ax::mojom::Role::kAlert);
#else
    SetAccessibleWindowRole(ax::mojom::Role::kAlertDialog);
#endif
  }

  set_internal_name(model_->internal_name(DialogModelHost::GetPassKey()));

  set_close_on_deactivate(
      model_->close_on_deactivate(DialogModelHost::GetPassKey()));

  // TODO(pbos): Reconsider this for dialogs which have no actions (look like
  // menus). This is probably too wide for the TabGroupEditorBubbleView which is
  // currently being converted.
  set_fixed_width(LayoutProvider::Get()->GetDistanceMetric(
      anchor_view ? DISTANCE_BUBBLE_PREFERRED_WIDTH
                  : DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  if (model_->footnote_label()) {
    SetFootnoteView(BubbleDialogModelHostContentsView::CreateViewForLabel(
        *model_->footnote_label()));
  }

  // Make sure we're up to date with initial contents state.
  UpdateSpacingAndMargins();
}

BubbleDialogModelHost::~BubbleDialogModelHost() {
  // Detach ContentsView as it's referring to state that's about to be
  // destroyed.
  contents_view_->Detach();
}

std::unique_ptr<BubbleDialogModelHost> BubbleDialogModelHost::CreateModal(
    std::unique_ptr<ui::DialogModel> model,
    ui::mojom::ModalType modal_type,
    bool autosize) {
  DCHECK_NE(modal_type, ui::mojom::ModalType::kNone);
  return std::make_unique<BubbleDialogModelHost>(
      base::PassKey<BubbleDialogModelHost>(), std::move(model), nullptr,
      BubbleBorder::Arrow::NONE, modal_type, autosize);
}

View* BubbleDialogModelHost::GetInitiallyFocusedView() {
  // TODO(pbos): Migrate this override to use
  // WidgetDelegate::SetInitiallyFocusedView() in constructor once it exists.
  // TODO(pbos): Try to prevent uses of GetInitiallyFocusedView() after Close()
  // and turn this in to a DCHECK for |model_| existence. This should fix
  // https://crbug.com/1130181 for now.
  if (!model_)
    return BubbleDialogDelegate::GetInitiallyFocusedView();

  // TODO(pbos): Reconsider the uniqueness requirement, maybe this should select
  // the first one? If so add corresponding GetFirst query to DialogModel.
  ui::ElementIdentifier unique_id =
      model_->initially_focused_field(DialogModelHost::GetPassKey());

  if (!unique_id)
    return BubbleDialogDelegate::GetInitiallyFocusedView();

  if (ui::DialogModel::Button* const ok_button =
          model_->ok_button(DialogModelHost::GetPassKey());
      ok_button && unique_id == ok_button->id()) {
    return GetOkButton();
  }

  if (ui::DialogModel::Button* const cancel_button =
          model_->cancel_button(DialogModelHost::GetPassKey());
      cancel_button && unique_id == cancel_button->id()) {
    return GetCancelButton();
  }

  if (ui::DialogModel::Button* const extra_button =
          model_->extra_button(DialogModelHost::GetPassKey());
      extra_button && unique_id == extra_button->id()) {
    return GetExtraView();
  }

  return GetTargetView(contents_view_->FindDialogModelHostField(
      model_->GetFieldByUniqueId(unique_id)));
}

void BubbleDialogModelHost::OnWidgetInitialized() {
  // Dialog buttons are added on dialog initialization.
  UpdateDialogButtons();

  if (const ui::ImageModel& banner =
          model_->banner(DialogModelHost::GetPassKey());
      !banner.IsEmpty()) {
    const ui::ImageModel& dark_mode_banner =
        model_->dark_mode_banner(DialogModelHost::GetPassKey());
    auto banner_view = std::make_unique<ThemeTrackingImageView>(
        banner.Rasterize(contents_view_->GetColorProvider()),
        (dark_mode_banner.IsEmpty() ? banner : dark_mode_banner)
            .Rasterize(contents_view_->GetColorProvider()),
        base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                            base::Unretained(this)));
    // The banner is supposed to be purely decorative.
    banner_view->GetViewAccessibility().SetIsIgnored(true);
    GetBubbleFrameView()->SetHeaderView(std::move(banner_view));
  }
}

void BubbleDialogModelHost::Close() {
  DCHECK(model_);
  DCHECK(GetWidget());
  GetWidget()->Close();

  // Synchronously destroy |model_|. Widget::Close() being asynchronous should
  // not be observable by the model or client code.

  // Notify the model of window closing before destroying it (as if
  // Widget::Close)
  model_->OnDialogDestroying(DialogModelHost::GetPassKey());

  // Detach ContentsView as it's referring to state that's about to be
  // destroyed.
  contents_view_->Detach();
  model_.reset();
}

BubbleDialogModelHostContentsView* BubbleDialogModelHost::InitContentsView(
    ui::DialogModelSection* contents) {
  auto contents_view_unique =
      std::make_unique<BubbleDialogModelHostContentsView>(
          contents,
          model_->initially_focused_field(DialogModelHost::GetPassKey()));

  BubbleDialogModelHostContentsView* const contents_view =
      contents_view_unique.get();

  if (IsModalDialog()) {
    // Margins are added directly in the dialog. When the dialog is modal, these
    // contents are wrapped by a scroll view and margins are added outside of it
    // (instead of outside this contents). This causes some items (e.g
    // emphasized buttons) to be cut by the scroll view margins (see
    // crbug.com/1360772). Since we do want the margins outside the scroll view
    // (so they are always present when scrolling), we add
    // `kScrollViewVerticalMargin` inside the contents view and later remove it
    // from the dialog margins.
    // TODO(crbug.com/40855129): Remove this workaround when contents view
    // directly supports a scroll view.
    contents_view_unique->SetInsideBorderInsets(
        gfx::Insets::VH(kScrollViewVerticalMargin, 0));

    // TODO(crbug.com/40855129): Non modal dialogs size is not dependent on its
    // content. Thus, the content has to be manually set by the view inside a
    // scroll view. Modal dialogs handle their own size via constrained windows,
    // so we can add a scroll view to the DialogModel directly.
    const int kMaxDialogHeight = LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT);
    auto scroll_view = std::make_unique<views::ScrollView>();
    scroll_view->ClipHeightTo(0, kMaxDialogHeight);
    scroll_view->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    scroll_view->SetContents(std::move(contents_view_unique));
    SetContentsView(std::move(scroll_view));
  } else {
    SetContentsView(std::move(contents_view_unique));
  }
  return contents_view;
}

void BubbleDialogModelHost::OnContentsViewChanged() {
  UpdateSpacingAndMargins();
}

void BubbleDialogModelHost::OnDialogButtonChanged() {
  UpdateDialogButtons();
}

void BubbleDialogModelHost::UpdateWindowIcon() {
  if (!ShouldShowWindowIcon()) {
    return;
  }
  const ui::ImageModel dark_mode_icon =
      model_->dark_mode_icon(DialogModelHost::GetPassKey());
  if (!dark_mode_icon.IsEmpty() && color_utils::IsDark(GetBackgroundColor())) {
    SetIcon(dark_mode_icon);
    return;
  }
  SetIcon(model_->icon(DialogModelHost::GetPassKey()));
}

void BubbleDialogModelHost::UpdateSpacingAndMargins() {
  LayoutProvider* const layout_provider = LayoutProvider::Get();
  gfx::Insets dialog_side_insets =
      layout_provider->GetInsetsMetric(InsetsMetric::INSETS_DIALOG);
  if (GetWindowTitle().empty()) {
    // If there is no title, increase the margin at the top to match the title
    // margin, so that the text is not too close to the top edge.
    dialog_side_insets.set_top(
        layout_provider->GetInsetsMetric(InsetsMetric::INSETS_DIALOG_TITLE)
            .top());
  } else {
    dialog_side_insets.set_top(0);
  }
  dialog_side_insets.set_bottom(0);

  ui::DialogModelField* first_field = nullptr;
  ui::DialogModelField* last_field = nullptr;

  auto* scroll_view =
      views::ScrollView::GetScrollViewForContents(contents_view_);
  const views::View::Views& children = scroll_view
                                           ? scroll_view->contents()->children()
                                           : contents_view_->children();

  if (children.empty()) {
    // TODO(pbos): Copied from the BubbleDialogDelegate constructor. Maybe there
    // should be a way to reset them if we become empty?
    set_margins(layout_provider->GetDialogInsetsForContentType(
        DialogContentType::kText, DialogContentType::kText));
    return;
  }

  for (View* const view : children) {
    ui::DialogModelField* const field =
        contents_view_->FindDialogModelHostField(view).dialog_model_field;

    const FieldType field_type = GetFieldTypeForField(field);

    gfx::Insets side_insets =
        field_type == FieldType::kMenuItem ? gfx::Insets() : dialog_side_insets;

    if (!first_field) {
      first_field = field;
      view->SetProperty(kMarginsKey, side_insets);
    } else {
      DCHECK(last_field);

      const FieldType last_field_type = GetFieldTypeForField(last_field);
      side_insets.set_top(
          GetFieldTopMargin(layout_provider, field_type, last_field_type));
      view->SetProperty(kMarginsKey, side_insets);
    }
    last_field = field;
  }

  contents_view_->InvalidateLayout();
  // Set margins based on the first and last item. Note that we remove margins
  // that were already added to contents view at construction.
  // TODO(crbug.com/40855129): Remove the extra margin workaround when contents
  // view directly supports a scroll view.
  const int extra_margin = scroll_view ? kScrollViewVerticalMargin : 0;
  const int top_margin =
      GetDialogTopMargins(layout_provider, first_field) - extra_margin;
  const int bottom_margin =
      GetDialogBottomMargins(
          layout_provider, last_field,
          buttons() != static_cast<int>(ui::mojom::DialogButton::kNone)) -
      extra_margin;
  set_margins(gfx::Insets::TLBR(top_margin >= 0 ? top_margin : 0, 0,
                                bottom_margin >= 0 ? bottom_margin : 0, 0));
}

void BubbleDialogModelHost::OnWindowClosing() {
  // If the model has been removed we have already notified it of closing on the
  // ::Close() stack.
  if (!model_)
    return;
  model_->OnDialogDestroying(DialogModelHost::GetPassKey());
  // TODO(pbos): Do we need to reset `model_` and destroy contents? See Close().
}

void BubbleDialogModelHost::UpdateDialogButtons() {
  if (ui::DialogModel::Button* const ok_button =
          model_->ok_button(DialogModelHost::GetPassKey())) {
    ConfigureBubbleButtonForParams(*this, GetOkButton(),
                                   ui::mojom::DialogButton::kOk, *ok_button);
  }
  if (ui::DialogModel::Button* const cancel_button =
          model_->cancel_button(DialogModelHost::GetPassKey())) {
    ConfigureBubbleButtonForParams(*this, GetCancelButton(),
                                   ui::mojom::DialogButton::kCancel,
                                   *cancel_button);
  }
  if (ui::DialogModel::Button* const extra_button =
          model_->extra_button(DialogModelHost::GetPassKey())) {
    auto* const extra_button_view = static_cast<MdTextButton*>(GetExtraView());
    extra_button_view->SetText(extra_button->label());
    extra_button_view->SetVisible(extra_button->is_visible());
    extra_button_view->SetEnabled(extra_button->is_enabled());
    extra_button_view->SetProperty(kElementIdentifierKey, extra_button->id());
  }
}

bool BubbleDialogModelHost::IsModalDialog() const {
  return GetModalType() != ui::mojom::ModalType::kNone;
}

}  // namespace views
