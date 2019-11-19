// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_delegate.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/buildflags.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_observer.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DialogDelegate::Params:
DialogDelegate::Params::Params() = default;
DialogDelegate::Params::~Params() = default;

////////////////////////////////////////////////////////////////////////////////
// DialogDelegate:

DialogDelegate::DialogDelegate() {
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
  widget->Init(std::move(params));
  return widget;
}

// static
bool DialogDelegate::CanSupportCustomFrame(gfx::NativeView parent) {
#if defined(OS_LINUX) && BUILDFLAG(ENABLE_DESKTOP_AURA)
  // The new style doesn't support unparented dialogs on Linux desktop.
  return parent != nullptr;
#elif defined(OS_WIN)
  // The new style doesn't support unparented dialogs on Windows Classic themes.
  if (!ui::win::IsAeroGlassEnabled())
    return parent != nullptr;
#endif
  return true;
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

  if (dialog)
    dialog->params_.custom_frame &= CanSupportCustomFrame(parent);

  if (!dialog || dialog->use_custom_frame()) {
    params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
    params.remove_standard_frame = true;
#if !defined(OS_MACOSX)
    // Except on Mac, the bubble frame includes its own shadow; remove any
    // native shadowing. On Mac, the window server provides the shadow.
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
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

int DialogDelegate::GetDialogButtons() const {
  return params_.buttons;
}

int DialogDelegate::GetDefaultDialogButton() const {
  if (GetParams().default_button.has_value())
    return *GetParams().default_button;
  if (GetDialogButtons() & ui::DIALOG_BUTTON_OK)
    return ui::DIALOG_BUTTON_OK;
  if (GetDialogButtons() & ui::DIALOG_BUTTON_CANCEL)
    return ui::DIALOG_BUTTON_CANCEL;
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 DialogDelegate::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (!GetParams().button_labels[button].empty())
    return GetParams().button_labels[button];

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

std::unique_ptr<View> DialogDelegate::CreateFootnoteView() {
  return std::move(footnote_view_);
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

View* DialogDelegate::GetInitiallyFocusedView() {
  // Focus the default button if any.
  const DialogClientView* dcv = GetDialogClientView();
  if (!dcv)
    return nullptr;
  int default_button = GetDefaultDialogButton();
  if (default_button == ui::DIALOG_BUTTON_NONE)
    return nullptr;

  if ((default_button & GetDialogButtons()) == 0) {
    // The default button is a button we don't have.
    NOTREACHED();
    return nullptr;
  }

  if (default_button & ui::DIALOG_BUTTON_OK)
    return dcv->ok_button();
  if (default_button & ui::DIALOG_BUTTON_CANCEL)
    return dcv->cancel_button();
  return nullptr;
}

DialogDelegate* DialogDelegate::AsDialogDelegate() {
  return this;
}

ClientView* DialogDelegate::CreateClientView(Widget* widget) {
  return new DialogClientView(widget, GetContentsView());
}

NonClientFrameView* DialogDelegate::CreateNonClientFrameView(Widget* widget) {
  if (use_custom_frame())
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
  DialogDelegate* delegate = widget->widget_delegate()->AsDialogDelegate();
  if (delegate) {
    if (delegate->GetParams().round_corners) {
      border->SetCornerRadius(
          base::FeatureList::IsEnabled(
              features::kEnableMDRoundedCornersOnDialogs)
              ? provider->GetCornerRadiusMetric(views::EMPHASIS_HIGH)
              : 2);
    }
    frame->SetFootnoteView(delegate->CreateFootnoteView());
  }
  frame->SetBubbleBorder(std::move(border));
  return frame;
}

const DialogClientView* DialogDelegate::GetDialogClientView() const {
  if (!GetWidget())
    return nullptr;
  return GetWidget()->client_view()->AsDialogClientView();
}

DialogClientView* DialogDelegate::GetDialogClientView() {
  if (!GetWidget())
    return nullptr;
  return GetWidget()->client_view()->AsDialogClientView();
}

BubbleFrameView* DialogDelegate::GetBubbleFrameView() const {
  if (!use_custom_frame())
    return nullptr;

  const NonClientView* view =
      GetWidget() ? GetWidget()->non_client_view() : nullptr;
  return view ? static_cast<BubbleFrameView*>(view->frame_view()) : nullptr;
}

views::LabelButton* DialogDelegate::GetOkButton() {
  DCHECK(GetWidget()) << "Don't call this before OnDialogInitialized";
  auto* client = GetDialogClientView();
  return client ? client->ok_button() : nullptr;
}

views::LabelButton* DialogDelegate::GetCancelButton() {
  DCHECK(GetWidget()) << "Don't call this before OnDialogInitialized";
  auto* client = GetDialogClientView();
  return client ? client->cancel_button() : nullptr;
}

views::View* DialogDelegate::GetExtraView() {
  DCHECK(GetWidget()) << "Don't call this before OnDialogInitialized";
  auto* client = GetDialogClientView();
  return client ? client->extra_view() : nullptr;
}

views::View* DialogDelegate::GetFootnoteViewForTesting() {
  if (!GetWidget())
    return footnote_view_.get();

  NonClientFrameView* frame = GetWidget()->non_client_view()->frame_view();

  // CreateDialogFrameView above always uses BubbleFrameView. There are
  // subclasses that override CreateDialogFrameView, but none of them override
  // it to create anything other than a BubbleFrameView.
  // TODO(https://crbug.com/1011446): Make CreateDialogFrameView final, then
  // remove this DCHECK.
  DCHECK_EQ(frame->GetClassName(), BubbleFrameView::kViewClassName);
  return static_cast<BubbleFrameView*>(frame)->GetFootnoteView();
}

void DialogDelegate::AddObserver(DialogObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DialogDelegate::RemoveObserver(DialogObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void DialogDelegate::DialogModelChanged() {
  for (DialogObserver& observer : observer_list_)
    observer.OnDialogChanged();
}

std::unique_ptr<View> DialogDelegate::DisownExtraView() {
  return std::move(extra_view_);
}

void DialogDelegate::CancelDialog() {
  GetDialogClientView()->CancelWindow();
}

void DialogDelegate::AcceptDialog() {
  GetDialogClientView()->AcceptWindow();
}

void DialogDelegate::ResetViewShownTimeStampForTesting() {
  GetDialogClientView()->ResetViewShownTimeStampForTesting();
}

DialogDelegate::~DialogDelegate() {
  UMA_HISTOGRAM_LONG_TIMES("Dialog.DialogDelegate.Duration",
                           base::TimeTicks::Now() - creation_time_);
}

ax::mojom::Role DialogDelegate::GetAccessibleWindowRole() {
  return ax::mojom::Role::kDialog;
}

void DialogDelegate::OnWidgetInitialized() {
  OnDialogInitialized();
}

////////////////////////////////////////////////////////////////////////////////
// DialogDelegateView:

DialogDelegateView::DialogDelegateView() {
  // A WidgetDelegate should be deleted on DeleteDelegate.
  set_owned_by_client();
  UMA_HISTOGRAM_BOOLEAN("Dialog.DialogDelegateView.Create", true);
}

DialogDelegateView::~DialogDelegateView() = default;

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
  if (details.is_add && details.child == this && GetWidget() &&
      (GetAccessibleWindowRole() == ax::mojom::Role::kAlert ||
       GetAccessibleWindowRole() == ax::mojom::Role::kAlertDialog)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

}  // namespace views
