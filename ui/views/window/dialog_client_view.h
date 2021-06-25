// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
#define UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/dialog_observer.h"

namespace views {

class DialogDelegate;
class LabelButton;
class Widget;

// DialogClientView provides adornments for a dialog's content view, including
// custom-labeled [OK] and [Cancel] buttons with [Enter] and [Esc] accelerators.
// The view also displays the delegate's extra view alongside the buttons. The
// view appears like below. NOTE: The contents view is not inset on the top or
// side client view edges.
//   +------------------------------+
//   |        Contents View         |
//   +------------------------------+
//   | [Extra View]   [OK] [Cancel] |
//   +------------------------------+
//
// You must not directly depend on or use DialogClientView; it is internal to
// //ui/views. Access it through the public interfaces on DialogDelegate. It is
// only VIEWS_EXPORT to make it available to views_unittests.
class VIEWS_EXPORT DialogClientView : public ClientView, public DialogObserver {
 public:
  METADATA_HEADER(DialogClientView);

  DialogClientView(Widget* widget, View* contents_view);
  ~DialogClientView() override;

  // Accessors in case the user wishes to adjust these buttons.
  LabelButton* ok_button() const { return ok_button_; }
  LabelButton* cancel_button() const { return cancel_button_; }
  View* extra_view() const { return extra_view_; }

  void SetButtonRowInsets(const gfx::Insets& insets);

  // View implementation:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  void Layout() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnThemeChanged() override;

  void set_minimum_size(const gfx::Size& size) { minimum_size_ = size; }

  // Resets the time when view has been shown. Tests may need to call this
  // method if they use events that could be otherwise treated as unintended.
  // See IsPossiblyUnintendedInteraction().
  void ResetViewShownTimeStampForTesting();

 private:
  enum {
    // The number of buttons that DialogClientView can support.
    kNumButtons = 3
  };
  class ButtonRowContainer;

  // Returns the DialogDelegate for the window.
  DialogDelegate* GetDialogDelegate() const;

  // View implementation.
  void ChildVisibilityChanged(View* child) override;

  // DialogObserver:
  void OnDialogChanged() override;

  // Update the dialog buttons to match the dialog's delegate.
  void UpdateDialogButtons();

  // Creates, deletes, or updates the appearance of the button of type |type|
  // (which must be pointed to by |member|).  Which action is chosen is based on
  // whether DialogDelegate::GetDialogButtons() includes |type|, and whether
  // |member| points to a button that already exists.
  void UpdateDialogButton(LabelButton** member, ui::DialogButton type);

  void ButtonPressed(ui::DialogButton type, const ui::Event& event);

  // Returns the spacing between the extra view and the ok/cancel buttons. 0 if
  // no extra view. Otherwise uses the default padding.
  int GetExtraViewSpacing() const;

  // Returns Views in the button row, as they should appear in the layout. If
  // a View should not appear, it will be null.
  std::array<View*, kNumButtons> GetButtonRowViews();

  // Installs and configures the LayoutManager for |button_row_container_|.
  void SetupLayout();

  // Creates or deletes any buttons that are required. Updates data members.
  // After calling this, no button row Views will be in the view hierarchy.
  void SetupViews();

  // How much to inset the button row.
  gfx::Insets button_row_insets_;

  // The minimum size of this dialog, regardless of the size of its content
  // view.
  gfx::Size minimum_size_;

  // The dialog buttons.
  LabelButton* ok_button_ = nullptr;
  LabelButton* cancel_button_ = nullptr;

  // The extra view shown in the row of buttons; may be NULL.
  View* extra_view_ = nullptr;

  // Container view for the button row.
  ButtonRowContainer* button_row_container_ = nullptr;

  // Used to prevent unnecessary or potentially harmful changes during
  // SetupLayout(). Everything will be manually updated afterwards.
  bool adding_or_removing_views_ = false;

  InputEventActivationProtector input_protector_;

  DISALLOW_COPY_AND_ASSIGN(DialogClientView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
