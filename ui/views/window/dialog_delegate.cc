// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_delegate.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_observer.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DialogDelegate:

DialogDelegate::DialogDelegate()
    : supports_custom_frame_(true),
      // TODO(crbug.com/733040): Most subclasses assume they must set their own
      // margins explicitly, so we set them to 0 here for now to avoid doubled
      // margins.
      margins_(0) {
  UMA_HISTOGRAM_BOOLEAN("Dialog.DialogDelegate.Create", true);
  creation_time_ = base::TimeTicks::Now();
}

// static
Widget* DialogDelegate::CreateDialogWidget(WidgetDelegate* delegate,
                                           gfx::NativeWindow context,
                                           gfx::NativeView parent) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params =
      GetDialogWidgetInitParams(delegate, context, parent, gfx::Rect());
  widget->Init(params);
  return widget;
}

// static
Widget::InitParams DialogDelegate::GetDialogWidgetInitParams(
    WidgetDelegate* delegate,
    gfx::NativeWindow context,
    gfx::NativeView parent,
    const gfx::Rect& bounds) {
  views::Widget::InitParams params;
  params.delegate = delegate;
  params.bounds = bounds;
  DialogDelegate* dialog = delegate->AsDialogDelegate();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // The new style doesn't support unparented dialogs on Linux desktop.
  if (dialog)
    dialog->supports_custom_frame_ &= parent != NULL;
#elif defined(OS_WIN)
  // The new style doesn't support unparented dialogs on Windows Classic themes.
  if (dialog && !ui::win::IsAeroGlassEnabled())
    dialog->supports_custom_frame_ &= parent != NULL;
#endif

  if (!dialog || dialog->ShouldUseCustomFrame()) {
    params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
    params.remove_standard_frame = true;
#if !defined(OS_MACOSX)
    // Except on Mac, the bubble frame includes its own shadow; remove any
    // native shadowing. On Mac, the window server provides the shadow.
    params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
#endif
  }
  params.context = context;
  params.parent = parent;
#if !defined(OS_MACOSX)
  // Web-modal (ui::MODAL_TYPE_CHILD) dialogs with parents are marked as child
  // widgets to prevent top-level window behavior (independent movement, etc).
  // On Mac, however, the parent may be a native window (not a views::Widget),
  // and so the dialog must be considered top-level to gain focus and input
  // method behaviors.
  params.child = parent && (delegate->GetModalType() == ui::MODAL_TYPE_CHILD);
#endif
  return params;
}

View* DialogDelegate::CreateExtraView() {
  return NULL;
}

bool DialogDelegate::GetExtraViewPadding(int* padding) {
  return false;
}

View* DialogDelegate::CreateFootnoteView() {
  return NULL;
}

bool DialogDelegate::Cancel() {
  return true;
}

bool DialogDelegate::Accept() {
  return true;
}

bool DialogDelegate::Close() {
  int buttons = GetDialogButtons();
  if ((buttons & ui::DIALOG_BUTTON_CANCEL) ||
      (buttons == ui::DIALOG_BUTTON_NONE)) {
    return Cancel();
  }
  return Accept();
}

void DialogDelegate::UpdateButton(LabelButton* button, ui::DialogButton type) {
  button->SetText(GetDialogButtonLabel(type));
  button->SetEnabled(IsDialogButtonEnabled(type));
  bool is_default = type == GetDefaultDialogButton();
  if (!PlatformStyle::kDialogDefaultButtonCanBeCancel &&
      type == ui::DIALOG_BUTTON_CANCEL) {
    is_default = false;
  }
  button->SetIsDefault(is_default);
}

bool DialogDelegate::ShouldSnapFrameWidth() const {
  return GetDialogButtons() != ui::DIALOG_BUTTON_NONE;
}

int DialogDelegate::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

int DialogDelegate::GetDefaultDialogButton() const {
  if (GetDialogButtons() & ui::DIALOG_BUTTON_OK)
    return ui::DIALOG_BUTTON_OK;
  if (GetDialogButtons() & ui::DIALOG_BUTTON_CANCEL)
    return ui::DIALOG_BUTTON_CANCEL;
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 DialogDelegate::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_APP_OK);
  if (button == ui::DIALOG_BUTTON_CANCEL) {
    if (GetDialogButtons() & ui::DIALOG_BUTTON_OK)
      return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
    return l10n_util::GetStringUTF16(IDS_APP_CLOSE);
  }
  NOTREACHED();
  return base::string16();
}

bool DialogDelegate::IsDialogButtonEnabled(ui::DialogButton button) const {
  return true;
}

View* DialogDelegate::GetInitiallyFocusedView() {
  // Focus the default button if any.
  const DialogClientView* dcv = GetDialogClientView();
  int default_button = GetDefaultDialogButton();
  if (default_button == ui::DIALOG_BUTTON_NONE)
    return NULL;

  if ((default_button & GetDialogButtons()) == 0) {
    // The default button is a button we don't have.
    NOTREACHED();
    return NULL;
  }

  if (default_button & ui::DIALOG_BUTTON_OK)
    return dcv->ok_button();
  if (default_button & ui::DIALOG_BUTTON_CANCEL)
    return dcv->cancel_button();
  return NULL;
}

DialogDelegate* DialogDelegate::AsDialogDelegate() {
  return this;
}

ClientView* DialogDelegate::CreateClientView(Widget* widget) {
  return new DialogClientView(widget, GetContentsView());
}

NonClientFrameView* DialogDelegate::CreateNonClientFrameView(Widget* widget) {
  if (ShouldUseCustomFrame())
    return CreateDialogFrameView(widget);
  return WidgetDelegate::CreateNonClientFrameView(widget);
}

// static
NonClientFrameView* DialogDelegate::CreateDialogFrameView(Widget* widget) {
  LayoutProvider* provider = LayoutProvider::Get();
  BubbleFrameView* frame = new BubbleFrameView(
      provider->GetInsetsMetric(INSETS_DIALOG_TITLE), gfx::Insets());
  const BubbleBorder::Shadow kShadow = BubbleBorder::DIALOG_SHADOW;
  std::unique_ptr<BubbleBorder> border = std::make_unique<BubbleBorder>(
      BubbleBorder::FLOAT, kShadow, gfx::kPlaceholderColor);
  border->set_use_theme_background_color(true);
  frame->SetBubbleBorder(std::move(border));
  DialogDelegate* delegate = widget->widget_delegate()->AsDialogDelegate();
  if (delegate)
    frame->SetFootnoteView(delegate->CreateFootnoteView());
  return frame;
}

bool DialogDelegate::ShouldUseCustomFrame() const {
  return supports_custom_frame_;
}

const DialogClientView* DialogDelegate::GetDialogClientView() const {
  return GetWidget()->client_view()->AsDialogClientView();
}

DialogClientView* DialogDelegate::GetDialogClientView() {
  return GetWidget()->client_view()->AsDialogClientView();
}

void DialogDelegate::AddObserver(DialogObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DialogDelegate::RemoveObserver(DialogObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void DialogDelegate::DialogModelChanged() {
  for (DialogObserver& observer : observer_list_)
    observer.OnDialogModelChanged();
}

DialogDelegate::~DialogDelegate() {
  UMA_HISTOGRAM_LONG_TIMES("Dialog.DialogDelegate.Duration",
                           base::TimeTicks::Now() - creation_time_);
}

ax::mojom::Role DialogDelegate::GetAccessibleWindowRole() const {
  return ax::mojom::Role::kDialog;
}

////////////////////////////////////////////////////////////////////////////////
// DialogDelegateView:

DialogDelegateView::DialogDelegateView() {
  // A WidgetDelegate should be deleted on DeleteDelegate.
  set_owned_by_client();
  UMA_HISTOGRAM_BOOLEAN("Dialog.DialogDelegateView.Create", true);
}

DialogDelegateView::~DialogDelegateView() {}

void DialogDelegateView::DeleteDelegate() {
  delete this;
}

Widget* DialogDelegateView::GetWidget() {
  return View::GetWidget();
}

const Widget* DialogDelegateView::GetWidget() const {
  return View::GetWidget();
}

View* DialogDelegateView::GetContentsView() {
  return this;
}

void DialogDelegateView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this && GetWidget())
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

}  // namespace views
