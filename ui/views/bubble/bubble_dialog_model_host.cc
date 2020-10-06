// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_model_host.h"

#include <utility>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
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
    case ui::DialogModelField::kCheckbox:
      return DialogContentType::CONTROL;
    case ui::DialogModelField::kTextfield:
      return DialogContentType::CONTROL;
    case ui::DialogModelField::kCombobox:
      return DialogContentType::CONTROL;
  }
  NOTREACHED();
  return DialogContentType::CONTROL;
}

std::unique_ptr<View> CreateCheckboxControl(std::unique_ptr<Checkbox> checkbox,
                                            std::unique_ptr<View> label) {
  auto container = std::make_unique<views::View>();

  // Move the checkbox border up to |container| so that it surrounds both
  // |checkbox| and |label|. This done so that |container| looks like a single
  // Checkbox control that has |label| as its internal label. This method is
  // necessary as Checkbox has no internal support for a StyledLabel, which is
  // required for link support in the checkbox label.
  container->SetBorder(checkbox->CreateDefaultBorder());
  checkbox->SetBorder(nullptr);

  auto* layout = container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  checkbox->SetAssociatedLabel(label.get());

  container->AddChildView(std::move(checkbox));
  container->AddChildView(std::move(label));
  return container;
}

}  // namespace

BubbleDialogModelHost::BubbleDialogModelHost(
    std::unique_ptr<ui::DialogModel> model,
    View* anchor_view,
    BubbleBorder::Arrow arrow)
    : BubbleDialogDelegateView(anchor_view, arrow), model_(std::move(model)) {
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

  SetTitle(model_->title(GetPassKey()));
  SetShowCloseButton(model_->show_close_button(GetPassKey()));
  if (model_->is_alert_dialog(GetPassKey()))
    SetAccessibleRole(ax::mojom::Role::kAlertDialog);

  set_close_on_deactivate(model_->close_on_deactivate(GetPassKey()));

  AddInitialFields();
}

BubbleDialogModelHost::~BubbleDialogModelHost() {
  // Remove children as they may refer to the soon-to-be-destructed model.
  RemoveAllChildViews(true);
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
    return BubbleDialogDelegateView::GetInitiallyFocusedView();

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

  // TODO(pbos): Note that this is in place because GridLayout doesn't handle
  // View removal correctly (keeps stale pointers). This is in place to prevent
  // UAFs between Widget::Close() and destroying |this|.
  // TODO(pbos): This uses a non-nullptr LayoutManager only to prevent infinite
  // recursion in CalculatePreferredSize(). CalculatePreferredSize calls
  // GetHeightForWidth(), which if there is no LayoutManager calls
  // GetPreferredSize(). See https://crbug.com/1128500.
  SetLayoutManager(std::make_unique<GridLayout>());

  // TODO(pbos): Consider turning this into for-each-field remove field.
  RemoveAllChildViews(true);
  field_to_view_.clear();
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
    const DialogContentType field_content_type =
        FieldTypeToContentType(field->type(GetPassKey()));

    if (!first_row) {
      int padding_margin = LayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_UNRELATED_CONTROL_VERTICAL);
      if (last_field_content_type == DialogContentType::CONTROL &&
          field_content_type == DialogContentType::CONTROL) {
        // TODO(pbos): Move DISTANCE_CONTROL_LIST_VERTICAL to
        // views::LayoutProvider and replace "12" here.
        padding_margin = 12;
      }
      DCHECK_NE(padding_margin, -1);
      GetGridLayout()->AddPaddingRow(GridLayout::kFixedSize, padding_margin);
    }

    View* last_view = nullptr;
    switch (field->type(GetPassKey())) {
      case ui::DialogModelField::kButton:
        // TODO(pbos): Add support for buttons that are part of content area.
        continue;
      case ui::DialogModelField::kBodyText:
        last_view = AddOrUpdateBodyText(field->AsBodyText(GetPassKey()));
        break;
      case ui::DialogModelField::kCheckbox:
        last_view = AddOrUpdateCheckbox(field->AsCheckbox(GetPassKey()));
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
    last_field_content_type = field_content_type;

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

  // Set up kTextfieldColumnSetId.
  ColumnSet* const textfield_column_set =
      GetGridLayout()->AddColumnSet(kTextfieldColumnSetId);
  textfield_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                                  GridLayout::kFixedSize,
                                  GridLayout::ColumnSize::kUsePreferred, 0, 0);
  textfield_column_set->AddPaddingColumn(
      GridLayout::kFixedSize,
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL));
  textfield_column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                                  GridLayout::ColumnSize::kFixed, 0, 0);

  // Set up kSingleColumnSetId.
  GetGridLayout()
      ->AddColumnSet(kSingleColumnSetId)
      ->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                  GridLayout::ColumnSize::kUsePreferred, 0, 0);
}

View* BubbleDialogModelHost::AddOrUpdateBodyText(
    ui::DialogModelBodyText* field) {
  // TODO(pbos): Handle updating existing field.

  auto* layout = GetGridLayout();
  layout->StartRow(1.0, kSingleColumnSetId);

  return layout->AddView(CreateViewForLabel(field->label(GetPassKey())));
}

View* BubbleDialogModelHost::AddOrUpdateCheckbox(
    ui::DialogModelCheckbox* field) {
  // TODO(pbos): Handle updating existing field.

  auto* layout = GetGridLayout();
  layout->StartRow(1.0, kSingleColumnSetId);

  auto checkbox = std::make_unique<Checkbox>();
  auto* checkbox_ptr = checkbox.get();

  checkbox->set_callback(base::BindRepeating(
      [](ui::DialogModelCheckbox* model,
         util::PassKey<DialogModelHost> pass_key, Checkbox* checkbox,
         const ui::Event& event) {
        model->OnChecked(pass_key, checkbox->GetChecked());
      },
      field, GetPassKey(), checkbox.get()));

  layout->AddView(CreateCheckboxControl(
      std::move(checkbox), CreateViewForLabel(field->label(GetPassKey()))));

  return checkbox_ptr;
}

View* BubbleDialogModelHost::AddOrUpdateCombobox(
    ui::DialogModelCombobox* model) {
  // TODO(pbos): Handle updating existing field.

  auto combobox = std::make_unique<Combobox>(model->combobox_model());
  combobox->SetAccessibleName(model->accessible_name(GetPassKey()).empty()
                                  ? model->label(GetPassKey())
                                  : model->accessible_name(GetPassKey()));
  combobox->set_callback(base::BindRepeating(
      [](ui::DialogModelCombobox* model,
         util::PassKey<DialogModelHost> pass_key, Combobox* combobox) {
        // TODO(pbos): This should be a subscription through the Combobox
        // directly, but Combobox right now doesn't support listening to
        // selected-index changes.
        model->OnSelectedIndexChanged(pass_key, combobox->GetSelectedIndex());
        model->OnPerformAction(pass_key);
      },
      model, GetPassKey(), combobox.get()));

  // TODO(pbos): Add subscription to combobox selected-index changes.
  combobox->SetSelectedIndex(model->selected_index());
  auto* combobox_ptr = combobox.get();
  AddLabelAndField(model->label(GetPassKey()), std::move(combobox),
                   combobox_ptr->GetFontList());
  return combobox_ptr;
}

View* BubbleDialogModelHost::AddOrUpdateTextfield(
    ui::DialogModelTextfield* model) {
  // TODO(pbos): Support updates to the existing model.

  auto textfield = std::make_unique<Textfield>();
  textfield->SetAccessibleName(model->accessible_name(GetPassKey()).empty()
                                   ? model->label(GetPassKey())
                                   : model->accessible_name(GetPassKey()));
  textfield->SetText(model->text());

  property_changed_subscriptions_.push_back(
      textfield->AddTextChangedCallback(base::BindRepeating(
          [](ui::DialogModelTextfield* model,
             util::PassKey<DialogModelHost> pass_key, Textfield* textfield) {
            model->OnTextChanged(pass_key, textfield->GetText());
          },
          model, GetPassKey(), textfield.get())));

  auto* textfield_ptr = textfield.get();
  AddLabelAndField(model->label(GetPassKey()), std::move(textfield),
                   textfield_ptr->GetFontList());

  return textfield_ptr;
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

void BubbleDialogModelHost::OnViewCreatedForField(View* view,
                                                  ui::DialogModelField* field) {
#if DCHECK_IS_ON()
  // Make sure neither view nor field has been previously used.
  for (const auto& kv : field_to_view_) {
    DCHECK_NE(kv.first, field);
    DCHECK_NE(kv.second, view);
  }
#endif  // DCHECK_IS_ON()
  field_to_view_[field] = view;
  for (const auto& accelerator : field->accelerators(GetPassKey()))
    view->AddAccelerator(accelerator);
}

View* BubbleDialogModelHost::FieldToView(ui::DialogModelField* field) {
  DCHECK(field);
  DCHECK(field_to_view_[field]);
  return field_to_view_[field];
}

std::unique_ptr<View> BubbleDialogModelHost::CreateViewForLabel(
    const ui::DialogModelLabel& dialog_label) {
  if (!dialog_label.links(GetPassKey()).empty()) {
    // Label contains links so it needs a styled label.

    // TODO(pbos): Make sure this works for >1 link, it uses .front() now.
    DCHECK_EQ(dialog_label.links(GetPassKey()).size(), 1u);

    size_t offset;
    const base::string16 link_text = l10n_util::GetStringUTF16(
        dialog_label.links(GetPassKey()).front().message_id);
    const base::string16 text = l10n_util::GetStringFUTF16(
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

  auto text_label = std::make_unique<Label>(
      l10n_util::GetStringUTF16(dialog_label.message_id(GetPassKey())),
      style::CONTEXT_DIALOG_BODY_TEXT,
      dialog_label.is_secondary(GetPassKey()) ? style::STYLE_SECONDARY
                                              : style::STYLE_PRIMARY);
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return text_label;
}

BEGIN_METADATA(BubbleDialogModelHost, BubbleDialogDelegateView)
END_METADATA

}  // namespace views
