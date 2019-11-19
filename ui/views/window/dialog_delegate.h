// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_
#define UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/ui_base_types.h"
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
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT DialogDelegate : public WidgetDelegate {
 public:
  struct Params {
    Params();
    ~Params();
    base::Optional<int> default_button = base::nullopt;
    bool round_corners = true;
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
    // Prefer to use this field (via set_button_label) rather than override
    // GetDialogButtonLabel - see https://crbug.com/1011446
    base::string16 button_labels[ui::DIALOG_BUTTON_LAST + 1];
  };

  DialogDelegate();

  // Creates a widget at a default location.
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

  // Called when the DialogDelegate and its frame have finished initializing but
  // not been shown yet. Override this to perform customizations to the dialog
  // that need to happen after the dialog's widget, border, buttons, and so on
  // are ready.
  //
  // Overrides of this method should be quite rare - prefer to do dialog
  // customization before the frame/widget/etc are ready if at all possible, via
  // other setters on this class.
  virtual void OnDialogInitialized() {}

  // Returns a mask specifying which of the available DialogButtons are visible
  // for the dialog. Note: Dialogs with just an OK button are frowned upon.
  // DEPRECATED: Prefer to use set_buttons() below; this method is being
  // removed. See https://crbug.com/1011446.
  virtual int GetDialogButtons() const;

  // Returns the default dialog button. This should not be a mask as only
  // one button should ever be the default button.  Return
  // ui::DIALOG_BUTTON_NONE if there is no default.  Default
  // behavior is to return ui::DIALOG_BUTTON_OK or
  // ui::DIALOG_BUTTON_CANCEL (in that order) if they are
  // present, ui::DIALOG_BUTTON_NONE otherwise.
  int GetDefaultDialogButton() const;

  // Returns the label of the specified dialog button.
  virtual base::string16 GetDialogButtonLabel(ui::DialogButton button) const;

  // Returns whether the specified dialog button is enabled.
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const;

  // Override this function to display a footnote view below the buttons.
  // Overrides may construct the view; this will only be called once per dialog.
  // DEPRECATED: Prefer to use SetFootnoteView() below; this method is being
  // removed. See https://crbug.com/1011446.
  virtual std::unique_ptr<View> CreateFootnoteView();

  // For Dialog boxes, if there is a "Cancel" button or no dialog button at all,
  // this is called when the user presses the "Cancel" button.
  // It can also be called on a close action if |Close| has not been
  // overridden. This function should return true if the window can be closed
  // after it returns, or false if it must remain open.
  virtual bool Cancel();

  // For Dialog boxes, this is called when the user presses the "OK" button,
  // or the Enter key. It can also be called on a close action if |Close|
  // has not been overridden. This function should return true if the window
  // can be closed after it returns, or false if it must remain open.
  virtual bool Accept();

  // Called when the user closes the window without selecting an option,
  // e.g. by pressing the close button on the window, pressing the Esc key, or
  // using a window manager gesture. By default, this calls Accept() if the only
  // button in the dialog is Accept, Cancel() otherwise. This function should
  // return true if the window can be closed after it returns, or false if it
  // must remain open.
  virtual bool Close();

  // Overridden from WidgetDelegate:
  View* GetInitiallyFocusedView() override;
  DialogDelegate* AsDialogDelegate() override;
  ClientView* CreateClientView(Widget* widget) override;
  NonClientFrameView* CreateNonClientFrameView(Widget* widget) override;

  static NonClientFrameView* CreateDialogFrameView(Widget* widget);

  const gfx::Insets& margins() const { return margins_; }
  void set_margins(const gfx::Insets& margins) { margins_ = margins; }

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

  // A helper for accessing the DialogClientView object contained by this
  // delegate's Window.
  const DialogClientView* GetDialogClientView() const;
  DialogClientView* GetDialogClientView();

  // Returns the BubbleFrameView of this dialog delegate. A bubble frame view
  // will only be created when use_custom_frame() is true.
  BubbleFrameView* GetBubbleFrameView() const;

  // Helpers for accessing parts of the DialogClientView without needing to know
  // about DialogClientView. Do not call these before OnDialogInitialized.
  views::LabelButton* GetOkButton();
  views::LabelButton* GetCancelButton();
  views::View* GetExtraView();

  // Helper for accessing the footnote view. Unlike the three methods just
  // above, this *is* safe to call before OnDialogInitialized.
  views::View* GetFootnoteViewForTesting();

  // Add or remove an observer notified by calls to DialogModelChanged().
  void AddObserver(DialogObserver* observer);
  void RemoveObserver(DialogObserver* observer);

  // Notifies observers when the result of the DialogModel overrides changes.
  void DialogModelChanged();

  void set_default_button(int button) { params_.default_button = button; }
  void set_use_round_corners(bool round) { params_.round_corners = round; }
  void set_draggable(bool draggable) { params_.draggable = draggable; }
  bool draggable() const { return params_.draggable; }
  void set_use_custom_frame(bool use) { params_.custom_frame = use; }
  bool use_custom_frame() const { return params_.custom_frame; }
  void set_buttons(int buttons) { params_.buttons = buttons; }

  void set_button_label(ui::DialogButton button, base::string16 label) {
    params_.button_labels[button] = label;
  }

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

  // Externally or accept the dialog. These methods:
  // 1) Invoke the DialogDelegate's Cancel or Accept methods
  // 2) Depending on their return value, close the dialog's widget.
  // Neither of these methods can be called before the dialog has been
  // initialized.
  void CancelDialog();
  void AcceptDialog();

  // Reset the dialog's shown timestamp, for tests that are subject to the
  // "unintended interaction" detection mechanism.
  void ResetViewShownTimeStampForTesting();

 protected:
  ~DialogDelegate() override;

  // Overridden from WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() override;

  const Params& GetParams() const { return params_; }

 private:
  // Overridden from WidgetDelegate. If you need to hook after widget
  // initialization, use OnDialogInitialized above.
  void OnWidgetInitialized() final;

  // The margins between the content and the inside of the border.
  // TODO(crbug.com/733040): Most subclasses assume they must set their own
  // margins explicitly, so we set them to 0 here for now to avoid doubled
  // margins.
  gfx::Insets margins_{0};

  // The time the dialog is created.
  base::TimeTicks creation_time_;

  // Dialog parameters for this dialog.
  Params params_;

  // The extra view for this dialog, if there is one.
  std::unique_ptr<View> extra_view_ = nullptr;

  // The footnote view for this dialog, if there is one.
  std::unique_ptr<View> footnote_view_ = nullptr;

  // Observers for DialogModel changes.
  base::ObserverList<DialogObserver>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DialogDelegate);
};

// A DialogDelegate implementation that is-a View. Used to override GetWidget()
// to call View's GetWidget() for the common case where a DialogDelegate
// implementation is-a View. Note that DialogDelegateView is not owned by
// view's hierarchy and is expected to be deleted on DeleteDelegate call.
class VIEWS_EXPORT DialogDelegateView : public DialogDelegate,
                                        public View {
 public:
  DialogDelegateView();
  ~DialogDelegateView() override;

  // Overridden from DialogDelegate:
  void DeleteDelegate() override;
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  View* GetContentsView() override;

  // Overridden from View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DialogDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_
