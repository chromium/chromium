// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_delegate.h"

#include <utility>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/buildflags.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_observer.h"

namespace views {

namespace {

bool HasCallback(
    const absl::variant<base::OnceClosure, base::RepeatingCallback<bool()>>&
        callback) {
  return absl::visit(
      [](const auto& variant) { return static_cast<bool>(variant); }, callback);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DialogDelegate::Params:
DialogDelegate::Params::Params() = default;
DialogDelegate::Params::~Params() = default;

////////////////////////////////////////////////////////////////////////////////
// DialogDelegate:

DialogDelegate::DialogDelegate() {
  WidgetDelegate::RegisterWindowWillCloseCallback(
      base::BindOnce(&DialogDelegate::WindowWillClose, base::Unretained(this)));
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
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    BUILDFLAG(ENABLE_DESKTOP_AURA)
  // The new style doesn't support unparented dialogs on Linux desktop.
  return parent != nullptr;
#else
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
#if !BUILDFLAG(IS_MAC)
    // Except on Mac, the bubble frame includes its own shadow; remove any
    // native shadowing. On Mac, the window server provides the shadow.
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
#endif
  }
  params.context = context;
  params.parent = parent;
#if !BUILDFLAG(IS_APPLE)
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

std::u16string DialogDelegate::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (!GetParams().button_labels[button].empty())
    return GetParams().button_labels[button];

  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_APP_OK);
  CHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  return GetDialogButtons() & ui::DIALOG_BUTTON_OK
             ? l10n_util::GetStringUTF16(IDS_APP_CANCEL)
             : l10n_util::GetStringUTF16(IDS_APP_CLOSE);
}

ui::ButtonStyle DialogDelegate::GetDialogButtonStyle(
    ui::DialogButton button) const {
  absl::optional<ui::ButtonStyle> style = GetParams().button_styles[button];
  if (style.has_value()) {
    return *style;
  }

  return GetIsDefault(button) ? ui::ButtonStyle::kProminent
                              : ui::ButtonStyle::kDefault;
}

bool DialogDelegate::GetIsDefault(ui::DialogButton button) const {
  return GetDefaultDialogButton() == button &&
         (button != ui::DIALOG_BUTTON_CANCEL ||
          PlatformStyle::kDialogDefaultButtonCanBeCancel);
}

bool DialogDelegate::IsDialogButtonEnabled(ui::DialogButton button) const {
  return params_.enabled_buttons & button;
}

bool DialogDelegate::ShouldIgnoreButtonPressedEventHandling(
    View* button,
    const ui::Event& event) const {
  return false;
}

bool DialogDelegate::Cancel() {
  DCHECK(!already_started_close_);
  if (HasCallback(cancel_callback_)) {
    return RunCloseCallback(cancel_callback_);
  }
  return true;
}

bool DialogDelegate::Accept() {
  DCHECK(!already_started_close_);
  if (HasCallback(accept_callback_)) {
    return RunCloseCallback(accept_callback_);
  }
  return true;
}

bool DialogDelegate::RunCloseCallback(
    absl::variant<base::OnceClosure, base::RepeatingCallback<bool()>>&
        callback) {
  DCHECK(!already_started_close_);
  if (absl::holds_alternative<base::OnceClosure>(callback)) {
    already_started_close_ = true;
    absl::get<base::OnceClosure>(std::move(callback)).Run();
  } else {
    already_started_close_ =
        absl::get<base::RepeatingCallback<bool()>>(callback).Run();
  }

  return already_started_close_;
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

  // The default button should be a button we have.
  CHECK(default_button & GetDialogButtons());

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

  const bool new_callback_present = close_callback_ ||
                                    HasCallback(cancel_callback_) ||
                                    HasCallback(accept_callback_);

  if (close_callback_) {
    // `RunCloseCallback` takes a non-const reference to this variant to support
    // the accept and cancel callbacks. It doesn't make sense to be storing a
    // variant for close callbacks, so we construct the variant here instead.
    absl::variant<base::OnceClosure, base::RepeatingCallback<bool()>>
        close_callback_wrapped(std::move(close_callback_));
    RunCloseCallback(close_callback_wrapped);
  }

  if (new_callback_present)
    return;

  // This is set here instead of before the invocations of Accept()/Cancel() so
  // that those methods can DCHECK that !already_started_close_. Otherwise,
  // client code could (eg) call Accept() from inside the cancel callback, which
  // could lead to multiple callbacks being delivered from this class.
  already_started_close_ = true;
}

bool DialogDelegate::EscShouldCancelDialog() const {
  // Use cancel as the Esc action if there's no defined "close" action. If the
  // delegate has either specified a closing action or a close-x they can expect
  // it to be called on Esc.
  return !close_callback_ && !ShouldShowCloseButton();
}

// static
std::unique_ptr<NonClientFrameView> DialogDelegate::CreateDialogFrameView(
    Widget* widget) {
  LayoutProvider* provider = LayoutProvider::Get();
  auto frame = std::make_unique<BubbleFrameView>(
      provider->GetInsetsMetric(INSETS_DIALOG_TITLE), gfx::Insets());

  const BubbleBorder::Shadow kShadow = BubbleBorder::DIALOG_SHADOW;
  std::unique_ptr<BubbleBorder> border =
      std::make_unique<BubbleBorder>(BubbleBorder::FLOAT, kShadow);
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
  return const_cast<DialogClientView*>(
      const_cast<const DialogDelegate*>(this)->GetDialogClientView());
}

BubbleFrameView* DialogDelegate::GetBubbleFrameView() const {
  if (!use_custom_frame())
    return nullptr;

  const NonClientView* view =
      GetWidget() ? GetWidget()->non_client_view() : nullptr;
  return view ? static_cast<BubbleFrameView*>(view->frame_view()) : nullptr;
}

views::MdTextButton* DialogDelegate::GetOkButton() const {
  DCHECK(GetWidget()) << "Don't call this before OnWidgetInitialized";
  auto* client = GetDialogClientView();
  return client ? client->ok_button() : nullptr;
}

views::MdTextButton* DialogDelegate::GetCancelButton() const {
  DCHECK(GetWidget()) << "Don't call this before OnWidgetInitialized";
  auto* client = GetDialogClientView();
  return client ? client->cancel_button() : nullptr;
}

views::View* DialogDelegate::GetExtraView() const {
  DCHECK(GetWidget()) << "Don't call this before OnWidgetInitialized";
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

void DialogDelegate::TriggerInputProtection(bool force_early) {
  GetDialogClientView()->TriggerInputProtection(force_early);
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
                                    std::u16string label) {
  if (params_.button_labels[button] == label)
    return;
  params_.button_labels[button] = label;
  DialogModelChanged();
}

void DialogDelegate::SetButtonStyle(ui::DialogButton button,
                                    absl::optional<ui::ButtonStyle> style) {
  if (params_.button_styles[button] == style) {
    return;
  }
  params_.button_styles[button] = style;
  DialogModelChanged();
}

void DialogDelegate::SetAcceptCallback(base::OnceClosure callback) {
  accept_callback_ = std::move(callback);
}

void DialogDelegate::SetAcceptCallbackWithClose(
    base::RepeatingCallback<bool()> callback) {
  accept_callback_ = std::move(callback);
}

void DialogDelegate::SetCancelCallback(base::OnceClosure callback) {
  cancel_callback_ = std::move(callback);
}

void DialogDelegate::SetCancelCallbackWithClose(
    base::RepeatingCallback<bool()> callback) {
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
  // Copy the dialog widget name onto the stack so it appears in crash dumps.
  DEBUG_ALIAS_FOR_CSTR(last_widget_name, GetWidget()->GetName().c_str(), 64);

  DCHECK(IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  if (already_started_close_ || !Accept())
    return;

  already_started_close_ = true;
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
}

void DialogDelegate::CancelDialog() {
  // If there's a close button available, this callback should only be reachable
  // if the cancel button is available. Otherwise this can be reached through
  // closing the dialog via Esc.
  if (ShouldShowCloseButton())
    DCHECK(IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  if (already_started_close_ || !Cancel())
    return;

  already_started_close_ = true;
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
}

DialogDelegate::~DialogDelegate() = default;

ax::mojom::Role DialogDelegate::GetAccessibleWindowRole() {
  return ax::mojom::Role::kDialog;
}

int DialogDelegate::GetCornerRadius() const {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/1116680): On Mac MODAL_TYPE_WINDOW is implemented using
  // sheets which causes visual artifacts when corner radius is increased for
  // modal types. Remove this after this issue has been addressed.
  if (GetModalType() == ui::MODAL_TYPE_WINDOW)
    return 2;
#endif
  if (params_.corner_radius)
    return *params_.corner_radius;
  return LayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kDialogRadius);
}

std::unique_ptr<View> DialogDelegate::DisownFootnoteView() {
  return std::move(footnote_view_);
}

////////////////////////////////////////////////////////////////////////////////
// DialogDelegateView:

DialogDelegateView::DialogDelegateView() {
  SetOwnedByWidget(true);
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
