// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_model_host.h"

#include <utility>

#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace views {
namespace {
// Note that textfields and comboboxes share column sets.
constexpr int kTextfieldColumnSetId = 0;
// Column sets used for fields where an individual control spans the entire
// dialog width.
constexpr int kSingleColumnSetId = 1;

DialogContentType FieldTypeToContentType(ui::DialogModelField::Type type) {
  switch (type) {
    case ui::DialogModelField::kButton:
      return DialogContentType::CONTROL;
    case ui::DialogModelField::kBodyText:
      return DialogContentType::TEXT;
    case ui::DialogModelField::kTextfield:
      return DialogContentType::CONTROL;
    case ui::DialogModelField::kCombobox:
      return DialogContentType::CONTROL;
  }
  NOTREACHED();
  return DialogContentType::CONTROL;
}

}  // namespace

BubbleDialogModelHost::BubbleDialogModelHost(
    std::unique_ptr<ui::DialogModel> model)
    : model_(std::move(model)) {
  model_->set_host(GetPassKey(), this);

  ConfigureGridLayout();

  // Dialog callbacks can safely refer to |model_|, they can't be called after
  // Widget::Close() calls WidgetWillClose() synchronously so there shouldn't
  // be any dangling references after model removal.
  SetAcceptCallback(base::BindOnce(&ui::DialogModel::OnDialogAccepted,
                                   base::Unretained(model_.get()),
                                   GetPassKey()));
  SetCancelCallback(base::BindOnce(&ui::DialogModel::OnDialogCancelled,
                                   base::Unretained(model_.get()),
                                   GetPassKey()));
  SetCloseCallback(base::BindOnce(&ui::DialogModel::OnDialogClosed,
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
  // OnDialogInitialized() will not work until then.
  auto* extra_button = model_->extra_button(GetPassKey());
  if (extra_button) {
    OnViewCreatedForField(
        SetExtraView(std::make_unique<MdTextButton>(
            base::BindRepeating(&ui::DialogModelButton::OnPressed,
                                base::Unretained(extra_button), GetPassKey()),
            extra_button->label(GetPassKey()))),
        extra_button);
  }

  SetButtons(button_mask);

  WidgetDelegate::SetTitle(model_->title(GetPassKey()));
  WidgetDelegate::SetShowCloseButton(model_->show_close_button(GetPassKey()));

  AddInitialFields();
}

BubbleDialogModelHost::~BubbleDialogModelHost() {
  // Remove children as they may refer to the soon-to-be-destructed model.
  RemoveAllChildViews(true);
}

View* BubbleDialogModelHost::GetInitiallyFocusedView() {
  base::Optional<int> unique_id = model_->initially_focused_field(GetPassKey());

  if (!unique_id)
    return BubbleDialogDelegateView::GetInitiallyFocusedView();

  return FieldToView(model_->GetFieldByUniqueId(unique_id.value()));
}

void BubbleDialogModelHost::OnDialogInitialized() {
  // Dialog buttons are added on dialog initialization.
  if (GetOkButton())
    OnViewCreatedForField(GetOkButton(), model_->ok_button(GetPassKey()));

  if (GetCancelButton()) {
    OnViewCreatedForField(GetCancelButton(),
                          model_->cancel_button(GetPassKey()));
  }
}

gfx::Size BubbleDialogModelHost::CalculatePreferredSize() const {
  // TODO(pbos): Move DISTANCE_BUBBLE_PREFERRED_WIDTH into views.
  const int width = 320 - margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void BubbleDialogModelHost::Close() {
  DCHECK(model_);
  DCHECK(GetWidget());
  GetWidget()->Close();

  // Synchronously destroy |model_|. Widget::Close() being asynchronous should
  // not be observable by the model.

  // Notify the model of window closing before destroying it (as if
  // Widget::Close)
  model_->OnWindowClosing(GetPassKey());

  // TODO(pbos): Consider turning this into for-each-field remove field.
  RemoveAllChildViews(true);
  view_to_field_.clear();
  model_.reset();
}

void BubbleDialogModelHost::SelectAllText(int unique_id) {
  static_cast<Textfield*>(
      FieldToView(model_->GetTextfieldByUniqueId(unique_id)))
      ->SelectAll(false);
}

void BubbleDialogModelHost::OnFieldAdded(ui::DialogModelField* field) {
  // TODO(pbos): Add support for adding fields while the model is hosted.
  NOTREACHED();
}

void BubbleDialogModelHost::AddInitialFields() {
  // TODO(pbos): Turn this method into consecutive OnFieldAdded(field) calls.

  DCHECK(children().empty()) << "This should only be called once.";

  bool first_row = true;
  const auto& fields = model_->fields(GetPassKey());
  const DialogContentType first_field_content_type =
      fields.empty()
          ? DialogContentType::CONTROL
          : FieldTypeToContentType(fields.front()->type(GetPassKey()));
  DialogContentType last_field_content_type = first_field_content_type;
  for (const auto& field : fields) {
    // TODO(pbos): This needs to take previous field type + next field type into
    // account to do this properly.
    if (!first_row) {
      // TODO(pbos): Move DISTANCE_CONTROL_LIST_VERTICAL to
      // views::LayoutProvider and replace "12" here.
      GetGridLayout()->AddPaddingRow(GridLayout::kFixedSize, 12);
    }

    View* last_view = nullptr;
    switch (field->type(GetPassKey())) {
      case ui::DialogModelField::kButton:
        // TODO(pbos): Add support for buttons that are part of content area.
        continue;
      case ui::DialogModelField::kBodyText:
        last_view = AddOrUpdateBodyText(field->AsBodyText(GetPassKey()));
        break;
      case ui::DialogModelField::kCombobox:
        last_view = AddOrUpdateCombobox(field->AsCombobox(GetPassKey()));
        break;
      case ui::DialogModelField::kTextfield:
        last_view = AddOrUpdateTextfield(field->AsTextfield(GetPassKey()));
        break;
    }

    DCHECK(last_view);
    OnViewCreatedForField(last_view, field.get());
    last_field_content_type = FieldTypeToContentType(field->type(GetPassKey()));

    // TODO(pbos): Update logic here when mixing types.
    first_row = false;
  }

  set_margins(LayoutProvider::Get()->GetDialogInsetsForContentType(
      first_field_content_type, last_field_content_type));
}

void BubbleDialogModelHost::OnWindowClosing() {
  // If the model has been removed we have already notified it of closing on the
  // ::Close() stack.
  if (!model_)
    return;
  model_->OnWindowClosing(GetPassKey());
}

GridLayout* BubbleDialogModelHost::GetGridLayout() {
  return static_cast<GridLayout*>(GetLayoutManager());
}

void BubbleDialogModelHost::ConfigureGridLayout() {
  SetLayoutManager(std::make_unique<GridLayout>());
  LayoutProvider* const provider = LayoutProvider::Get();
  const int between_padding =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL);

  ColumnSet* const textfield_column_set =
      GetGridLayout()->AddColumnSet(kTextfieldColumnSetId);
  textfield_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                                  GridLayout::kFixedSize,
                                  GridLayout::ColumnSize::kUsePreferred, 0, 0);
  textfield_column_set->AddPaddingColumn(GridLayout::kFixedSize,
                                         between_padding);

  textfield_column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                                  GridLayout::ColumnSize::kFixed, 0, 0);

  GetGridLayout()
      ->AddColumnSet(kSingleColumnSetId)
      ->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                  GridLayout::ColumnSize::kUsePreferred, 0, 0);
}

Textfield* BubbleDialogModelHost::AddOrUpdateTextfield(
    ui::DialogModelTextfield* model) {
  // TODO(pbos): Support updates to the existing model.

  auto textfield = std::make_unique<Textfield>();
  textfield->SetAccessibleName(model->accessible_name(GetPassKey()).empty()
                                   ? model->label(GetPassKey())
                                   : model->accessible_name(GetPassKey()));
  textfield->SetText(model->text());

  property_changed_subscriptions_.push_back(textfield->AddTextChangedCallback(
      base::BindRepeating(&BubbleDialogModelHost::NotifyTextfieldTextChanged,
                          base::Unretained(this), textfield.get())));

  auto* textfield_ptr = textfield.get();
  AddLabelAndField(model->label(GetPassKey()), std::move(textfield),
                   textfield_ptr->GetFontList());

  return textfield_ptr;
}

Label* BubbleDialogModelHost::AddOrUpdateBodyText(
    ui::DialogModelBodyText* field) {
  // TODO(pbos): Handle updating existing field.

  auto text_label = std::make_unique<Label>(
      field->text(GetPassKey()), style::CONTEXT_DIALOG_BODY_TEXT,
      field->is_secondary(GetPassKey()) ? style::STYLE_SECONDARY
                                        : style::STYLE_PRIMARY);
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* layout = GetGridLayout();
  layout->StartRow(1.0, kSingleColumnSetId);

  return layout->AddView(std::move(text_label));
}

Combobox* BubbleDialogModelHost::AddOrUpdateCombobox(
    ui::DialogModelCombobox* model) {
  // TODO(pbos): Handle updating existing field.

  auto combobox = std::make_unique<Combobox>(model->combobox_model());
  combobox->SetAccessibleName(model->accessible_name(GetPassKey()).empty()
                                  ? model->label(GetPassKey())
                                  : model->accessible_name(GetPassKey()));
  combobox->set_listener(this);
  // TODO(pbos): Add subscription to combobox selected-index changes.
  combobox->SetSelectedIndex(model->selected_index());
  auto* combobox_ptr = combobox.get();
  AddLabelAndField(model->label(GetPassKey()), std::move(combobox),
                   combobox_ptr->GetFontList());
  return combobox_ptr;
}

void BubbleDialogModelHost::AddLabelAndField(const base::string16& label_text,
                                             std::unique_ptr<View> field,
                                             const gfx::FontList& field_font) {
  constexpr int kFontContext = style::CONTEXT_LABEL;
  constexpr int kFontStyle = style::STYLE_PRIMARY;

  int row_height = LayoutProvider::GetControlHeightForFont(
      kFontContext, kFontStyle, field_font);
  GridLayout* const layout = GetGridLayout();
  layout->StartRow(GridLayout::kFixedSize, kTextfieldColumnSetId, row_height);
  layout->AddView(
      std::make_unique<Label>(label_text, kFontContext, kFontStyle));
  layout->AddView(std::move(field));
}

void BubbleDialogModelHost::NotifyTextfieldTextChanged(Textfield* textfield) {
  view_to_field_[textfield]
      ->AsTextfield(GetPassKey())
      ->OnTextChanged(GetPassKey(), textfield->GetText());
}

void BubbleDialogModelHost::NotifyComboboxSelectedIndexChanged(
    Combobox* combobox) {
  view_to_field_[combobox]
      ->AsCombobox(GetPassKey())
      ->OnSelectedIndexChanged(GetPassKey(), combobox->GetSelectedIndex());
}

void BubbleDialogModelHost::OnPerformAction(Combobox* combobox) {
  // TODO(pbos): This should be a subscription through the Combobox directly,
  // but Combobox right now doesn't support listening to selected-index changes.
  NotifyComboboxSelectedIndexChanged(combobox);

  view_to_field_[combobox]
      ->AsCombobox(GetPassKey())
      ->OnPerformAction(GetPassKey());
}

void BubbleDialogModelHost::OnViewCreatedForField(View* view,
                                                  ui::DialogModelField* field) {
#if DCHECK_IS_ON()
  // Make sure neither view nor field has been previously used.
  for (const auto& kv : view_to_field_) {
    DCHECK_NE(kv.first, view);
    DCHECK_NE(kv.second, field);
  }
#endif  // DCHECK_IS_ON()
  view_to_field_[view] = field;
  for (const auto& accelerator : field->accelerators(GetPassKey()))
    view->AddAccelerator(accelerator);
}

View* BubbleDialogModelHost::FieldToView(ui::DialogModelField* field) {
  DCHECK(field);
  for (auto& kv : view_to_field_) {
    if (kv.second == field)
      return kv.first;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace views
