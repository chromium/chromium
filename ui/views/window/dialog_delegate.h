// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_
#define UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

class BubbleFrameView;
class DialogClientView;
class DialogObserver;
class LabelButton;

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
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT DialogDelegate : public WidgetDelegate {
 public:
  struct Params {
    Params();
    ~Params();
    absl::optional<int> default_button = absl::nullopt;
    bool round_corners = true;
    absl::optional<int> corner_radius = absl::nullopt;

    bool draggable = false;

    // Whether to use the Views-styled frame (if true) or a platform-native
    // frame if false. In general, dialogs that look like fully separate windows
    // should use the platform-native frame, and all other dialogs should use
    // the Views-styled one.
    bool custom_frame = true;

    // A bitmask of buttons (from ui::DialogButton) that are present in this
    // dialog.
    int buttons = ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;

    // Text labels for the buttons on this dialog. Any button without a label
    // here will get the default text for its type from GetDialogButtonLabel.
    // Prefer to use this field (via SetButtonLabel) rather than override
    // GetDialogButtonLabel - see https://crbug.com/1011446
    std::u16string button_labels[ui::DIALOG_BUTTON_LAST + 1];

    // A bitmask of buttons (from ui::DialogButton) that are enabled in this
    // dialog. It's legal for a button to be marked enabled that isn't present
    // in |buttons| (see above).
    int enabled_buttons = ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  };

  DialogDelegate();
  DialogDelegate(const DialogDelegate&) = delete;
  DialogDelegate& operator=(const DialogDelegate&) = delete;
  ~DialogDelegate() override;

  // Creates a widget at a default location.
  // There are two variant of this method. The newer one is the unique_ptr
  // method, which simply takes ownership of the WidgetDelegate and passes it to
  // the created Widget. When using the unique_ptr version, it is required that
  // delegate->owned_by_widget(). Unless you have a good reason, you should use
  // this variant.
  //
  // If !delegate->owned_by_widget() *or* if your WidgetDelegate subclass has a
  // custom override of WidgetDelegate::DeleteDelegate, use the raw pointer
  // variant instead, and please talk to one of the //ui/views owners about
  // your use case.
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
  // TODO(https://crbug.com/1011446): Rename this to buttons().
  int GetDialogButtons() const { return params_.buttons; }

  // Returns the default dialog button. This should not be a mask as only
  // one button should ever be the default button.  Return
  // ui::DIALOG_BUTTON_NONE if there is no default.  Default
  // behavior is to return ui::DIALOG_BUTTON_OK or
  // ui::DIALOG_BUTTON_CANCEL (in that order) if they are
  // present, ui::DIALOG_BUTTON_NONE otherwise.
  int GetDefaultDialogButton() const;

  // Returns the label of the specified dialog button.
  std::u16string GetDialogButtonLabel(ui::DialogButton button) const;

  // Returns whether the specified dialog button is enabled.
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const;

  // For Dialog boxes, if there is a "Cancel" button or no dialog button at all,
  // this is called when the user presses the "Cancel" button.  This function
  // should return true if the window can be closed after it returns, or false
  // if it must remain open. By default, return true without doing anything.
  // DEPRECATED: use |SetCancelCallback| instead.
  virtual bool Cancel();

  // For Dialog boxes, this is called when the user presses the "OK" button, or
  // the Enter key. This function should return true if the window can be closed
  // after it returns, or false if it must remain open. By default, return true
  // without doing anything.
  // DEPRECATED: use |SetAcceptCallback| instead.
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

  template <typename T>
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
  views::LabelButton* GetOkButton() const;
  views::LabelButton* GetCancelButton() const;
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
  // TODO(https://crbug.com/1011446): Make this private.
  void DialogModelChanged();

  // Input protection is triggered upon prompt creation and updated on
  // visibility changes. Other situations such as top window changes in certain
  // situations should trigger the input protection manually by calling this
  // method. Input protection protects against certain kinds of clickjacking.
  // Essentially it prevents clicks that happen within a user's double click
  // interval from when the protection is started as well as any following
  // clicks that happen in shorter succession than the user's double click
  // interval. Refer to InputEventActivationProtector for more information.
  void TriggerInputProtection();

  void set_use_round_corners(bool round) { params_.round_corners = round; }
  void set_corner_radius(int corner_radius) {
    params_.corner_radius = corner_radius;
  }
  const absl::optional<int> corner_radius() const {
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
  void SetButtonLabel(ui::DialogButton button, std::u16string label);
  void SetButtonEnabled(ui::DialogButton button, bool enabled);

  // Called when the user presses the dialog's "OK" button or presses the dialog
  // accept accelerator, if there is one.
  void SetAcceptCallback(base::OnceClosure callback);

  // Called when the user presses the dialog's "Cancel" button or presses the
  // dialog close accelerator (which is always VKEY_ESCAPE).
  void SetCancelCallback(base::OnceClosure callback);

  // Called when:
  // * The user presses the dialog's close button, if it has one
  // * The dialog's widget is closed via Widget::Close()
  // NOT called when the dialog's widget is closed via Widget::CloseNow() - in
  // that case, the normal widget close path is skipped, so no orderly teardown
  // of the dialog's widget happens. The main way that can happen in production
  // use is if the dialog's parent widget is closed.
  void SetCloseCallback(base::OnceClosure callback);

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
  std::unique_ptr<View> DisownExtraView();

  // Accept or cancel the dialog, as though the user had pressed the
  // Accept/Cancel buttons. These methods:
  // 1) Invoke the DialogDelegate's Cancel or Accept methods
  // 2) Depending on their return value, close the dialog's widget.
  // Neither of these methods can be called before the dialog has been
  // initialized.
  // NOT_TAIL_CALLED forces the calling function to appear on the stack in
  // crash dumps. https://crbug.com/1215247
  NOT_TAIL_CALLED void AcceptDialog();
  void CancelDialog();

  // This method invokes the behavior that *would* happen if this dialog's
  // containing widget were closed. It is present only as a compatibility shim
  // for unit tests; do not add new calls to it.
  // TODO(https://crbug.com/1011446): Delete this.
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

 protected:
  // Overridden from WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() override;

  const Params& GetParams() const { return params_; }

  int GetCornerRadius() const;

  // Return ownership of the footnote view for this dialog. Only use this in
  // subclass overrides of CreateNonClientFrameView.
  std::unique_ptr<View> DisownFootnoteView();

 private:
  // Runs a close callback, ensuring that at most one close callback is ever
  // run.
  void RunCloseCallback(base::OnceClosure callback);

  // The margins between the content and the inside of the border.
  // TODO(crbug.com/733040): Most subclasses assume they must set their own
  // margins explicitly, so we set them to 0 here for now to avoid doubled
  // margins.
  gfx::Insets margins_{0};

  // Use a fixed dialog width for dialog. Used by DialogClientView.
  int fixed_width_ = 0;

  // Dialog parameters for this dialog.
  Params params_;

  // The extra view for this dialog, if there is one.
  std::unique_ptr<View> extra_view_;

  // The footnote view for this dialog, if there is one.
  std::unique_ptr<View> footnote_view_;

  // Observers for DialogModel changes.
  base::ObserverList<DialogObserver>::Unchecked observer_list_;

  // Callbacks for the dialog's actions:
  base::OnceClosure accept_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure close_callback_;

  // Whether any of the three callbacks just above has been delivered yet, *or*
  // one of the Accept/Cancel methods have been called and returned true.
  bool already_started_close_ = false;
};

// A DialogDelegate implementation that is-a View. Used to override GetWidget()
// to call View's GetWidget() for the common case where a DialogDelegate
// implementation is-a View. Note that DialogDelegateView is not owned by
// view's hierarchy and is expected to be deleted on DeleteDelegate call.
//
// It is best not to add new uses of this class, and instead to subclass View
// directly and have a DialogDelegate member that you configure - essentially,
// to compose with DialogDelegate rather than inheriting from it.
// DialogDelegateView has unusual lifetime semantics that you can avoid dealing
// with, and your class will be smaller.
class VIEWS_EXPORT DialogDelegateView : public DialogDelegate, public View {
 public:
  METADATA_HEADER(DialogDelegateView);
  DialogDelegateView();
  DialogDelegateView(const DialogDelegateView&) = delete;
  DialogDelegateView& operator=(const DialogDelegateView&) = delete;
  ~DialogDelegateView() override;

  // DialogDelegate:
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  View* GetContentsView() override;
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
VIEW_BUILDER_PROPERTY(ui::ModalType, ModalType)
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
VIEW_BUILDER_METHOD(SetButtonLabel, ui::DialogButton, std::u16string)
VIEW_BUILDER_METHOD(SetButtonEnabled, ui::DialogButton, bool)
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
