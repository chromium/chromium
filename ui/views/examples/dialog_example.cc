// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/dialog_example.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::examples {
namespace {

constexpr size_t kFakeModeless =
    static_cast<size_t>(ui::mojom::ModalType::kSystem) + 1;

}  // namespace

template <class DialogType>
class DialogExample::Delegate : public virtual DialogType {
 public:
  explicit Delegate(DialogExample* parent) : parent_(parent) {
    DialogDelegate::SetButtons(parent_->GetDialogButtons());
    DialogDelegate::SetButtonLabel(ui::mojom::DialogButton::kOk,
                                   parent_->ok_button_label_->GetText());
    DialogDelegate::SetButtonLabel(ui::mojom::DialogButton::kCancel,
                                   parent_->cancel_button_label_->GetText());
    DialogDelegate::SetCloseCallback(base::BindRepeating(
        &DialogExample::OnCloseCallback, base::Unretained(parent_)));
    WidgetDelegate::SetModalType(parent_->GetModalType());
  }

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  void InitDelegate() {
    this->SetLayoutManager(std::make_unique<FillLayout>());
    auto body = std::make_unique<Label>(parent_->body_->GetText());
    body->SetMultiLine(true);
    body->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    // Give the example code a way to change the body text.
    parent_->last_body_label_ = this->AddChildView(std::move(body));

    if (parent_->has_extra_button_->GetChecked()) {
      DialogDelegate::SetExtraView(std::make_unique<views::MdTextButton>(
          Button::PressedCallback(), parent_->extra_button_label_->GetText()));
    }
  }

 protected:
  std::u16string GetWindowTitle() const override {
    return parent_->title_->GetText();
  }

  bool Cancel() override { return parent_->AllowDialogClose(false); }
  bool Accept() override { return parent_->AllowDialogClose(true); }

 private:
  raw_ptr<DialogExample> parent_;
};

class DialogExample::Bubble : public Delegate<BubbleDialogDelegateView> {
 public:
  Bubble(DialogExample* parent, View* anchor)
      : BubbleDialogDelegateView(anchor, BubbleBorder::TOP_LEFT),
        Delegate(parent) {
    set_close_on_deactivate(!parent->persistent_bubble_->GetChecked());
  }

  Bubble(const Bubble&) = delete;
  Bubble& operator=(const Bubble&) = delete;

  // BubbleDialogDelegateView:
  void Init() override { InitDelegate(); }
};

class DialogExample::Dialog : public Delegate<DialogDelegateView> {
 public:
  explicit Dialog(DialogExample* parent) : Delegate(parent) {
    // Mac supports resizing of modal dialogs (parent or window-modal). On other
    // platforms this will be weird unless the modal type is "none", but helps
    // test layout.
    SetCanResize(true);
  }

  Dialog(const Dialog&) = delete;
  Dialog& operator=(const Dialog&) = delete;
};

DialogExample::DialogExample()
    : ExampleBase("Dialog"),
      mode_model_({
          ui::SimpleComboboxModel::Item(u"Modeless"),
          ui::SimpleComboboxModel::Item(u"Window Modal"),
          ui::SimpleComboboxModel::Item(u"Child Modal"),
          ui::SimpleComboboxModel::Item(u"System Modal"),
          ui::SimpleComboboxModel::Item(u"Fake Modeless (non-bubbles)"),
      }) {}

DialogExample::~DialogExample() {
  if (title_) {
    title_->set_controller(nullptr);
  }
  if (body_) {
    body_->set_controller(nullptr);
  }
  if (ok_button_label_) {
    ok_button_label_->set_controller(nullptr);
  }
  if (cancel_button_label_) {
    cancel_button_label_->set_controller(nullptr);
  }
  if (extra_button_label_) {
    extra_button_label_->set_controller(nullptr);
  }
}

void DialogExample::CreateExampleView(View* container) {
  auto* flex_layout =
      container->SetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kVertical);

  auto* table = container->AddChildView(std::make_unique<View>());
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  const int horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  auto* table_layout = table->SetLayoutManager(std::make_unique<TableLayout>());
  table_layout
      ->AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStretch,
                  TableLayout::kFixedSize,
                  TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(TableLayout::kFixedSize, horizontal_spacing)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 1.0f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(TableLayout::kFixedSize, horizontal_spacing)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                 TableLayout::kFixedSize,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0);
  const int vertical_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  for (int i = 0; i < 7; ++i) {
    table_layout->AddPaddingRow(TableLayout::kFixedSize, vertical_padding)
        .AddRows(1, TableLayout::kFixedSize);
  }

  StartTextfieldRow(
      table, &title_, l10n_util::GetStringUTF16(IDS_DIALOG_TITLE_LABEL),
      l10n_util::GetStringUTF16(IDS_DIALOG_TITLE_TEXT), nullptr, true);
  StartTextfieldRow(
      table, &body_, l10n_util::GetStringUTF16(IDS_DIALOG_BODY_LABEL),
      l10n_util::GetStringUTF16(IDS_DIALOG_BODY_LABEL), nullptr, true);

  Label* row_label = nullptr;
  StartTextfieldRow(table, &ok_button_label_,
                    l10n_util::GetStringUTF16(IDS_DIALOG_OK_BUTTON_LABEL),
                    l10n_util::GetStringUTF16(IDS_DIALOG_OK_BUTTON_TEXT),
                    &row_label, false);
  AddCheckbox(table, &has_ok_button_, row_label);

  StartTextfieldRow(table, &cancel_button_label_,
                    l10n_util::GetStringUTF16(IDS_DIALOG_CANCEL_BUTTON_LABEL),
                    l10n_util::GetStringUTF16(IDS_DIALOG_CANCEL_BUTTON_TEXT),
                    &row_label, false);
  AddCheckbox(table, &has_cancel_button_, row_label);

  StartTextfieldRow(table, &extra_button_label_,
                    l10n_util::GetStringUTF16(IDS_DIALOG_EXTRA_BUTTON_LABEL),
                    l10n_util::GetStringUTF16(IDS_DIALOG_EXTRA_BUTTON_TEXT),
                    &row_label, false);
  AddCheckbox(table, &has_extra_button_, row_label);

  std::u16string modal_label =
      l10n_util::GetStringUTF16(IDS_DIALOG_MODAL_TYPE_LABEL);
  table->AddChildView(std::make_unique<Label>(modal_label));
  mode_ = table->AddChildView(std::make_unique<Combobox>(&mode_model_));
  mode_->SetCallback(base::BindRepeating(&DialogExample::OnPerformAction,
                                         base::Unretained(this)));
  mode_->SetSelectedIndex(static_cast<size_t>(ui::mojom::ModalType::kChild));
  mode_->GetViewAccessibility().SetName(modal_label);
  table->AddChildView(std::make_unique<View>());

  Label* bubble_label = table->AddChildView(std::make_unique<Label>(
      l10n_util::GetStringUTF16(IDS_DIALOG_BUBBLE_LABEL)));
  AddCheckbox(table, &bubble_, bubble_label);
  AddCheckbox(table, &persistent_bubble_, nullptr);
  persistent_bubble_->SetText(
      l10n_util::GetStringUTF16(IDS_DIALOG_PERSISTENT_LABEL));

  show_ = container->AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&DialogExample::ShowButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_DIALOG_SHOW_BUTTON_LABEL)));
  show_->SetProperty(kCrossAxisAlignmentKey, LayoutAlignment::kCenter);
  show_->SetProperty(
      kMarginsKey,
      gfx::Insets::TLBR(provider->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        0, 0, 0));
}

void DialogExample::StartTextfieldRow(View* parent,
                                      raw_ptr<Textfield>* member,
                                      std::u16string label,
                                      std::u16string value,
                                      Label** created_label,
                                      bool pad_last_col) {
  Label* row_label = parent->AddChildView(std::make_unique<Label>(label));
  if (created_label)
    *created_label = row_label;
  auto textfield = std::make_unique<Textfield>();
  textfield->set_controller(this);
  textfield->SetText(value);
  textfield->GetViewAccessibility().SetName(*row_label);
  *member = parent->AddChildView(std::move(textfield));
  if (pad_last_col)
    parent->AddChildView(std::make_unique<View>());
}

void DialogExample::AddCheckbox(View* parent,
                                raw_ptr<Checkbox>* member,
                                Label* label) {
  auto callback = member == &bubble_ ? &DialogExample::BubbleCheckboxPressed
                                     : &DialogExample::OtherCheckboxPressed;
  auto checkbox = std::make_unique<Checkbox>(
      std::u16string(), base::BindRepeating(callback, base::Unretained(this)));
  checkbox->SetChecked(true);
  if (label)
    checkbox->GetViewAccessibility().SetName(*label);
  *member = parent->AddChildView(std::move(checkbox));
}

ui::mojom::ModalType DialogExample::GetModalType() const {
  // "Fake" modeless happens when a DialogDelegate specifies window-modal, but
  // doesn't provide a parent window.
  // TODO(ellyjones): This doesn't work on Mac at all - something should happen
  // other than changing modality on the fly like this. In fact, it should be
  // impossible to change modality in a live dialog at all, and this example
  // should stop doing it.
  if (mode_->GetSelectedIndex() == kFakeModeless)
    return ui::mojom::ModalType::kWindow;

  return static_cast<ui::mojom::ModalType>(mode_->GetSelectedIndex().value());
}

int DialogExample::GetDialogButtons() const {
  int buttons = 0;
  if (has_ok_button_->GetChecked())
    buttons |= static_cast<int>(ui::mojom::DialogButton::kOk);
  if (has_cancel_button_->GetChecked())
    buttons |= static_cast<int>(ui::mojom::DialogButton::kCancel);
  return buttons;
}

void DialogExample::OnCloseCallback() {
  AllowDialogClose(false);
}

bool DialogExample::AllowDialogClose(bool accept) {
  PrintStatus("Dialog closed with %s.", accept ? "Accept" : "Cancel");
  last_dialog_ = nullptr;
  last_body_label_ = nullptr;
  return true;
}

void DialogExample::ResizeDialog() {
  DCHECK(last_dialog_);
  Widget* widget = last_dialog_->GetWidget();
  gfx::Rect preferred_bounds(widget->GetRestoredBounds());
  preferred_bounds.set_size(widget->non_client_view()->GetPreferredSize({}));

  // Q: Do we need NonClientFrameView::GetWindowBoundsForClientBounds() here?
  // A: When DialogCientView properly feeds back sizes, we do not.
  widget->SetBoundsConstrained(preferred_bounds);

  // For user-resizable dialogs, ensure the window manager enforces any new
  // minimum size.
  widget->OnSizeConstraintsChanged();
}

void DialogExample::ShowButtonPressed() {
  if (bubble_->GetChecked()) {
    // |bubble| will be destroyed by its widget when the widget is destroyed.
    Bubble* bubble = new Bubble(this, show_);
    last_dialog_ = bubble;
    BubbleDialogDelegateView::CreateBubble(bubble);
  } else {
    // |dialog| will be destroyed by its widget when the widget is destroyed.
    Dialog* dialog = new Dialog(this);
    last_dialog_ = dialog;
    dialog->InitDelegate();

    // constrained_window::CreateBrowserModalDialogViews() allows dialogs to
    // be created as MODAL_TYPE_WINDOW without specifying a parent.
    gfx::NativeView parent = gfx::NativeView();
    if (mode_->GetSelectedIndex() != kFakeModeless)
      parent = example_view()->GetWidget()->GetNativeView();

    DialogDelegate::CreateDialogWidget(
        dialog, example_view()->GetWidget()->GetNativeWindow(), parent);
  }
  last_dialog_->GetWidget()->Show();
}

void DialogExample::BubbleCheckboxPressed() {
  if (bubble_->GetChecked() && GetModalType() != ui::mojom::ModalType::kChild) {
    mode_->SetSelectedIndex(static_cast<size_t>(ui::mojom::ModalType::kChild));
    LogStatus("You nearly always want Child Modal for bubbles.");
  }
  persistent_bubble_->SetEnabled(bubble_->GetChecked());
  OnPerformAction();  // Validate the modal type.

  if (!bubble_->GetChecked() &&
      GetModalType() == ui::mojom::ModalType::kChild) {
    // Do something reasonable when simply unchecking bubble and re-enable.
    mode_->SetSelectedIndex(static_cast<size_t>(ui::mojom::ModalType::kWindow));
    OnPerformAction();
  }
}

void DialogExample::OtherCheckboxPressed() {
  // Buttons other than show and bubble are pressed. They are all checkboxes.
  // Update the dialog if there is one.
  if (last_dialog_) {
    // TODO(crbug.com/40799020): This can segfault.
    last_dialog_->DialogModelChanged();
    ResizeDialog();
  }
}

void DialogExample::ContentsChanged(Textfield* sender,
                                    const std::u16string& new_contents) {
  if (!last_dialog_)
    return;

  if (sender == extra_button_label_)
    LogStatus("DialogDelegate can never refresh the extra view.");

  if (sender == title_) {
    last_dialog_->GetWidget()->UpdateWindowTitle();
  } else if (sender == body_) {
    last_body_label_->SetText(new_contents);
  } else {
    last_dialog_->DialogModelChanged();
  }

  ResizeDialog();
}

void DialogExample::OnPerformAction() {
  bool enable =
      bubble_->GetChecked() || GetModalType() != ui::mojom::ModalType::kChild;
#if BUILDFLAG(IS_MAC)
  enable = enable && GetModalType() != ui::mojom::ModalType::kSystem;
#endif
  show_->SetEnabled(enable);
  if (!enable && GetModalType() == ui::mojom::ModalType::kChild) {
    LogStatus("MODAL_TYPE_CHILD can't be used with non-bubbles.");
  }
  if (!enable && GetModalType() == ui::mojom::ModalType::kSystem) {
    LogStatus("MODAL_TYPE_SYSTEM isn't supported on Mac.");
  }
}

}  // namespace views::examples
