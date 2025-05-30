// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_
#define UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

class AppInfoDialogViewsTest;
class AuthenticatorRequestDialogView;
class AutoSigninFirstRunDialogView;
class BatchUploadDialogView;
class BluetoothDeviceCredentialsView;
class BluetoothDevicePairConfirmView;
class BookmarkEditorView;
class BruschettaInstallerView;
class CaretBrowsingDialogDelegate;
class CertificateSelector;
class ChooserDialogView;
class ConfirmBubbleViews;
class ConstrainedWindowTestDialog;
class CreateChromeApplicationShortcutView;
class CreateShortcutConfirmationView;
class CredentialLeakDialogView;
class CryptoModulePasswordDialogView;
class DeprecatedAppsDialogView;
class DesktopMediaPickerDialogView;
class DownloadDangerPromptViews;
class DownloadInProgressDialogView;
class ExtensionPopupInteractiveUiTest;
class ExternalProtocolDialog;
class FirstRunDialog;
class HungRendererDialogView;
class ImportLockDialogView;
class InteractiveBrowserTestDialog;
class JavaScriptTabModalDialogViewViews;
class NativeDialogContainer;
class OneClickSigninDialogView;
class ParentPermissionDialogView;
class RelaunchRequiredDialogView;
class RequestPinView;
class SelectAudioOutputDialog;
class ShareThisTabDialogView;
class SigninViewControllerDelegateViews;
class TabDragControllerTestDialog;
class TestWebModalDialog;
class UninstallView;
class WebAppIdentityUpdateConfirmationView;
class WebAppUninstallDialogDelegateView;
FORWARD_DECLARE_TEST(ExtensionPopupInteractiveUiTest,
                     ExtensionPopupClosesOnShowingWebDialog);

namespace arc {
class ArcAppDialogView;
class DataRemovalConfirmationDialog;
}  // namespace arc

namespace ash {
class AccessibilityConfirmationDialog;
class AccessibilityFeatureDisableDialog;
class CancelCastingDialog;
class ChildModalDialogDelegate;
class ConfirmSignoutDialog;
class DisplayChangeDialog;
class EchoDialogView;
class IdleActionWarningDialogView;
class LocalAuthenticationRequestView;
class LogoutConfirmationDialog;
class ManagementDisclosureDialog;
class MultiprofilesIntroDialog;
class PinRequestView;
class PublicAccountMonitoringInfoDialog;
class RequestSystemProxyCredentialsView;
class SessionAbortedDialog;
class ShutdownConfirmationDialog;
class TeleportWarningDialog;
FORWARD_DECLARE_TEST(SnapGroupDividerTest,
                     DividerStackingOrderWithDialogTransientUndoStacking);
FORWARD_DECLARE_TEST(SnapGroupDividerTest,
                     DividerStackingWhenResizingWithDialogTransient);

namespace enrollment {
class EnrollmentDialogView;
}

namespace printing::oauth2 {
class SigninDialog;
}
}  // namespace ash

namespace autofill {
class AutofillErrorDialogViewNativeViews;
class AutofillProgressDialogViews;
class BnplTosDialog;
class CardUnmaskOtpInputDialogViews;
class EditAddressProfileView;
class SaveAndFillDialog;
class WebauthnDialogView;

namespace payments {
class PaymentsWindowUserConsentDialogView;
class SelectBnplIssuerDialog;
}  // namespace payments
}  // namespace autofill

namespace borealis {
class BorealisSplashScreenView;
}

namespace extensions {
class SecurityDialogTrackerTest;
}

namespace glic {
class GlicFreDialogView;
}

namespace payments {
class PaymentRequestDialogView;
class SecurePaymentConfirmationDialogView;
class SecurePaymentConfirmationNoCredsDialogView;
}  // namespace payments

namespace policy {
class EnterpriseStartupDialogView;
class IdleDialogView;
class PolicyDialogBase;
}  // namespace policy

namespace remoting {
class MessageBoxCore;
}

namespace safe_browsing {
class PasswordReuseModalWarningDialog;
class PromptForScanningModalDialog;
class TailoredSecurityUnconsentedModal;
}  // namespace safe_browsing

namespace task_manager {
class TaskManagerView;
}

namespace web_app {
class LaunchAppUserChoiceDialogView;
}

namespace webid {
class AccountSelectionModalView;
}

namespace views {

class BubbleFrameView;
class DialogClientView;
class DialogClientViewTestDelegate;
class DialogObserver;
class InitialFocusTestDialog;
class MakeCloseSynchronousTest;
class TestDialog;
class TestDialogDelegateView;
FORWARD_DECLARE_TEST(DesktopScreenPositionClientTest, PositionDialog);
FORWARD_DECLARE_TEST(DialogDelegateCloseTest, AnyCallbackInhibitsDefaultClose);
FORWARD_DECLARE_TEST(DialogDelegateCloseTest,
                     CloseParentWidgetDoesNotInvokeCloseCallback);
FORWARD_DECLARE_TEST(
    DialogDelegateCloseTest,
    RecursiveCloseFromAcceptCallbackDoesNotTriggerSecondCallback);
FORWARD_DECLARE_TEST(DialogTest, AcceptCallbackWithCloseDoesClose);
FORWARD_DECLARE_TEST(DialogTest, AcceptCallbackWithCloseDoesNotClose);
FORWARD_DECLARE_TEST(DialogTest, CancelCallbackWithCloseDoesClose);
FORWARD_DECLARE_TEST(DialogTest, CancelCallbackWithCloseDoesNotClose);
FORWARD_DECLARE_TEST(DialogTest, ButtonEnableUpdatesState);
FORWARD_DECLARE_TEST(DialogTest, UnfocusableInitialFocus);

namespace examples {
class ColoredDialog;
template <class DialogType>
class DialogExampleDelegate;
class WidgetExample;
}  // namespace examples

namespace test {
class NativeWidgetMacTest;
class RootViewTestDialogDelegate;
FORWARD_DECLARE_TEST(DesktopNativeWidgetAuraTest, WindowModalityActivationTest);
FORWARD_DECLARE_TEST(DesktopNativeWidgetAuraTest, WindowMouseModalityTest);
FORWARD_DECLARE_TEST(DesktopWidgetTestInteractive,
                     DesktopNativeWidgetWithModalTransientChild);
FORWARD_DECLARE_TEST(DesktopWidgetTestInteractive,
                     WindowModalWindowDestroyedActivationTest);
FORWARD_DECLARE_TEST(WidgetCaptureTest, SystemModalWindowReleasesCapture);
}  // namespace test

///////////////////////////////////////////////////////////////////////////////
//
// DialogDelegate
//
//  DialogDelegate is an interface implemented by objects that wish to show a
//  dialog box Window. The window that is displayed uses this interface to
//  determine how it should be displayed and notify the delegate object of
//  certain events.
//
//  If possible, it is better to compose DialogDelegate rather than subclassing
//  it; it has many setters, including some inherited from WidgetDelegate, that
//  let you set properties on it without overriding virtuals. Doing this also
//  means you do not need to deal with implementing ::DeleteDelegate().
//
//  This class overrides WidgetDelegate::CreateClientView() to create an
//  instance of DialogClientView as the client view for the widget. This class
//  coordinates with DialogClientView to call Widget::Close() at 3 places:
//    * DialogDelegate::AcceptDialog() -- OK button pressed
//    * DialogDelegate::CancelDialog() -- cancel button pressed
//    * DialogClientView::AcceleratorPressed -- esc pressed
//  Subclasses have further coordination points.
//
//  The problem with Widget::Close() is that it's asynchronous, which doesn't
//  play well with CLIENT_OWNS_WIDGET, because it means that the client needs to
//  handle the edge case of Widget is closed (e.g. by this class), but not
//  destroyed.
//
//  The solution is to use the method: Widget::MakeCloseSynchronous(). This
//  allows the client to control how the widget is closed, which should
//  typically be resetting the unique_pt<Widget>. The combination of
//  Widget::MakeCloseSynchronous() and CLIENT_OWNS_WIDGET replaces: Cancel(),
//  Accept(), SetAcceptCallback(), SetCancelCallback(),
//  SetCancelCallbackWithClose(), SetCloseCallback(), Close(), AcceptDialog(),
//  CancelDialog().
//
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT DialogDelegate : public WidgetDelegate {
 public:
  struct Params {
    Params();
    ~Params();
    std::optional<int> default_button = std::nullopt;
    bool round_corners = true;
    std::optional<int> corner_radius = std::nullopt;

    bool draggable = false;

    // Whether to use the Views-styled frame (if true) or a platform-native
    // frame if false. In general, dialogs that look like fully separate windows
    // should use the platform-native frame, and all other dialogs should use
    // the Views-styled one.
    bool custom_frame = true;

    // A bitmask of buttons (from ui::mojom::DialogButton) that are present in
    // this dialog.
    int buttons = static_cast<int>(ui::mojom::DialogButton::kOk) |
                  static_cast<int>(ui::mojom::DialogButton::kCancel);

    // Text labels for the buttons on this dialog. Any button without a label
    // here will get the default text for its type from GetDialogButtonLabel.
    // Prefer to use this field (via SetButtonLabel) rather than override
    // GetDialogButtonLabel - see https://crbug.com/1011446
    std::array<std::u16string,
               static_cast<size_t>(ui::mojom::DialogButton::kCancel) + 1>
        button_labels;

    // Styles of each button on this dialog. If empty a style will be derived.
    std::array<std::optional<ui::ButtonStyle>,
               static_cast<size_t>(ui::mojom::DialogButton::kCancel) + 1>
        button_styles;

    // A bitmask of buttons (from ui::mojom::DialogButton) that are enabled in
    // this dialog. It's legal for a button to be marked enabled that isn't
    // present in |buttons| (see above).
    int enabled_buttons = static_cast<int>(ui::mojom::DialogButton::kOk) |
                          static_cast<int>(ui::mojom::DialogButton::kCancel);
  };

  DialogDelegate();
  DialogDelegate(const DialogDelegate&) = delete;
  DialogDelegate& operator=(const DialogDelegate&) = delete;
  ~DialogDelegate() override;

  // Creates a widget at a default location.
  // The correct approach is for the client to own the WidgetDelegate via a
  // unique_ptr, and for the client to own the Widget via a unique_ptr (see
  // CLIENT_OWNS_WIDGET), and to pass the WidgetDelegate as a raw_ptr.
  //
  // The unique_ptr variant is deprecated and requires calling
  // WidgetDelegate::SetOwnedByWidget().
  static Widget* CreateDialogWidget(std::unique_ptr<WidgetDelegate> delegate,
                                    gfx::NativeWindow context,
                                    gfx::NativeView parent);
  static Widget* CreateDialogWidget(WidgetDelegate* delegate,
                                    gfx::NativeWindow context,
                                    gfx::NativeView parent);

  // Whether using custom dialog frame is supported for this dialog.
  static bool CanSupportCustomFrame(gfx::NativeView parent);

  // Returns the dialog widget InitParams for a given |context| or |parent|.
  // If |bounds| is not empty, used to initially place the dialog, otherwise
  // a default location is used.
  static Widget::InitParams GetDialogWidgetInitParams(WidgetDelegate* delegate,
                                                      gfx::NativeWindow context,
                                                      gfx::NativeView parent,
                                                      const gfx::Rect& bounds);

  // Returns a mask specifying which of the available DialogButtons are visible
  // for the dialog.
  int buttons() const { return params_.buttons; }

  // Returns the default dialog button. This should not be a mask as only
  // one button should ever be the default button.  Return
  // ui::mojom::DialogButton::kNone if there is no default.  Default
  // behavior is to return ui::mojom::DialogButton::kOk or
  // ui::mojom::DialogButton::kCancel (in that order) if they are
  // present, ui::mojom::DialogButton::kNone otherwise.
  int GetDefaultDialogButton() const;

  // Returns the label of the specified dialog button.
  std::u16string GetDialogButtonLabel(ui::mojom::DialogButton button) const;

  // Returns the style of the specific dialog button.
  ui::ButtonStyle GetDialogButtonStyle(ui::mojom::DialogButton button) const;

  // Returns true if `button` should be the default button.
  bool GetIsDefault(ui::mojom::DialogButton button) const;

  // Returns whether the specified dialog button is enabled.
  virtual bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const;

  // Returns true if we should ignore key pressed event handling of `button`.
  virtual bool ShouldIgnoreButtonPressedEventHandling(
      View* button,
      const ui::Event& event) const;

  // For Dialog boxes, if there is a "Cancel" button or no dialog button at all,
  // this is called when the user presses the "Cancel" button.  This function
  // should return true if the window can be closed after it returns, or false
  // if it must remain open. By default, return true without doing anything.
  // DEPRECATED: use |Widget::MakeCloseSynchronous| instead.
  virtual bool Cancel();

  // For Dialog boxes, this is called when the user presses the "OK" button, or
  // the Enter key. This function should return true if the window can be closed
  // after it returns, or false if it must remain open. By default, return true
  // without doing anything.
  // DEPRECATED: use |Widget::MakeCloseSynchronous| instead.
  virtual bool Accept();

  // Overridden from WidgetDelegate:
  View* GetInitiallyFocusedView() override;
  DialogDelegate* AsDialogDelegate() override;
  ClientView* CreateClientView(Widget* widget) override;
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView(
      Widget* widget) override;

  static std::unique_ptr<NonClientFrameView> CreateDialogFrameView(
      Widget* widget);

  const gfx::Insets& margins() const { return margins_; }
  void set_margins(const gfx::Insets& margins) { margins_ = margins; }

  // Set a fixed width for the dialog. Used by DialogClientView.
  void set_fixed_width(int fixed_width) { fixed_width_ = fixed_width; }
  int fixed_width() const { return fixed_width_; }

  // Sets an extra view on the dialog button row. This can only be called once,
  // because of how the view is propagated into the Dialog.
  template <typename T = View>
  T* SetExtraView(std::unique_ptr<T> extra_view) {
    T* view = extra_view.get();
    extra_view_ = std::move(extra_view);
    return view;
  }

  template <typename T>
  T* SetFootnoteView(std::unique_ptr<T> footnote_view) {
    T* view = footnote_view.get();
    footnote_view_ = std::move(footnote_view);
    return view;
  }

  // Returns the BubbleFrameView of this dialog delegate. A bubble frame view
  // will only be created when use_custom_frame() is true.
  BubbleFrameView* GetBubbleFrameView() const;

  // A helper for accessing the DialogClientView object contained by this
  // delegate's Window. This function can return nullptr if the |client_view| is
  // a DialogClientView subclass which also has metadata or overrides
  // GetClassName().
  const DialogClientView* GetDialogClientView() const;
  DialogClientView* GetDialogClientView();

  // Helpers for accessing parts of the DialogClientView without needing to know
  // about DialogClientView. Do not call these before OnWidgetInitialized().
  views::MdTextButton* GetOkButton() const;
  views::MdTextButton* GetCancelButton() const;
  views::View* GetExtraView() const;

  // Helper for accessing the footnote view. Unlike the three methods just
  // above, this *is* safe to call before OnWidgetInitialized().
  views::View* GetFootnoteViewForTesting() const;

  // Add or remove an observer notified by calls to DialogModelChanged().
  void AddObserver(DialogObserver* observer);
  void RemoveObserver(DialogObserver* observer);

  // Notifies DialogDelegate that the result of one of the virtual getter
  // functions above has changed, which causes it to rebuild its layout. It is
  // not necessary to call this unless you are overriding
  // IsDialogButtonEnabled() or manually manipulating the dialog buttons.
  // TODO(crbug.com/40101916): Make this private.
  void DialogModelChanged();

  // Input protection is triggered upon prompt creation and updated on
  // visibility changes. Other situations such as top window changes in certain
  // situations should trigger the input protection manually by calling this
  // method. Input protection protects against certain kinds of clickjacking.
  // Essentially it prevents clicks that happen within a user's double click
  // interval from when the protection is started as well as any following
  // clicks that happen in shorter succession than the user's double click
  // interval. Refer to InputEventActivationProtector for more information.
  void TriggerInputProtection(bool force_early = false);

  void set_use_round_corners(bool round) { params_.round_corners = round; }
  void set_corner_radius(int corner_radius) {
    params_.corner_radius = corner_radius;
  }
  const std::optional<int> corner_radius() const {
    return params_.corner_radius;
  }
  void set_draggable(bool draggable) { params_.draggable = draggable; }
  bool draggable() const { return params_.draggable; }
  void set_use_custom_frame(bool use) { params_.custom_frame = use; }
  bool use_custom_frame() const { return params_.custom_frame; }

  // These methods internally call DialogModelChanged() if needed, so it is not
  // necessary to call DialogModelChanged() yourself after calling them.
  void SetDefaultButton(int button);
  void SetButtons(int buttons);
  void SetButtonLabel(ui::mojom::DialogButton dialog_button,
                      std::u16string_view label);
  void SetButtonStyle(ui::mojom::DialogButton button,
                      std::optional<ui::ButtonStyle> style);
  void SetButtonEnabled(ui::mojom::DialogButton dialog_button, bool enabled);

  // Called when the user presses the dialog's "OK" button or presses the dialog
  // accept accelerator, if there is one. The dialog is closed after the
  // callback is run.
  // DEPRECATED: use |Widget::MakeCloseSynchronous| instead, and handle the
  // case of Widget::ClosedReason == kAcceptButtonClicked.
  void SetAcceptCallback(base::OnceClosure callback);

  // Called when the user presses the dialog's "OK" button or presses the dialog
  // accept accelerator, if there is one. Callbacks can return true to close the
  // dialog, false to leave the dialog open.
  // Most use cases can use |Widget::MakeCloseSynchronous| instead, and handle
  // the case of Widget::ClosedReason == kAcceptButtonClicked.
  // Currently, there is no other mechanism to handle the accept and not close
  // the dialog. This should eventually be replaced with a new method
  // SetUserDidAcceptCallback() which does nothing other than run the callback.
  void SetAcceptCallbackWithClose(base::RepeatingCallback<bool()> callback);

  // Called when the user cancels the dialog, which can happen either by:
  //   * Clicking the Cancel button, if there is one, or
  //   * Closing the dialog with the Esc key, if the dialog has a close button
  //     but no close callback
  // The dialog is closed after the callback is run. The callback variant which
  // returns a bool decides whether the dialog actually closes or not; returning
  // false prevents closing, returning true allows closing.
  // DEPRECATED: use |Widget::MakeCloseSynchronous| instead, and handle the
  // case of Widget::ClosedReason != kAcceptButtonClicked.
  void SetCancelCallback(base::OnceClosure callback);
  // Currently, there is no other mechanism to handle the cancel and not close
  // the dialog. This should eventually be replaced with a new method
  // SetUserDidCancelCallback() which does nothing other than run the callback.
  void SetCancelCallbackWithClose(base::RepeatingCallback<bool()> callback);

  // Called when:
  // * The user presses the dialog's close button, if it has one
  // * The dialog's widget is closed via Widget::Close()
  // NOT called when the dialog's widget is closed via Widget::CloseNow() - in
  // that case, the normal widget close path is skipped, so no orderly teardown
  // of the dialog's widget happens. The main way that can happen in production
  // use is if the dialog's parent widget is closed.
  // DEPRECATED. When using a Widget with CLIENT_OWNS_WIDGET and
  // Widget::MakeCloseSynchronous(), the only way to close a Widget is by
  // resetting the unique_ptr. That means this method is no longer required.
  void SetCloseCallback(base::OnceClosure callback);

  // Sets the ownership of the views::Widget created by CreateDialogWidget().
  void SetOwnershipOfNewWidget(Widget::InitParams::Ownership ownership);

  // Returns ownership of the extra view for this dialog, if one was provided
  // via SetExtraView(). This is only for use by DialogClientView; don't call
  // it.
  // It would be good to instead have a DialogClientView::SetExtraView method
  // that passes ownership into DialogClientView once. Unfortunately doing this
  // broke a bunch of tests in a subtle way: the obvious place to call
  // DCV::SetExtraView was from DD::OnWidgetInitialized.  DCV::SetExtraView
  // would then add the new view to DCV, which would invalidate its layout.
  // However, many tests were doing essentially this:
  //
  //   TestDialogDelegate delegate;
  //   ShowBubble(&delegate);
  //   TryToUseExtraView();
  //
  // and then trying to use the extra view's bounds, most commonly by
  // synthesizing mouse clicks on it. At this point the extra view's layout is
  // invalid *but* it has not yet been laid out, because View::InvalidateLayout
  // schedules a deferred re-layout later. The design where DCV pulls the extra
  // view from DD doesn't have this issue: during the initial construction of
  // DCV, DCV fetches the extra view and slots it into its layout, and then the
  // initial layout pass in Widget::Init causes the extra view to get laid out.
  // Deferring inserting the extra view until after Widget::Init has finished is
  // what causes the extra view to not be laid out (and hence the tests to
  // fail).
  //
  // Potential future fixes:
  // 1) The tests could manually force a re-layout here, or
  // 2) The tests could be rewritten to not depend on the extra view's
  //    bounds, by not trying to deliver mouse events to it somehow, or
  // 3) DCV::SetupLayout could always force an explicit Layout, ignoring the
  //    lazy layout system in View::InvalidateLayout
  std::optional<std::unique_ptr<View>> DisownExtraView();

  // Accept or cancel the dialog, as though the user had pressed the
  // Accept/Cancel buttons. These methods:
  // 1) Invoke the DialogDelegate's Cancel or Accept methods
  // 2) Depending on their return value, close the dialog's widget.
  // Neither of these methods can be called before the dialog has been
  // initialized.
  // DEPRECATED. To close the dialog, reset the unique_ptr instead.
  void AcceptDialog();
  void CancelDialog();

  // This method invokes the behavior that *would* happen if this dialog's
  // containing widget were closed. It is present only as a compatibility shim
  // for unit tests; do not add new calls to it.
  // DEPRECATED. Use CLIENT_OWNS_WIDGET and reset the unique_ptr instead.
  // TODO(crbug.com/40101916): Delete this.
  bool Close();

  // Reset the dialog's shown timestamp, for tests that are subject to the
  // "unintended interaction" detection mechanism.
  void ResetViewShownTimeStampForTesting();

  // Set the insets used for the dialog's button row. This should be used only
  // rarely.
  // TODO(ellyjones): Investigate getting rid of this entirely and having all
  // dialogs use the same button row insets.
  void SetButtonRowInsets(const gfx::Insets& insets);

  // Callback for WidgetDelegate when the window this dialog is hosted in is
  // closing. Don't call this yourself.
  void WindowWillClose();

  // Returns whether the delegate's CancelDialog() should be called instead of
  // closing the Widget when Esc is pressed. Called by DialogClientView.
  bool EscShouldCancelDialog() const;

  // Explicitly sets the behavior of `EscShouldCancelDialog()`.
  // Useful if something other than the default logic is needed.
  void set_esc_should_cancel_dialog_override(
      std::optional<bool> esc_should_cancel_dialog_override) {
    esc_should_cancel_dialog_override_ = esc_should_cancel_dialog_override;
  }

  // Returns the corner radius that is used for this dialog.
  int GetCornerRadius() const;

  bool allow_vertical_buttons() const { return allow_vertical_buttons_; }
  void set_allow_vertical_buttons(bool allow) {
    allow_vertical_buttons_ = allow;
  }

 protected:
  // Overridden from WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() override;

  const Params& GetParams() const { return params_; }

  // Return ownership of the footnote view for this dialog. Only use this in
  // subclass overrides of CreateNonClientFrameView.
  std::unique_ptr<View> DisownFootnoteView();

 private:
  // Runs a close callback, ensuring that at most one close callback is run
  // if `callback` is a OnceClosure or returns true.
  bool RunCloseCallback(
      std::variant<base::OnceClosure, base::RepeatingCallback<bool()>>&
          callback);

  // The margins between the content and the inside of the border.
  // TODO(crbug.com/41325252): Most subclasses assume they must set their own
  // margins explicitly, so we set them to 0 here for now to avoid doubled
  // margins.
  gfx::Insets margins_{0};

  // Use a fixed dialog width for dialog. Used by DialogClientView.
  int fixed_width_ = 0;

  // Dialog parameters for this dialog.
  Params params_;

  // The extra view for this dialog, if there is one.
  std::optional<std::unique_ptr<View>> extra_view_;

  // The footnote view for this dialog, if there is one.
  std::unique_ptr<View> footnote_view_;

  // Observers for DialogModel changes.
  base::ObserverList<DialogObserver>::UncheckedAndDanglingUntriaged
      observer_list_;

  // Callbacks for the dialog's actions:
  std::variant<base::OnceClosure, base::RepeatingCallback<bool()>>
      accept_callback_;
  std::variant<base::OnceClosure, base::RepeatingCallback<bool()>>
      cancel_callback_;
  base::OnceClosure close_callback_;

  // Whether any of the three callbacks just above has been delivered yet and
  // returned true, *or* one of the Accept/Cancel methods have been called and
  // returned true.
  bool already_started_close_ = false;

  // If set, changes the behavior of EscShouldCancelDialog() to return the
  // specified value.
  std::optional<bool> esc_should_cancel_dialog_override_;

  // Ownership of the views::Widget created by CreateDialogWidget().
  Widget::InitParams::Ownership ownership_of_new_widget_ =
      Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;

  // If set, allows the dialog buttons to be arranged in a vertical
  // layout to maintain fixed dialog width. Specifically, if an extra view has
  // been supplied (commonly a third button), and the width of the resulting
  // row of buttons exceeds the specified `fixed_width_`, buttons are stacked
  // in a column instead. Conventionally, three-button dialogs are designed
  // with a width large enough to accommodate the required horizontal width.
  // This switch is an experiment to explore an alternate approach.
  bool allow_vertical_buttons_ = false;
};

// A DialogDelegate implementation that is-a View. Used to override GetWidget()
// to call View's GetWidget() for the common case where a DialogDelegate
// implementation is-a View. Note that DialogDelegateView is not owned by
// view's hierarchy and is expected to be deleted on DeleteDelegate call.
//
// DEPRECATED: Using this class makes it more challenging to reason about object
// ownership/lifetimes and promotes writing "fat" views that also contain
// business logic. Instead, use DialogModel if possible; otherwise, use separate
// subclasses of DialogDelegate and View to handle those interfaces' respective
// concerns.
class VIEWS_EXPORT DialogDelegateView : public DialogDelegate, public View {
  METADATA_HEADER(DialogDelegateView, View)

 public:
  // Not named `PassKey` as `View::PassKey` already exists in this hierarchy.
  using DdvPassKey = base::PassKey<DialogDelegateView>;

  // For use with std::make_unique<>(). Callers still must be in the friend list
  // below, just as with the private constructor.
  explicit DialogDelegateView(DdvPassKey) {}
  DialogDelegateView(const DialogDelegateView&) = delete;
  DialogDelegateView& operator=(const DialogDelegateView&) = delete;
  ~DialogDelegateView() override;

  // DialogDelegate:
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  View* GetContentsView() override;

 private:
  // DO NOT ADD TO THIS LIST!
  // These existing cases are "grandfathered in", but there shouldn't be more.
  // See comments atop class.
  friend class ::AppInfoDialogViewsTest;
  friend class ::AuthenticatorRequestDialogView;
  friend class ::AutoSigninFirstRunDialogView;
  friend class ::BatchUploadDialogView;
  friend class ::BluetoothDeviceCredentialsView;
  friend class ::BluetoothDevicePairConfirmView;
  friend class ::BookmarkEditorView;
  friend class ::BruschettaInstallerView;
  friend class ::CaretBrowsingDialogDelegate;
  friend class ::CertificateSelector;
  friend class ::ChooserDialogView;
  friend class ::ConfirmBubbleViews;
  friend class ::ConstrainedWindowTestDialog;
  friend class ::CreateChromeApplicationShortcutView;
  friend class ::CreateShortcutConfirmationView;
  friend class ::CredentialLeakDialogView;
  friend class ::CryptoModulePasswordDialogView;
  friend class ::DeprecatedAppsDialogView;
  friend class ::DesktopMediaPickerDialogView;
  friend class ::DownloadDangerPromptViews;
  friend class ::DownloadInProgressDialogView;
  friend class ::ExtensionPopupInteractiveUiTest;
  friend class ::ExternalProtocolDialog;
  friend class ::FirstRunDialog;
  friend class ::HungRendererDialogView;
  friend class ::ImportLockDialogView;
  friend class ::InteractiveBrowserTestDialog;
  friend class ::JavaScriptTabModalDialogViewViews;
  friend class ::NativeDialogContainer;
  friend class ::OneClickSigninDialogView;
  friend class ::ParentPermissionDialogView;
  friend class ::RelaunchRequiredDialogView;
  friend class ::RequestPinView;
  friend class ::SelectAudioOutputDialog;
  friend class ::ShareThisTabDialogView;
  friend class ::SigninViewControllerDelegateViews;
  friend class ::TabDragControllerTestDialog;
  friend class ::TestWebModalDialog;
  friend class ::UninstallView;
  friend class ::WebAppIdentityUpdateConfirmationView;
  friend class ::WebAppUninstallDialogDelegateView;
  FRIEND_TEST_ALL_PREFIXES(::ExtensionPopupInteractiveUiTest,
                           ExtensionPopupClosesOnShowingWebDialog);
  friend class ::arc::ArcAppDialogView;
  friend class ::arc::DataRemovalConfirmationDialog;
  friend class ::ash::AccessibilityConfirmationDialog;
  friend class ::ash::AccessibilityFeatureDisableDialog;
  friend class ::ash::CancelCastingDialog;
  friend class ::ash::ChildModalDialogDelegate;
  friend class ::ash::ConfirmSignoutDialog;
  friend class ::ash::DisplayChangeDialog;
  friend class ::ash::EchoDialogView;
  friend class ::ash::IdleActionWarningDialogView;
  friend class ::ash::LocalAuthenticationRequestView;
  friend class ::ash::LogoutConfirmationDialog;
  friend class ::ash::ManagementDisclosureDialog;
  friend class ::ash::MultiprofilesIntroDialog;
  friend class ::ash::PinRequestView;
  friend class ::ash::PublicAccountMonitoringInfoDialog;
  friend class ::ash::RequestSystemProxyCredentialsView;
  friend class ::ash::SessionAbortedDialog;
  friend class ::ash::ShutdownConfirmationDialog;
  friend class ::ash::TeleportWarningDialog;
  FRIEND_TEST_ALL_PREFIXES(::ash::SnapGroupDividerTest,
                           DividerStackingOrderWithDialogTransientUndoStacking);
  FRIEND_TEST_ALL_PREFIXES(::ash::SnapGroupDividerTest,
                           DividerStackingWhenResizingWithDialogTransient);
  friend class ::ash::enrollment::EnrollmentDialogView;
  friend class ::ash::printing::oauth2::SigninDialog;
  friend class ::autofill::AutofillErrorDialogViewNativeViews;
  friend class ::autofill::AutofillProgressDialogViews;
  friend class ::autofill::BnplTosDialog;
  friend class ::autofill::CardUnmaskOtpInputDialogViews;
  friend class ::autofill::EditAddressProfileView;
  friend class ::autofill::SaveAndFillDialog;
  friend class ::autofill::WebauthnDialogView;
  friend class ::autofill::payments::PaymentsWindowUserConsentDialogView;
  friend class ::autofill::payments::SelectBnplIssuerDialog;
  friend class ::borealis::BorealisSplashScreenView;
  friend class ::extensions::SecurityDialogTrackerTest;
  friend class ::glic::GlicFreDialogView;
  friend class ::payments::PaymentRequestDialogView;
  friend class ::payments::SecurePaymentConfirmationDialogView;
  friend class ::payments::SecurePaymentConfirmationNoCredsDialogView;
  friend class ::policy::EnterpriseStartupDialogView;
  friend class ::policy::IdleDialogView;
  friend class ::policy::PolicyDialogBase;
  friend class ::remoting::MessageBoxCore;
  friend class ::safe_browsing::PasswordReuseModalWarningDialog;
  friend class ::safe_browsing::PromptForScanningModalDialog;
  friend class ::safe_browsing::TailoredSecurityUnconsentedModal;
  friend class ::task_manager::TaskManagerView;
  friend class DialogClientViewTestDelegate;
  friend class InitialFocusTestDialog;
  friend class MakeCloseSynchronousTest;
  friend class TestDialog;
  friend class TestDialogDelegateView;
  FRIEND_TEST_ALL_PREFIXES(DesktopScreenPositionClientTest, PositionDialog);
  FRIEND_TEST_ALL_PREFIXES(DialogDelegateCloseTest,
                           AnyCallbackInhibitsDefaultClose);
  FRIEND_TEST_ALL_PREFIXES(DialogDelegateCloseTest,
                           CloseParentWidgetDoesNotInvokeCloseCallback);
  FRIEND_TEST_ALL_PREFIXES(
      DialogDelegateCloseTest,
      RecursiveCloseFromAcceptCallbackDoesNotTriggerSecondCallback);
  FRIEND_TEST_ALL_PREFIXES(DialogTest, AcceptCallbackWithCloseDoesClose);
  FRIEND_TEST_ALL_PREFIXES(DialogTest, AcceptCallbackWithCloseDoesNotClose);
  FRIEND_TEST_ALL_PREFIXES(DialogTest, CancelCallbackWithCloseDoesClose);
  FRIEND_TEST_ALL_PREFIXES(DialogTest, CancelCallbackWithCloseDoesNotClose);
  FRIEND_TEST_ALL_PREFIXES(DialogTest, ButtonEnableUpdatesState);
  FRIEND_TEST_ALL_PREFIXES(DialogTest, UnfocusableInitialFocus);
  friend class examples::ColoredDialog;
  friend class examples::DialogExampleDelegate<DialogDelegateView>;
  friend class examples::WidgetExample;
  friend class test::NativeWidgetMacTest;
  friend class test::RootViewTestDialogDelegate;
  FRIEND_TEST_ALL_PREFIXES(test::DesktopNativeWidgetAuraTest,
                           WindowModalityActivationTest);
  FRIEND_TEST_ALL_PREFIXES(test::DesktopNativeWidgetAuraTest,
                           WindowMouseModalityTest);
  FRIEND_TEST_ALL_PREFIXES(test::DesktopWidgetTestInteractive,
                           DesktopNativeWidgetWithModalTransientChild);
  FRIEND_TEST_ALL_PREFIXES(test::DesktopWidgetTestInteractive,
                           WindowModalWindowDestroyedActivationTest);
  FRIEND_TEST_ALL_PREFIXES(test::WidgetCaptureTest,
                           SystemModalWindowReleasesCapture);
  friend class ::web_app::LaunchAppUserChoiceDialogView;
  friend class ::webid::AccountSelectionModalView;

  DialogDelegateView();

  static DdvPassKey CreatePassKey() { return DdvPassKey(); }
};

// Explicitly instantiate the following templates to ensure proper linking,
// especially when using GCC.
template View* DialogDelegate::SetExtraView<View>(std::unique_ptr<View>);
template View* DialogDelegate::SetFootnoteView<View>(std::unique_ptr<View>);

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, DialogDelegateView, View)
VIEW_BUILDER_PROPERTY(ax::mojom::Role, AccessibleWindowRole)
VIEW_BUILDER_PROPERTY(std::u16string, AccessibleTitle)
VIEW_BUILDER_PROPERTY(bool, CanMaximize)
VIEW_BUILDER_PROPERTY(bool, CanMinimize)
VIEW_BUILDER_PROPERTY(bool, CanResize)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, ExtraView)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, FootnoteView)
VIEW_BUILDER_PROPERTY(bool, FocusTraversesOut)
VIEW_BUILDER_PROPERTY(bool, EnableArrowKeyTraversal)
VIEW_BUILDER_PROPERTY(ui::ImageModel, Icon)
VIEW_BUILDER_PROPERTY(ui::ImageModel, AppIcon)
VIEW_BUILDER_PROPERTY(ui::mojom::ModalType, ModalType)
VIEW_BUILDER_PROPERTY(bool, OwnedByWidget)
VIEW_BUILDER_PROPERTY(bool, ShowCloseButton)
VIEW_BUILDER_PROPERTY(bool, ShowIcon)
VIEW_BUILDER_PROPERTY(bool, ShowTitle)
VIEW_BUILDER_OVERLOAD_METHOD_CLASS(WidgetDelegate,
                                   SetTitle,
                                   const std::u16string&)
VIEW_BUILDER_OVERLOAD_METHOD_CLASS(WidgetDelegate, SetTitle, int)
#if defined(USE_AURA)
VIEW_BUILDER_PROPERTY(bool, CenterTitle)
#endif
VIEW_BUILDER_PROPERTY(int, Buttons)
VIEW_BUILDER_PROPERTY(int, DefaultButton)
VIEW_BUILDER_METHOD(SetButtonLabel, ui::mojom::DialogButton, std::u16string)
VIEW_BUILDER_METHOD(SetButtonEnabled, ui::mojom::DialogButton, bool)
VIEW_BUILDER_METHOD(set_margins, gfx::Insets)
VIEW_BUILDER_METHOD(set_use_round_corners, bool)
VIEW_BUILDER_METHOD(set_corner_radius, int)
VIEW_BUILDER_METHOD(set_draggable, bool)
VIEW_BUILDER_METHOD(set_use_custom_frame, bool)
VIEW_BUILDER_METHOD(set_fixed_width, int)
VIEW_BUILDER_PROPERTY(base::OnceClosure, AcceptCallback)
VIEW_BUILDER_PROPERTY(base::OnceClosure, CancelCallback)
VIEW_BUILDER_PROPERTY(base::OnceClosure, CloseCallback)
VIEW_BUILDER_PROPERTY(const gfx::Insets&, ButtonRowInsets)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, DialogDelegateView)

#endif  // UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_
