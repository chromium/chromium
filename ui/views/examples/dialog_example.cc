// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/dialog_example.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {
namespace {

constexpr int kFieldsColumnId = 0;
constexpr int kButtonsColumnId = 1;
constexpr int kFakeModeless = ui::MODAL_TYPE_SYSTEM + 1;

}  // namespace

template <class DialogType>
class DialogExample::Delegate : public virtual DialogType {
 public:
  explicit Delegate(DialogExample* parent) : parent_(parent) {}

  void InitDelegate() {
    this->SetLayoutManager(std::make_unique<FillLayout>());
    Label* body = new Label(parent_->body_->GetText());
    body->SetMultiLine(true);
    body->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    body->SetBackground(CreateSolidBackground(SkColorSetRGB(0, 255, 255)));
    this->AddChildView(body);

    if (parent_->has_extra_button_->GetChecked()) {
      DialogDelegate::SetExtraView(MdTextButton::CreateSecondaryUiButton(
          nullptr, parent_->extra_button_label_->GetText()));
    }

    // Give the example code a way to change the body text.
    parent_->last_body_label_ = body;
  }

 protected:
  // WidgetDelegate:
  ui::ModalType GetModalType() const override {
    return parent_->GetModalType();
  }

  base::string16 GetWindowTitle() const override {
    return parent_->title_->GetText();
  }

  bool Cancel() override { return parent_->AllowDialogClose(false); }
  bool Accept() override { return parent_->AllowDialogClose(true); }
  int GetDialogButtons() const override { return parent_->GetDialogButtons(); }
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override {
    if (button == ui::DIALOG_BUTTON_OK)
      return parent_->ok_button_label_->GetText();
    if (button == ui::DIALOG_BUTTON_CANCEL)
      return parent_->cancel_button_label_->GetText();
    return base::string16();
  }

 private:
  DialogExample* parent_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

class DialogExample::Bubble : public Delegate<BubbleDialogDelegateView> {
 public:
  Bubble(DialogExample* parent, View* anchor)
      : BubbleDialogDelegateView(anchor, BubbleBorder::TOP_LEFT),
        Delegate(parent) {
    set_close_on_deactivate(!parent->persistent_bubble_->GetChecked());
  }

  // BubbleDialogDelegateView:
  void Init() override { InitDelegate(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(Bubble);
};

class DialogExample::Dialog : public Delegate<DialogDelegateView> {
 public:
  explicit Dialog(DialogExample* parent) : Delegate(parent) {}

  // WidgetDelegate:
  bool CanResize() const override {
    // Mac supports resizing of modal dialogs (parent or window-modal). On other
    // platforms this will be weird unless the modal type is "none", but helps
    // test layout.
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Dialog);
};

DialogExample::DialogExample()
    : ExampleBase("Dialog"),
      mode_model_({
          base::ASCIIToUTF16("Modeless"), base::ASCIIToUTF16("Window Modal"),
          base::ASCIIToUTF16("Child Modal"), base::ASCIIToUTF16("System Modal"),
          base::ASCIIToUTF16("Fake Modeless (non-bubbles)"),
      }) {}

DialogExample::~DialogExample() = default;

void DialogExample::CreateExampleView(View* container) {
  // GridLayout |resize_percent| constants.
  const float kFixed = 0.f;
  const float kStretchy = 1.f;

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  const int horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());
  ColumnSet* column_set = layout->AddColumnSet(kFieldsColumnId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, kFixed,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(kFixed, horizontal_spacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, kStretchy,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(kFixed, horizontal_spacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, kFixed,
                        GridLayout::USE_PREF, 0, 0);
  StartTextfieldRow(layout, &title_, "Dialog Title", "Title");
  StartTextfieldRow(layout, &body_, "Dialog Body Text", "Body Text");

  StartTextfieldRow(layout, &ok_button_label_, "OK Button Label", "Done");
  AddCheckbox(layout, &has_ok_button_);

  StartTextfieldRow(layout, &cancel_button_label_, "Cancel Button Label",
                    "Cancel");
  AddCheckbox(layout, &has_cancel_button_);

  StartTextfieldRow(layout, &extra_button_label_, "Extra Button Label", "Edit");
  AddCheckbox(layout, &has_extra_button_);

  StartRowWithLabel(layout, "Modal Type");
  mode_ = layout->AddView(std::make_unique<Combobox>(&mode_model_));
  mode_->set_listener(this);
  mode_->SetSelectedIndex(ui::MODAL_TYPE_CHILD);

  StartRowWithLabel(layout, "Bubble");
  AddCheckbox(layout, &bubble_);
  AddCheckbox(layout, &persistent_bubble_);
  persistent_bubble_->SetText(base::ASCIIToUTF16("Persistent"));

  column_set = layout->AddColumnSet(kButtonsColumnId);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, kStretchy,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRowWithPadding(
      kFixed, kButtonsColumnId, kFixed,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  show_ = layout->AddView(
      MdTextButton::CreateSecondaryUiButton(this, base::ASCIIToUTF16("Show")));
}

void DialogExample::StartRowWithLabel(GridLayout* layout, const char* label) {
  const float kFixedVerticalResize = 0.f;
  layout->StartRowWithPadding(kFixedVerticalResize, kFieldsColumnId,
                              kFixedVerticalResize,
                              views::LayoutProvider::Get()->GetDistanceMetric(
                                  views::DISTANCE_RELATED_CONTROL_VERTICAL));
  layout->AddView(std::make_unique<Label>(base::ASCIIToUTF16(label)));
}

void DialogExample::StartTextfieldRow(GridLayout* layout,
                                      Textfield** member,
                                      const char* label,
                                      const char* value) {
  StartRowWithLabel(layout, label);
  auto textfield = std::make_unique<Textfield>();
  textfield->set_controller(this);
  textfield->SetText(base::ASCIIToUTF16(value));
  *member = layout->AddView(std::move(textfield));
}

void DialogExample::AddCheckbox(GridLayout* layout, Checkbox** member) {
  auto checkbox = std::make_unique<Checkbox>(base::string16(), this);
  checkbox->SetChecked(true);
  *member = layout->AddView(std::move(checkbox));
}

ui::ModalType DialogExample::GetModalType() const {
  // "Fake" modeless happens when a DialogDelegate specifies window-modal, but
  // doesn't provide a parent window.
  if (mode_->GetSelectedIndex() == kFakeModeless)
    return ui::MODAL_TYPE_WINDOW;

  return static_cast<ui::ModalType>(mode_->GetSelectedIndex());
}

int DialogExample::GetDialogButtons() const {
  int buttons = 0;
  if (has_ok_button_->GetChecked())
    buttons |= ui::DIALOG_BUTTON_OK;
  if (has_cancel_button_->GetChecked())
    buttons |= ui::DIALOG_BUTTON_CANCEL;
  return buttons;
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
  preferred_bounds.set_size(widget->non_client_view()->GetPreferredSize());

  // Q: Do we need NonClientFrameView::GetWindowBoundsForClientBounds() here?
  // A: When DialogCientView properly feeds back sizes, we do not.
  widget->SetBoundsConstrained(preferred_bounds);

  // For user-resizable dialogs, ensure the window manager enforces any new
  // minimum size.
  widget->OnSizeConstraintsChanged();
}

void DialogExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == show_) {
    if (bubble_->GetChecked()) {
      Bubble* bubble = new Bubble(this, sender);
      last_dialog_ = bubble;
      BubbleDialogDelegateView::CreateBubble(bubble);
    } else {
      Dialog* dialog = new Dialog(this);
      last_dialog_ = dialog;
      dialog->InitDelegate();

      // constrained_window::CreateBrowserModalDialogViews() allows dialogs to
      // be created as MODAL_TYPE_WINDOW without specifying a parent.
      gfx::NativeView parent = nullptr;
      if (mode_->GetSelectedIndex() != kFakeModeless)
        parent = example_view()->GetWidget()->GetNativeView();

      DialogDelegate::CreateDialogWidget(
          dialog, example_view()->GetWidget()->GetNativeWindow(), parent);
    }
    last_dialog_->GetWidget()->Show();
    return;
  }

  if (sender == bubble_) {
    if (bubble_->GetChecked() && GetModalType() != ui::MODAL_TYPE_CHILD) {
      mode_->SetSelectedIndex(ui::MODAL_TYPE_CHILD);
      PrintStatus("You nearly always want Child Modal for bubbles.");
    }
    persistent_bubble_->SetEnabled(bubble_->GetChecked());
    OnPerformAction(mode_);  // Validate the modal type.

    if (!bubble_->GetChecked() && GetModalType() == ui::MODAL_TYPE_CHILD) {
      // Do something reasonable when simply unchecking bubble and re-enable.
      mode_->SetSelectedIndex(ui::MODAL_TYPE_WINDOW);
      OnPerformAction(mode_);
    }
    return;
  }

  // Other buttons are all checkboxes. Update the dialog if there is one.
  if (last_dialog_) {
    last_dialog_->DialogModelChanged();
    ResizeDialog();
  }
}

void DialogExample::ContentsChanged(Textfield* sender,
                                    const base::string16& new_contents) {
  if (!last_dialog_)
    return;

  if (sender == extra_button_label_)
    PrintStatus("DialogClientView can never refresh the extra view.");

  if (sender == title_) {
    last_dialog_->GetWidget()->UpdateWindowTitle();
  } else if (sender == body_) {
    last_body_label_->SetText(new_contents);
  } else {
    last_dialog_->DialogModelChanged();
  }

  ResizeDialog();
}

void DialogExample::OnPerformAction(Combobox* combobox) {
  bool enable = bubble_->GetChecked() || GetModalType() != ui::MODAL_TYPE_CHILD;
#if defined(OS_MACOSX)
  enable = enable && GetModalType() != ui::MODAL_TYPE_SYSTEM;
#endif
  show_->SetEnabled(enable);
  if (!enable && GetModalType() == ui::MODAL_TYPE_CHILD)
    PrintStatus("MODAL_TYPE_CHILD can't be used with non-bubbles.");
  if (!enable && GetModalType() == ui::MODAL_TYPE_SYSTEM)
    PrintStatus("MODAL_TYPE_SYSTEM isn't supported on Mac.");
}

}  // namespace examples
}  // namespace views
