// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace views {
namespace {

BubbleDialogModelHost::FieldType GetFieldTypeForField(
    ui::DialogModelField* field,
    base::PassKey<ui::DialogModelHost> pass_key) {
  DCHECK(field);
  switch (field->type(pass_key)) {
    case ui::DialogModelField::kButton:
      return BubbleDialogModelHost::FieldType::kControl;
    case ui::DialogModelField::kBodyText:
      return BubbleDialogModelHost::FieldType::kText;
    case ui::DialogModelField::kCheckbox:
      return BubbleDialogModelHost::FieldType::kControl;
    case ui::DialogModelField::kTextfield:
      return BubbleDialogModelHost::FieldType::kControl;
    case ui::DialogModelField::kCombobox:
      return BubbleDialogModelHost::FieldType::kControl;
    case ui::DialogModelField::kCustom:
      return static_cast<BubbleDialogModelHost::CustomViewFactory*>(
                 field->AsCustomField(pass_key)->factory(pass_key))
          ->GetFieldType();
  }
}

int GetDialogTopMargins(LayoutProvider* layout_provider,
                        ui::DialogModelField* first_field,
                        base::PassKey<ui::DialogModelHost> pass_key) {
  const BubbleDialogModelHost::FieldType field_type =
      first_field ? GetFieldTypeForField(first_field, pass_key)
                  : BubbleDialogModelHost::FieldType::kControl;
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
                           base::PassKey<ui::DialogModelHost> pass_key) {
  const BubbleDialogModelHost::FieldType field_type =
      last_field ? GetFieldTypeForField(last_field, pass_key)
                 : BubbleDialogModelHost::FieldType::kControl;
  switch (field_type) {
    case BubbleDialogModelHost::FieldType::kMenuItem:
      return 0;
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
 public:
  METADATA_HEADER(CheckboxControl);
  CheckboxControl(std::unique_ptr<View> label, int label_line_height)
      : label_line_height_(label_line_height) {
    auto* layout = SetLayoutManager(std::make_unique<BoxLayout>());
    layout->set_between_child_spacing(LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL));
    layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStart);

    SetAssociatedLabel(label.get());

    AddChildView(std::move(label));
  }

  void Layout() override {
    // Skip LabelButton to use LayoutManager.
    View::Layout();
  }

  gfx::Size CalculatePreferredSize() const override {
    // Skip LabelButton to use LayoutManager.
    return View::CalculatePreferredSize();
  }

  int GetHeightForWidth(int width) const override {
    // Skip LabelButton to use LayoutManager.
    return View::GetHeightForWidth(width);
  }

  void OnThemeChanged() override {
    Checkbox::OnThemeChanged();
    // This offsets the image to align with the first line of text. See
    // LabelButton::Layout().
    image()->SetBorder(CreateEmptyBorder(gfx::Insets::TLBR(
        (label_line_height_ - image()->GetPreferredSize().height()) / 2, 0, 0,
        0)));
  }

  const int label_line_height_;
};

BEGIN_METADATA(CheckboxControl, Checkbox)
END_METADATA

}  // namespace

// TODO(pbos): Migrate most code that calls contents_view_->(some View method)
// into this class. This was done in steps to limit the size of the diff.
class BubbleDialogModelHost::ContentsView : public View {
 public:
  explicit ContentsView(ui::DialogModel* model) {
    // Note that between-child spacing is manually handled using kMarginsKey.
    SetLayoutManager(
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  }
};

class BubbleDialogModelHost::LayoutConsensusView : public View {
 public:
  METADATA_HEADER(LayoutConsensusView);
  LayoutConsensusView(LayoutConsensusGroup* group, std::unique_ptr<View> view)
      : group_(group) {
    group->AddView(this);
    SetLayoutManager(std::make_unique<FillLayout>());
    AddChildView(std::move(view));
  }

  ~LayoutConsensusView() override { group_->RemoveView(this); }

  gfx::Size CalculatePreferredSize() const override {
    const gfx::Size group_preferred_size = group_->GetMaxPreferredSize();
    DCHECK_EQ(1u, children().size());
    const gfx::Size child_preferred_size = children()[0]->GetPreferredSize();
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

BEGIN_METADATA(BubbleDialogModelHost, LayoutConsensusView, View)
END_METADATA

BubbleDialogModelHost::LayoutConsensusGroup::LayoutConsensusGroup() = default;
BubbleDialogModelHost::LayoutConsensusGroup::~LayoutConsensusGroup() {
  DCHECK(children_.empty());
}

void BubbleDialogModelHost::LayoutConsensusGroup::AddView(
    LayoutConsensusView* view) {
  children_.insert(view);
  // Because this may change the max preferred/min size, invalidate all child
  // layouts.
  for (auto* child : children_)
    child->InvalidateLayout();
}

void BubbleDialogModelHost::LayoutConsensusGroup::RemoveView(
    LayoutConsensusView* view) {
  children_.erase(view);
}

gfx::Size BubbleDialogModelHost::LayoutConsensusGroup::GetMaxPreferredSize()
    const {
  gfx::Size size;
  for (auto* child : children_) {
    DCHECK_EQ(1u, child->children().size());
    size.SetToMax(child->children().front()->GetPreferredSize());
  }
  return size;
}

gfx::Size BubbleDialogModelHost::LayoutConsensusGroup::GetMaxMinimumSize()
    const {
  gfx::Size size;
  for (auto* child : children_) {
    DCHECK_EQ(1u, child->children().size());
    size.SetToMax(child->children().front()->GetMinimumSize());
  }
  return size;
}

BubbleDialogModelHost::BubbleDialogModelHost(
    std::unique_ptr<ui::DialogModel> model,
    View* anchor_view,
    BubbleBorder::Arrow arrow)
    : BubbleDialogDelegate(anchor_view, arrow),
      model_(std::move(model)),
      contents_view_(
          SetContentsView(std::make_unique<ContentsView>(model_.get()))) {
  model_->set_host(GetPassKey(), this);

  // Dialog callbacks can safely refer to |model_|, they can't be called after
  // Widget::Close() calls WidgetWillClose() synchronously so there shouldn't
  // be any dangling references after model removal.
  SetAcceptCallback(base::BindOnce(&ui::DialogModel::OnDialogAcceptAction,
                                   base::Unretained(model_.get()),
                                   GetPassKey()));
  SetCancelCallback(base::BindOnce(&ui::DialogModel::OnDialogCancelAction,
                                   base::Unretained(model_.get()),
                                   GetPassKey()));
  SetCloseCallback(base::BindOnce(&ui::DialogModel::OnDialogCloseAction,
                                  base::Unretained(model_.get()),
                                  GetPassKey()));

  // WindowClosingCallback happens on native widget destruction which is after
  // |model_| reset. Hence routing this callback through |this| so that we only
  // forward the call to DialogModel::OnWindowClosing if we haven't already been
  // closed.
  RegisterWindowClosingCallback(base::BindOnce(
      &BubbleDialogModelHost::OnWindowClosing, base::Unretained(this)));

  int button_mask = ui::DIALOG_BUTTON_NONE;
  auto* ok_button = model_->ok_button(GetPassKey());
  if (ok_button) {
    button_mask |= ui::DIALOG_BUTTON_OK;
    if (!ok_button->label(GetPassKey()).empty())
      SetButtonLabel(ui::DIALOG_BUTTON_OK, ok_button->label(GetPassKey()));
  }

  auto* cancel_button = model_->cancel_button(GetPassKey());
  if (cancel_button) {
    button_mask |= ui::DIALOG_BUTTON_CANCEL;
    if (!cancel_button->label(GetPassKey()).empty())
      SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                     cancel_button->label(GetPassKey()));
  }

  // TODO(pbos): Consider refactoring ::SetExtraView() so it can be called after
  // the Widget is created and still be picked up. Moving this to
  // OnWidgetInitialized() will not work until then.
  auto* extra_button = model_->extra_button(GetPassKey());
  if (extra_button) {
    SetExtraView(std::make_unique<MdTextButton>(
        base::BindRepeating(&ui::DialogModelButton::OnPressed,
                            base::Unretained(extra_button), GetPassKey()),
        extra_button->label(GetPassKey())));
  }

  SetButtons(button_mask);

  SetTitle(model_->title(GetPassKey()));

  if (model_->override_show_close_button(GetPassKey())) {
    SetShowCloseButton(*model_->override_show_close_button(GetPassKey()));
  } else {
    SetShowCloseButton(!IsModalDialog());
  }

  if (!model_->icon(GetPassKey()).IsEmpty()) {
    // TODO(pbos): Consider adding ImageModel support to SetIcon().
    SetIcon(model_->icon(GetPassKey()).GetImage().AsImageSkia());
    SetShowIcon(true);
  }

  if (model_->is_alert_dialog(GetPassKey()))
    SetAccessibleRole(ax::mojom::Role::kAlertDialog);

  set_internal_name(model_->internal_name(GetPassKey()));

  set_close_on_deactivate(model_->close_on_deactivate(GetPassKey()));

  // TODO(pbos): Reconsider this for dialogs which have no actions (look like
  // menus). This is probably too wide for the TabGroupEditorBubbleView which is
  // currently being converted.
  set_fixed_width(LayoutProvider::Get()->GetDistanceMetric(
      anchor_view ? DISTANCE_BUBBLE_PREFERRED_WIDTH
                  : DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  AddInitialFields();
}

BubbleDialogModelHost::~BubbleDialogModelHost() {
  // Remove children as they may refer to the soon-to-be-destructed model.
  contents_view_->RemoveAllChildViews();
}

std::unique_ptr<BubbleDialogModelHost> BubbleDialogModelHost::CreateModal(
    std::unique_ptr<ui::DialogModel> model,
    ui::ModalType modal_type) {
  DCHECK_NE(modal_type, ui::MODAL_TYPE_NONE);
  auto dialog = std::make_unique<BubbleDialogModelHost>(
      std::move(model), nullptr, BubbleBorder::Arrow::NONE);
  dialog->SetModalType(modal_type);
  return dialog;
}

View* BubbleDialogModelHost::GetInitiallyFocusedView() {
  // TODO(pbos): Migrate this override to use
  // WidgetDelegate::SetInitiallyFocusedView() in constructor once it exists.
  // TODO(pbos): Try to prevent uses of GetInitiallyFocusedView() after Close()
  // and turn this in to a DCHECK for |model_| existence. This should fix
  // https://crbug.com/1130181 for now.
  if (!model_)
    return BubbleDialogDelegate::GetInitiallyFocusedView();

  absl::optional<int> unique_id = model_->initially_focused_field(GetPassKey());

  if (!unique_id)
    return BubbleDialogDelegate::GetInitiallyFocusedView();

  return GetTargetView(
      FindDialogModelHostField(model_->GetFieldByUniqueId(unique_id.value())));
}

void BubbleDialogModelHost::OnWidgetInitialized() {
  // Dialog buttons are added on dialog initialization.
  if (GetOkButton()) {
    AddDialogModelHostFieldForExistingView(
        {model_->ok_button(GetPassKey()), GetOkButton(), nullptr});
  }

  if (GetCancelButton()) {
    AddDialogModelHostFieldForExistingView(
        {model_->cancel_button(GetPassKey()), GetCancelButton(), nullptr});
  }

  if (model_->extra_button(GetPassKey())) {
    DCHECK(GetExtraView());
    AddDialogModelHostFieldForExistingView(
        {model_->extra_button(GetPassKey()), GetExtraView(), nullptr});
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
  model_->OnDialogDestroying(GetPassKey());

  // TODO(pbos): Consider turning this into for-each-field remove field.
  // TODO(pbos): Move this into a better-named call inside contents_view_ to
  // make it clear that the model_ is about to be destroyed.
  contents_view_->RemoveAllChildViews();
  fields_.clear();
  model_.reset();
}

void BubbleDialogModelHost::OnFieldAdded(ui::DialogModelField* field) {
  switch (field->type(GetPassKey())) {
    case ui::DialogModelField::kButton:
      // TODO(pbos): Add support for buttons that are part of content area.
      NOTREACHED();
      return;
    case ui::DialogModelField::kBodyText:
      AddOrUpdateBodyText(field->AsBodyText(GetPassKey()));
      break;
    case ui::DialogModelField::kCheckbox:
      AddOrUpdateCheckbox(field->AsCheckbox(GetPassKey()));
      break;
    case ui::DialogModelField::kCombobox:
      AddOrUpdateCombobox(field->AsCombobox(GetPassKey()));
      break;
    case ui::DialogModelField::kTextfield:
      AddOrUpdateTextfield(field->AsTextfield(GetPassKey()));
      break;
    case ui::DialogModelField::kCustom:
      std::unique_ptr<View> view =
          static_cast<CustomViewFactory*>(
              field->AsCustomField(GetPassKey())->factory(GetPassKey()))
              ->CreateView();
      view->SetID(field->unique_id(GetPassKey()));
      DialogModelHostField info{field, view.get(), nullptr};
      AddDialogModelHostField(std::move(view), info);
      break;
  }
  UpdateSpacingAndMargins();
}

void BubbleDialogModelHost::AddInitialFields() {
  DCHECK(contents_view_->children().empty())
      << "This should only be called once.";

  const auto& fields = model_->fields(GetPassKey());
  for (const auto& field : fields)
    OnFieldAdded(field.get());
}

void BubbleDialogModelHost::UpdateSpacingAndMargins() {
  LayoutProvider* const layout_provider = LayoutProvider::Get();
  gfx::Insets dialog_side_insets =
      layout_provider->GetInsetsMetric(InsetsMetric::INSETS_DIALOG);
  dialog_side_insets.set_top(0);
  dialog_side_insets.set_bottom(0);

  if (!model_->icon(GetPassKey()).IsEmpty()) {
    // If we have a window icon, inset margins additionally to align with
    // title label.
    // TODO(pbos): Reconsider this. Aligning with title gives a massive gap on
    // the left side of the dialog. This style is from
    // ExtensionUninstallDialogView as part of refactoring it to use
    // DialogModel.
    dialog_side_insets.set_left(
        dialog_side_insets.left() + model_->icon(GetPassKey()).Size().width() +
        layout_provider->GetInsetsMetric(INSETS_DIALOG_TITLE).left());
  }

  ui::DialogModelField* first_field = nullptr;
  ui::DialogModelField* last_field = nullptr;
  for (View* const view : contents_view_->children()) {
    ui::DialogModelField* const field =
        FindDialogModelHostField(view).dialog_model_field;

    FieldType field_type = GetFieldTypeForField(field, GetPassKey());

    gfx::Insets side_insets =
        field_type == FieldType::kMenuItem ? gfx::Insets() : dialog_side_insets;

    if (!first_field) {
      first_field = field;
      view->SetProperty(kMarginsKey, side_insets);
    } else {
      int padding_margin = field_type == FieldType::kMenuItem
                               ? 0
                               : layout_provider->GetDistanceMetric(
                                     DISTANCE_UNRELATED_CONTROL_VERTICAL);
      if (last_field &&
          GetFieldTypeForField(last_field, GetPassKey()) ==
              FieldType::kControl &&
          field_type == FieldType::kControl) {
        // TODO(pbos): Move DISTANCE_CONTROL_LIST_VERTICAL to
        // views::LayoutProvider and replace "12" here.
        padding_margin = 12;
      }
      side_insets.set_top(padding_margin);
      view->SetProperty(kMarginsKey, side_insets);
    }
    last_field = field;
  }
  contents_view_->InvalidateLayout();

  set_margins(gfx::Insets::TLBR(
      GetDialogTopMargins(layout_provider, first_field, GetPassKey()), 0,
      GetDialogBottomMargins(layout_provider, last_field, GetPassKey()), 0));
}

void BubbleDialogModelHost::OnWindowClosing() {
  // If the model has been removed we have already notified it of closing on the
  // ::Close() stack.
  if (!model_)
    return;
  model_->OnDialogDestroying(GetPassKey());
  // TODO(pbos): Do we need to reset `model_` and destroy contents? See Close().
}

void BubbleDialogModelHost::AddOrUpdateBodyText(
    ui::DialogModelBodyText* model_field) {
  // TODO(pbos): Handle updating existing field.
  std::unique_ptr<View> view =
      CreateViewForLabel(model_field->label(GetPassKey()));
  DialogModelHostField info{model_field, view.get(), nullptr};
  AddDialogModelHostField(std::move(view), info);
}

void BubbleDialogModelHost::AddOrUpdateCheckbox(
    ui::DialogModelCheckbox* model_field) {
  // TODO(pbos): Handle updating existing field.

  std::unique_ptr<CheckboxControl> checkbox;
  if (DialogModelLabelRequiresStyledLabel(model_field->label(GetPassKey()))) {
    auto label =
        CreateStyledLabelForDialogModelLabel(model_field->label(GetPassKey()));
    const int line_height = label->GetLineHeight();
    checkbox = std::make_unique<CheckboxControl>(std::move(label), line_height);
  } else {
    auto label =
        CreateLabelForDialogModelLabel(model_field->label(GetPassKey()));
    const int line_height = label->GetLineHeight();
    checkbox = std::make_unique<CheckboxControl>(std::move(label), line_height);
  }
  checkbox->SetChecked(model_field->is_checked());

  checkbox->SetCallback(base::BindRepeating(
      [](ui::DialogModelCheckbox* model_field,
         base::PassKey<DialogModelHost> pass_key, Checkbox* checkbox,
         const ui::Event& event) {
        model_field->OnChecked(pass_key, checkbox->GetChecked());
      },
      model_field, GetPassKey(), checkbox.get()));

  DialogModelHostField info{model_field, checkbox.get(), nullptr};
  AddDialogModelHostField(std::move(checkbox), info);
}

void BubbleDialogModelHost::AddOrUpdateCombobox(
    ui::DialogModelCombobox* model_field) {
  // TODO(pbos): Handle updating existing field.

  auto combobox = std::make_unique<Combobox>(model_field->combobox_model());
  combobox->SetAccessibleName(model_field->accessible_name(GetPassKey()).empty()
                                  ? model_field->label(GetPassKey())
                                  : model_field->accessible_name(GetPassKey()));
  combobox->SetCallback(base::BindRepeating(
      [](ui::DialogModelCombobox* model_field,
         base::PassKey<DialogModelHost> pass_key, Combobox* combobox) {
        model_field->OnPerformAction(pass_key);
      },
      model_field, GetPassKey(), combobox.get()));

  combobox->SetSelectedIndex(model_field->selected_index());
  property_changed_subscriptions_.push_back(
      combobox->AddSelectedIndexChangedCallback(base::BindRepeating(
          [](ui::DialogModelCombobox* model_field,
             base::PassKey<DialogModelHost> pass_key, Combobox* combobox) {
            model_field->OnSelectedIndexChanged(pass_key,
                                                combobox->GetSelectedIndex());
          },
          model_field, GetPassKey(), combobox.get())));
  const gfx::FontList& font_list = combobox->GetFontList();
  AddViewForLabelAndField(model_field, model_field->label(GetPassKey()),
                          std::move(combobox), font_list);
}

void BubbleDialogModelHost::AddOrUpdateTextfield(
    ui::DialogModelTextfield* model_field) {
  // TODO(pbos): Support updates to the existing model.

  auto textfield = std::make_unique<Textfield>();
  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce. The
  // placeholder name may need to be pushed into DialogModel, unless we can tie
  // this to the label. Maybe SetAssociatedField on Textfield is sufficient?
  textfield->SetProperty(kSkipAccessibilityPaintChecks, true);
  textfield->SetAccessibleName(
      model_field->accessible_name(GetPassKey()).empty()
          ? model_field->label(GetPassKey())
          : model_field->accessible_name(GetPassKey()));
  textfield->SetText(model_field->text());

  // If this textfield is initially focused the text should be initially
  // selected as well.
  absl::optional<int> initially_focused_field_id =
      model_->initially_focused_field(GetPassKey());
  if (initially_focused_field_id &&
      model_field->unique_id(GetPassKey()) == initially_focused_field_id) {
    textfield->SelectAll(true);
  }

  property_changed_subscriptions_.push_back(
      textfield->AddTextChangedCallback(base::BindRepeating(
          [](ui::DialogModelTextfield* model_field,
             base::PassKey<DialogModelHost> pass_key, Textfield* textfield) {
            model_field->OnTextChanged(pass_key, textfield->GetText());
          },
          model_field, GetPassKey(), textfield.get())));

  const gfx::FontList& font_list = textfield->GetFontList();
  AddViewForLabelAndField(model_field, model_field->label(GetPassKey()),
                          std::move(textfield), font_list);
}

void BubbleDialogModelHost::AddViewForLabelAndField(
    ui::DialogModelField* model_field,
    const std::u16string& label_text,
    std::unique_ptr<View> field,
    const gfx::FontList& field_font) {
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

void BubbleDialogModelHost::AddDialogModelHostField(
    std::unique_ptr<View> view,
    const DialogModelHostField& field_view_info) {
  DCHECK_EQ(view.get(), field_view_info.field_view);

  contents_view_->AddChildView(std::move(view));
  AddDialogModelHostFieldForExistingView(field_view_info);
}

void BubbleDialogModelHost::AddDialogModelHostFieldForExistingView(
    const DialogModelHostField& field_view_info) {
  DCHECK(field_view_info.dialog_model_field);
  DCHECK(field_view_info.field_view);
  DCHECK(contents_view_->Contains(field_view_info.field_view) ||
         field_view_info.field_view == GetOkButton() ||
         field_view_info.field_view == GetCancelButton() ||
         field_view_info.field_view == GetExtraView());
#if DCHECK_IS_ON()
  // Make sure none of the info is already in use.
  for (const auto& info : fields_) {
    DCHECK_NE(info.field_view, field_view_info.field_view);
    DCHECK_NE(info.dialog_model_field, field_view_info.dialog_model_field);
    if (info.focusable_view)
      DCHECK_NE(info.focusable_view, field_view_info.focusable_view);
  }
#endif  // DCHECK_IS_ON()
  fields_.push_back(field_view_info);
  View* const target = GetTargetView(field_view_info);
  for (const auto& accelerator :
       field_view_info.dialog_model_field->accelerators(GetPassKey())) {
    target->AddAccelerator(accelerator);
  }
}

BubbleDialogModelHost::DialogModelHostField
BubbleDialogModelHost::FindDialogModelHostField(ui::DialogModelField* field) {
  for (const auto& info : fields_) {
    if (info.dialog_model_field == field)
      return info;
  }
  NOTREACHED();
  return {};
}

BubbleDialogModelHost::DialogModelHostField
BubbleDialogModelHost::FindDialogModelHostField(View* view) {
  for (const auto& info : fields_) {
    if (info.field_view == view)
      return info;
  }
  NOTREACHED();
  return {};
}

View* BubbleDialogModelHost::GetTargetView(
    const DialogModelHostField& field_view_info) {
  return field_view_info.focusable_view ? field_view_info.focusable_view
                                        : field_view_info.field_view;
}

bool BubbleDialogModelHost::DialogModelLabelRequiresStyledLabel(
    const ui::DialogModelLabel& dialog_label) {
  // Link support only exists in StyledLabel.
  return !dialog_label.links(GetPassKey()).empty();
}

std::unique_ptr<View> BubbleDialogModelHost::CreateViewForLabel(
    const ui::DialogModelLabel& dialog_label) {
  if (DialogModelLabelRequiresStyledLabel(dialog_label))
    return CreateStyledLabelForDialogModelLabel(dialog_label);
  return CreateLabelForDialogModelLabel(dialog_label);
}

std::unique_ptr<StyledLabel>
BubbleDialogModelHost::CreateStyledLabelForDialogModelLabel(
    const ui::DialogModelLabel& dialog_label) {
  DCHECK(DialogModelLabelRequiresStyledLabel(dialog_label));
  // TODO(pbos): Make sure this works for >1 link, it uses .front() now.
  DCHECK_EQ(dialog_label.links(GetPassKey()).size(), 1u);

  size_t offset;
  const std::u16string link_text = l10n_util::GetStringUTF16(
      dialog_label.links(GetPassKey()).front().message_id);
  const std::u16string text = l10n_util::GetStringFUTF16(
      dialog_label.message_id(GetPassKey()), link_text, &offset);

  auto styled_label = std::make_unique<StyledLabel>();
  styled_label->SetText(text);
  styled_label->AddStyleRange(
      gfx::Range(offset, offset + link_text.length()),
      StyledLabel::RangeStyleInfo::CreateForLink(
          dialog_label.links(GetPassKey()).front().callback));

  styled_label->SetDefaultTextStyle(dialog_label.is_secondary(GetPassKey())
                                        ? style::STYLE_SECONDARY
                                        : style::STYLE_PRIMARY);

  return styled_label;
}

std::unique_ptr<Label> BubbleDialogModelHost::CreateLabelForDialogModelLabel(
    const ui::DialogModelLabel& dialog_label) {
  DCHECK(!DialogModelLabelRequiresStyledLabel(dialog_label));

  auto text_label = std::make_unique<Label>(
      dialog_label.GetString(GetPassKey()), style::CONTEXT_DIALOG_BODY_TEXT,
      dialog_label.is_secondary(GetPassKey()) ? style::STYLE_SECONDARY
                                              : style::STYLE_PRIMARY);
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return text_label;
}

bool BubbleDialogModelHost::IsModalDialog() const {
  return GetModalType() != ui::MODAL_TYPE_NONE;
}

}  // namespace views
