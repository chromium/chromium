// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/widget_example.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

namespace {

// TODO(ellyjones): This should return unique_ptr<> instead, but
// CreateDialogWidget doesn't yet have an override that takes that, and can't
// pending sorting out the mess around WidgetDelegate ownership.
DialogDelegate* MakeExampleDialog(bool modal) {
  auto dialog = std::make_unique<DialogDelegateView>();
  dialog->SetTitle(IDS_WIDGET_WINDOW_TITLE);

  dialog->SetBackground(CreateSolidBackground(SK_ColorGRAY));
  dialog->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));
  dialog->SetExtraView(std::make_unique<MdTextButton>(
      nullptr, GetStringUTF16(IDS_WIDGET_EXTRA_BUTTON)));
  dialog->SetFootnoteView(
      std::make_unique<Label>(GetStringUTF16(IDS_WIDGET_FOOTNOTE_LABEL)));
  dialog->AddChildView(std::make_unique<Label>(
      GetStringUTF16(IDS_WIDGET_DIALOG_CONTENTS_LABEL)));

  if (modal)
    dialog->SetModalType(ui::MODAL_TYPE_WINDOW);

  return dialog.release();
}

}  // namespace

WidgetExample::WidgetExample()
    : ExampleBase(GetStringUTF8(IDS_WIDGET_SELECT_LABEL).c_str()) {}

WidgetExample::~WidgetExample() = default;

void WidgetExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(), 10));
  BuildButton(container, GetStringUTF16(IDS_WIDGET_POPUP_BUTTON_LABEL), POPUP);
  BuildButton(container, GetStringUTF16(IDS_WIDGET_DIALOG_BUTTON_LABEL),
              DIALOG);
  BuildButton(container, GetStringUTF16(IDS_WIDGET_MODAL_BUTTON_LABEL),
              MODAL_DIALOG);
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Windows does not support TYPE_CONTROL top-level widgets.
  BuildButton(container, GetStringUTF16(IDS_WIDGET_CHILD_WIDGET_BUTTON_LABEL),
              CHILD);
#endif
}

void WidgetExample::BuildButton(View* container,
                                const base::string16& label,
                                int tag) {
  LabelButton* button =
      container->AddChildView(std::make_unique<LabelButton>(this, label));
  button->SetFocusForPlatform();
  button->SetRequestFocusOnPress(true);
  button->set_tag(tag);
}

void WidgetExample::ShowWidget(View* sender, Widget::InitParams params) {
  // Setup shared Widget hierarchy and bounds parameters.
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
    contents->SetBackground(CreateSolidBackground(SK_ColorGRAY));
    BuildButton(contents, GetStringUTF16(IDS_WIDGET_CLOSE_BUTTON_LABEL),
                CLOSE_WIDGET);
  }

  widget->Show();
}

void WidgetExample::ButtonPressed(Button* sender, const ui::Event& event) {
  switch (sender->tag()) {
    case POPUP:
      ShowWidget(sender, Widget::InitParams(Widget::InitParams::TYPE_POPUP));
      break;
    case DIALOG: {
      // The DialogDelegate will be destroyed by the widget when the created
      // widget is destroyed.
      DialogDelegate::CreateDialogWidget(MakeExampleDialog(false), nullptr,
                                         sender->GetWidget()->GetNativeView())
          ->Show();
      break;
    }
    case MODAL_DIALOG: {
      // ModalDialogExample will be destroyed by the widget when the created
      // widget is destroyed.
      DialogDelegate::CreateDialogWidget(MakeExampleDialog(true), nullptr,
                                         sender->GetWidget()->GetNativeView())
          ->Show();
      break;
    }
    case CHILD:
      ShowWidget(sender, Widget::InitParams(Widget::InitParams::TYPE_CONTROL));
      break;
    case CLOSE_WIDGET:
      sender->GetWidget()->Close();
      break;
  }
}

}  // namespace examples
}  // namespace views
