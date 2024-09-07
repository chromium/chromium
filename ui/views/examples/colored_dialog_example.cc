// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/colored_dialog_example.h"

#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "base/memory/raw_ref.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace views::examples {

class ThemeTrackingCheckbox : public views::Checkbox {
  METADATA_HEADER(ThemeTrackingCheckbox, views::Checkbox)

 public:
  explicit ThemeTrackingCheckbox(const std::u16string& label)
      : Checkbox(label,
                 base::BindRepeating(&ThemeTrackingCheckbox::ButtonPressed,
                                     base::Unretained(this))) {}
  ThemeTrackingCheckbox(const ThemeTrackingCheckbox&) = delete;
  ThemeTrackingCheckbox& operator=(const ThemeTrackingCheckbox&) = delete;
  ~ThemeTrackingCheckbox() override = default;

  // views::Checkbox
  void OnThemeChanged() override {
    views::Checkbox::OnThemeChanged();
    SetChecked(GetNativeTheme()->ShouldUseDarkColors());
  }

  void ButtonPressed() {
    GetNativeTheme()->set_use_dark_colors(GetChecked());
    GetWidget()->ThemeChanged();
  }
};

BEGIN_METADATA(ThemeTrackingCheckbox)
END_METADATA

class TextVectorImageButton : public views::MdTextButton {
  METADATA_HEADER(TextVectorImageButton, views::MdTextButton)

 public:
  TextVectorImageButton(PressedCallback callback,
                        const std::u16string& text,
                        const gfx::VectorIcon& icon)
      : MdTextButton(std::move(callback), text), icon_(icon) {}
  TextVectorImageButton(const TextVectorImageButton&) = delete;
  TextVectorImageButton& operator=(const TextVectorImageButton&) = delete;
  ~TextVectorImageButton() override = default;

  void OnThemeChanged() override {
    views::MdTextButton::OnThemeChanged();

    // Use the text color for the associated vector image.
    SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*icon_, label()->GetEnabledColor()));
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
};

BEGIN_METADATA(TextVectorImageButton)
END_METADATA

ColoredDialog::ColoredDialog(AcceptCallback accept_callback) {
  SetAcceptCallback(base::BindOnce(
      [](ColoredDialog* dialog, AcceptCallback callback) {
        std::move(callback).Run(dialog->textfield_->GetText());
      },
      base::Unretained(this), std::move(accept_callback)));

  SetModalType(ui::mojom::ModalType::kWindow);
  SetTitle(l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_TITLE));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(views::LayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  textfield_ = AddChildView(std::make_unique<views::Textfield>());
  textfield_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_TEXTFIELD_PLACEHOLDER));
  textfield_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_TEXTFIELD_AX_LABEL));
  textfield_->set_controller(this);

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_SUBMIT_BUTTON));
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
}

ColoredDialog::~ColoredDialog() {
  if (textfield_) {
    textfield_->set_controller(nullptr);
  }
}

bool ColoredDialog::ShouldShowCloseButton() const {
  return false;
}

void ColoredDialog::ContentsChanged(Textfield* sender,
                                    const std::u16string& new_contents) {
  SetButtonEnabled(ui::mojom::DialogButton::kOk,
                   !textfield_->GetText().empty());
  DialogModelChanged();
}

BEGIN_METADATA(ColoredDialog)
END_METADATA

ColoredDialogChooser::ColoredDialogChooser() {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_spacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddChildView(std::make_unique<ThemeTrackingCheckbox>(
      l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_CHOOSER_CHECKBOX)));

  AddChildView(std::make_unique<TextVectorImageButton>(
      base::BindRepeating(&ColoredDialogChooser::ButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_COLORED_DIALOG_CHOOSER_BUTTON),
      views::kInfoIcon));

  confirmation_label_ = AddChildView(
      std::make_unique<views::Label>(std::u16string(), style::CONTEXT_LABEL));
  confirmation_label_->SetVisible(false);
}

ColoredDialogChooser::~ColoredDialogChooser() = default;

void ColoredDialogChooser::ButtonPressed() {
  // Create the colored dialog.
  views::Widget* widget = DialogDelegate::CreateDialogWidget(
      new ColoredDialog(base::BindOnce(&ColoredDialogChooser::OnFeedbackSubmit,
                                       base::Unretained(this))),
      nullptr, GetWidget()->GetNativeView());
  widget->Show();
}

void ColoredDialogChooser::OnFeedbackSubmit(std::u16string text) {
  constexpr base::TimeDelta kConfirmationDuration = base::Seconds(3);

  confirmation_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_COLORED_DIALOG_CHOOSER_CONFIRM_LABEL, text));
  confirmation_label_->SetVisible(true);

  confirmation_timer_.Start(
      FROM_HERE, kConfirmationDuration,
      base::BindOnce([](views::View* view) { view->SetVisible(false); },
                     confirmation_label_));
}

BEGIN_METADATA(ColoredDialogChooser)
END_METADATA

ColoredDialogExample::ColoredDialogExample() : ExampleBase("Colored Dialog") {}

ColoredDialogExample::~ColoredDialogExample() = default;

void ColoredDialogExample::CreateExampleView(views::View* container) {
  container->SetLayoutManager(std::make_unique<views::FillLayout>());
  container->AddChildView(std::make_unique<ColoredDialogChooser>());
}

}  // namespace views::examples
