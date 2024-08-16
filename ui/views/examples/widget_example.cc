// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/widget_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

WidgetExample::WidgetExample()
    : ExampleBase(GetStringUTF8(IDS_WIDGET_SELECT_LABEL).c_str()) {}

WidgetExample::~WidgetExample() = default;

void WidgetExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(), 10));
  LabelButton* popup_button =
      BuildButton(container, GetStringUTF16(IDS_WIDGET_POPUP_BUTTON_LABEL));
  popup_button->SetCallback(
      base::BindRepeating(&WidgetExample::ShowWidget, base::Unretained(this),
                          popup_button, Widget::InitParams::TYPE_POPUP));
  LabelButton* dialog_button =
      BuildButton(container, GetStringUTF16(IDS_WIDGET_DIALOG_BUTTON_LABEL));
  dialog_button->SetCallback(
      base::BindRepeating(&WidgetExample::CreateDialogWidget,
                          base::Unretained(this), dialog_button, false));
  LabelButton* modal_button =
      BuildButton(container, GetStringUTF16(IDS_WIDGET_MODAL_BUTTON_LABEL));
  modal_button->SetCallback(
      base::BindRepeating(&WidgetExample::CreateDialogWidget,
                          base::Unretained(this), modal_button, true));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Windows does not support TYPE_CONTROL top-level widgets.
  LabelButton* control_button = BuildButton(
      container, GetStringUTF16(IDS_WIDGET_CHILD_WIDGET_BUTTON_LABEL));
  control_button->SetCallback(
      base::BindRepeating(&WidgetExample::ShowWidget, base::Unretained(this),
                          control_button, Widget::InitParams::TYPE_CONTROL));
#endif
}

LabelButton* WidgetExample::BuildButton(View* container,
                                        const std::u16string& label) {
  LabelButton* button = container->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(), label));
  button->SetRequestFocusOnPress(true);
  return button;
}

void WidgetExample::CreateDialogWidget(View* sender, bool modal) {
  auto dialog = std::make_unique<DialogDelegateView>();
  dialog->SetTitle(IDS_WIDGET_WINDOW_TITLE);
  dialog->SetBackground(CreateThemedSolidBackground(
      ExamplesColorIds::kColorWidgetExampleDialogBorder));
  dialog->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));
  dialog->SetExtraView(std::make_unique<MdTextButton>(
      Button::PressedCallback(), GetStringUTF16(IDS_WIDGET_EXTRA_BUTTON)));
  dialog->SetFootnoteView(
      std::make_unique<Label>(GetStringUTF16(IDS_WIDGET_FOOTNOTE_LABEL)));
  dialog->AddChildView(std::make_unique<Label>(
      GetStringUTF16(IDS_WIDGET_DIALOG_CONTENTS_LABEL)));
  if (modal)
    dialog->SetModalType(ui::mojom::ModalType::kWindow);
  DialogDelegate::CreateDialogWidget(dialog.release(), nullptr,
                                     sender->GetWidget()->GetNativeView())
      ->Show();
}

void WidgetExample::ShowWidget(View* sender, Widget::InitParams::Type type) {
  // Setup shared Widget hierarchy and bounds parameters.
  Widget::InitParams params(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                            type);
  params.parent = sender->GetWidget()->GetNativeView();
  params.bounds =
      gfx::Rect(sender->GetBoundsInScreen().CenterPoint(), gfx::Size(300, 200));

  // A widget handles its own lifetime.
  Widget* widget = new Widget();
  widget->Init(std::move(params));

  // If the Widget has no contents by default, add a view with a 'Close' button.
  if (!widget->GetContentsView()) {
    View* contents = widget->SetContentsView(std::make_unique<View>());
    contents->SetLayoutManager(
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
    contents->SetBackground(CreateThemedSolidBackground(
        ExamplesColorIds::kColorWidgetExampleContentBorder));
    BuildButton(contents, GetStringUTF16(IDS_WIDGET_CLOSE_BUTTON_LABEL))
        ->SetCallback(
            base::BindRepeating(&Widget::Close, base::Unretained(widget)));
  }

  widget->Show();
}

}  // namespace views::examples
