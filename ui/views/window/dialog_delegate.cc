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
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/buildflags.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
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
  WidgetDelegate::RegisterWindowWillCloseCallback(
      base::BindOnce(&DialogDelegate::WindowWillClose, base::Unretained(this)));
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
Widget* DialogDelegate::CreateDialogWidget(
    std::unique_ptr<WidgetDelegate> delegate,
    gfx::NativeWindow context,
    gfx::NativeView parent) {
  DCHECK(delegate->owned_by_widget());
  return CreateDialogWidget(delegate.release(), context, parent);
}

// static
bool DialogDelegate::CanSupportCustomFrame(gfx::NativeView parent) {
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && \
    BUILDFLAG(ENABLE_DESKTOP_AURA)
  // The new style doesn't support unparented dialogs on Linux desktop.
  return parent != nullptr;
#else
#if defined(OS_WIN)
  // The new style doesn't support unparented dialogs on Windows Classic themes.
  if (!ui::win::IsAeroGlassEnabled())
    return parent != nullptr;
#endif
  return true;
#endif
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
    params.opacity = Widget::InitParams::WindowOpacity::kTranslucent;
    params.remove_standard_frame = true;
#if !defined(OS_APPLE)
    // Except on Mac, the bubble frame includes its own shadow; remove any
    // native shadowing. On Mac, the window server provides the shadow.
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
#endif
  }
  params.context = context;
  params.parent = parent;
#if !defined(OS_APPLE)
  // Web-modal (ui::MODAL_TYPE_CHILD) dialogs with parents are marked as child
  // widgets to prevent top-level window behavior (independent movement, etc).
  // On Mac, however, the parent may be a native window (not a views::Widget),
  // and so the dialog must be considered top-level to gain focus and input
  // method behaviors.
  params.child = parent && (delegate->GetModalType() == ui::MODAL_TYPE_CHILD);
#endif
  return params;
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
  return params_.enabled_buttons & button;
}

bool DialogDelegate::Cancel() {
  DCHECK(!already_started_close_);
  if (cancel_callback_)
    RunCloseCallback(std::move(cancel_callback_));
  return true;
}

bool DialogDelegate::Accept() {
  DCHECK(!already_started_close_);
  if (accept_callback_)
    RunCloseCallback(std::move(accept_callback_));
  return true;
}

void DialogDelegate::RunCloseCallback(base::OnceClosure callback) {
  DCHECK(!already_started_close_);
  already_started_close_ = true;
  std::move(callback).Run();
}

View* DialogDelegate::GetInitiallyFocusedView() {
  if (WidgetDelegate::HasConfiguredInitiallyFocusedView())
    return WidgetDelegate::GetInitiallyFocusedView();

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
  return new DialogClientView(widget, TransferOwnershipOfContentsView());
}

std::unique_ptr<NonClientFrameView> DialogDelegate::CreateNonClientFrameView(
    Widget* widget) {
  return use_custom_frame() ? CreateDialogFrameView(widget)
                            : WidgetDelegate::CreateNonClientFrameView(widget);
}

void DialogDelegate::WindowWillClose() {
  if (already_started_close_)
    return;

  bool new_callback_present =
      close_callback_ || cancel_callback_ || accept_callback_;

  if (close_callback_)
    RunCloseCallback(std::move(close_callback_));

  if (new_callback_present)
    return;

  // This is set here instead of before the invocations of Accept()/Cancel() so
  // that those methods can DCHECK that !already_started_close_. Otherwise,
  // client code could (eg) call Accept() from inside the cancel callback, which
  // could lead to multiple callbacks being delivered from this class.
  already_started_close_ = true;
}

// static
std::unique_ptr<NonClientFrameView> DialogDelegate::CreateDialogFrameView(
    Widget* widget) {
  LayoutProvider* provider = LayoutProvider::Get();
  auto frame = std::make_unique<BubbleFrameView>(
      provider->GetInsetsMetric(INSETS_DIALOG_TITLE), gfx::Insets());

  const BubbleBorder::Shadow kShadow = BubbleBorder::DIALOG_SHADOW;
  std::unique_ptr<BubbleBorder> border = std::make_unique<BubbleBorder>(
      BubbleBorder::FLOAT, kShadow, gfx::kPlaceholderColor);
  border->set_use_theme_background_color(true);
  DialogDelegate* delegate = widget->widget_delegate()->AsDialogDelegate();
  if (delegate) {
    if (delegate->GetParams().round_corners)
      border->SetCornerRadius(delegate->GetCornerRadius());
    frame->SetFootnoteView(delegate->DisownFootnoteView());
  }
  frame->SetBubbleBorder(std::move(border));
  return frame;
}

const DialogClientView* DialogDelegate::GetDialogClientView() const {
  if (!GetWidget())
    return nullptr;
  const views::View* client_view = GetWidget()->client_view();
  return client_view->GetClassName() == DialogClientView::kViewClassName
             ? static_cast<const DialogClientView*>(client_view)
             : nullptr;
}

DialogClientView* DialogDelegate::GetDialogClientView() {
  if (!GetWidget())
    return nullptr;
  views::View* client_view = GetWidget()->client_view();
  return client_view->GetClassName() == DialogClientView::kViewClassName
             ? static_cast<DialogClientView*>(client_view)
             : nullptr;
}

BubbleFrameView* DialogDelegate::GetBubbleFrameView() const {
  if (!use_custom_frame())
    return nullptr;

  const NonClientView* view =
      GetWidget() ? GetWidget()->non_client_view() : nullptr;
  return view ? static_cast<BubbleFrameView*>(view->frame_view()) : nullptr;
}

views::LabelButton* DialogDelegate::GetOkButton() const {
  DCHECK(GetWidget()) << "Don't call this before OnDialogInitialized";
  auto* client = GetDialogClientView();
  return client ? client->ok_button() : nullptr;
}

views::LabelButton* DialogDelegate::GetCancelButton() const {
  DCHECK(GetWidget()) << "Don't call this before OnDialogInitialized";
  auto* client = GetDialogClientView();
  return client ? client->cancel_button() : nullptr;
}

views::View* DialogDelegate::GetExtraView() const {
  DCHECK(GetWidget()) << "Don't call this before OnDialogInitialized";
  auto* client = GetDialogClientView();
  return client ? client->extra_view() : nullptr;
}

views::View* DialogDelegate::GetFootnoteViewForTesting() const {
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

void DialogDelegate::SetDefaultButton(int button) {
  if (params_.default_button == button)
    return;
  params_.default_button = button;
  DialogModelChanged();
}

void DialogDelegate::SetButtons(int buttons) {
  if (params_.buttons == buttons)
    return;
  params_.buttons = buttons;
  DialogModelChanged();
}

void DialogDelegate::SetButtonEnabled(ui::DialogButton button, bool enabled) {
  if (!!(params_.enabled_buttons & button) == enabled)
    return;
  if (enabled)
    params_.enabled_buttons |= button;
  else
    params_.enabled_buttons &= ~button;
  DialogModelChanged();
}

void DialogDelegate::SetButtonLabel(ui::DialogButton button,
    base::string16 label) {
  if (params_.button_labels[button] == label)
    return;
  params_.button_labels[button] = label;
  DialogModelChanged();
}

void DialogDelegate::SetAcceptCallback(base::OnceClosure callback) {
  accept_callback_ = std::move(callback);
}

void DialogDelegate::SetCancelCallback(base::OnceClosure callback) {
  cancel_callback_ = std::move(callback);
}

void DialogDelegate::SetCloseCallback(base::OnceClosure callback) {
  close_callback_ = std::move(callback);
}

std::unique_ptr<View> DialogDelegate::DisownExtraView() {
  return std::move(extra_view_);
}

bool DialogDelegate::Close() {
  WindowWillClose();
  return true;
}

void DialogDelegate::ResetViewShownTimeStampForTesting() {
  GetDialogClientView()->ResetViewShownTimeStampForTesting();
}

void DialogDelegate::SetButtonRowInsets(const gfx::Insets& insets) {
  GetDialogClientView()->SetButtonRowInsets(insets);
}

void DialogDelegate::AcceptDialog() {
  DCHECK(IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  if (already_started_close_ || !Accept())
    return;

  already_started_close_ = true;
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
}

void DialogDelegate::CancelDialog() {
  // Note: don't DCHECK(IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL)) here;
  // CancelDialog() is *always* reachable via Esc closing the dialog, even if
  // the cancel button is disabled or there is no cancel button at all.
  if (already_started_close_ || !Cancel())
    return;

  already_started_close_ = true;
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
}

DialogDelegate::~DialogDelegate() {
  UMA_HISTOGRAM_LONG_TIMES("Dialog.DialogDelegate.Duration",
                           base::TimeTicks::Now() - creation_time_);
}

ax::mojom::Role DialogDelegate::GetAccessibleWindowRole() {
  return ax::mojom::Role::kDialog;
}

int DialogDelegate::GetCornerRadius() const {
#if defined(OS_MAC)
  // TODO(crbug.com/1116680): On Mac MODAL_TYPE_WINDOW is implemented using
  // sheets which causes visual artifacts when corner radius is increased for
  // modal types. Remove this after this issue has been addressed.
  if (GetModalType() == ui::MODAL_TYPE_WINDOW)
    return 2;
#endif
  return LayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_MEDIUM);
}

std::unique_ptr<View> DialogDelegate::DisownFootnoteView() {
  return std::move(footnote_view_);
}

void DialogDelegate::OnWidgetInitialized() {
  OnDialogInitialized();
}

////////////////////////////////////////////////////////////////////////////////
// DialogDelegateView:

DialogDelegateView::DialogDelegateView() {
  set_owned_by_client();
  SetOwnedByWidget(true);
  UMA_HISTOGRAM_BOOLEAN("Dialog.DialogDelegateView.Create", true);
}

DialogDelegateView::~DialogDelegateView() = default;

Widget* DialogDelegateView::GetWidget() {
  return View::GetWidget();
}

const Widget* DialogDelegateView::GetWidget() const {
  return View::GetWidget();
}

View* DialogDelegateView::GetContentsView() {
  return this;
}

BEGIN_METADATA(DialogDelegateView, View)
END_METADATA

}  // namespace views
