// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
#define UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/dialog_observer.h"

namespace views {

class DialogDelegate;
class MdTextButton;
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
class VIEWS_EXPORT DialogClientView : public ClientView,
                                      public DialogObserver,
                                      public LayoutDelegate {
  METADATA_HEADER(DialogClientView, ClientView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOkButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonElementId);

  DialogClientView(Widget* widget, View* contents_view);

  DialogClientView(const DialogClientView&) = delete;
  DialogClientView& operator=(const DialogClientView&) = delete;

  ~DialogClientView() override;

  // Accessors in case the user wishes to adjust these buttons.
  MdTextButton* ok_button() const { return ok_button_; }
  MdTextButton* cancel_button() const { return cancel_button_; }
  View* extra_view() const { return extra_view_; }

  void SetButtonRowInsets(const gfx::Insets& insets);

  // View implementation:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ClientView implementation:
  void UpdateWindowRoundedCorners(int corner_radius) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Input protection is triggered upon prompt creation and updated on
  // visibility changes. Other situations such as top window changes in certain
  // situations should trigger the input protection manually by calling this
  // method. Input protection protects against certain kinds of clickjacking.
  // Essentially it prevents clicks that happen within a user's double click
  // interval from when the protection is started as well as any following
  // clicks that happen in shorter succession than the user's double click
  // interval. Refer to InputEventActivationProtector for more information. If
  // `force_early` is true, force to trigger even earlier (shortly before the
  // this view is visible).
  void TriggerInputProtection(bool force_early = false);

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnThemeChanged() override;

  // Update the `view_shown_time_stamp_` of input protector. A short time
  // from this point onward, input event will be ignored.
  void UpdateInputProtectorTimeStamp();

  void set_minimum_size(const gfx::Size& size) { minimum_size_ = size; }

  // Resets the time when view has been shown. Tests may need to call this
  // method if they use events that could be otherwise treated as unintended.
  // See IsPossiblyUnintendedInteraction().
  void ResetViewShownTimeStampForTesting();

  // Override the internal input protector for testing; usually to inject a mock
  // version whose return value can be controlled.
  void SetInputProtectorForTesting(
      std::unique_ptr<views::InputEventActivationProtector> input_protector) {
    input_protector_ = std::move(input_protector);
  }

  bool IsPossiblyUnintendedInteraction(const ui::Event& event);

  // LayoutDelegate:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

 private:
  enum {
    // The number of buttons that DialogClientView can support.
    kNumButtons = 3
  };
  class ButtonRowContainer;

  // Returns the DialogDelegate for the window.
  DialogDelegate* GetDialogDelegate() const;

  void SetBackgroundRadii(const gfx::RoundedCornersF& radii);

  void UpdateBackground();

  // DialogObserver:
  void OnDialogChanged() override;

  // Update the dialog buttons to match the dialog's delegate.
  void UpdateDialogButtons();
  void OnButtonVisibilityChanged(View* view);

  // Creates, deletes, or updates the appearance of the button of type `type`
  // (which must be pointed to by `member`).  Which action is chosen is based on
  // whether DialogDelegate::GetDialogButtons() includes `type`, and whether
  // `member` points to a button that already exists.
  void UpdateDialogButton(raw_ptr<MdTextButton>* member,
                          ui::mojom::DialogButton type);

  void ButtonPressed(ui::mojom::DialogButton type, const ui::Event& event);

  // Returns the spacing between the extra view and the ok/cancel buttons. 0 if
  // no extra view. Otherwise uses the default padding.
  int GetExtraViewSpacing() const;

  // Returns Views in the button row, as they should appear in the layout. If
  // a View should not appear, it will be null.
  std::array<View*, kNumButtons> GetButtonRowViews();

  // Installs and configures the LayoutManager for `button_row_container_`.
  void SetupLayout();

  // Creates or deletes any buttons that are required. Updates data members.
  // After calling this, no button row Views will be in the view hierarchy.
  void UpdateButtonsFromModel();

  // Ask the delegate for a new extra view. If there is one, replace the
  // existing extra view with it.
  void UpdateExtraViewFromDelegate();

  // Adds/Removes a filler view depending on whether the corresponding live view
  // is present.
  void AddFillerView(size_t view_index);
  void RemoveFillerView(size_t view_index);

  // How much to inset the button row.
  gfx::Insets button_row_insets_;

  // The minimum size of this dialog, regardless of the size of its content
  // view.
  gfx::Size minimum_size_;

  // The dialog buttons.
  raw_ptr<MdTextButton> ok_button_ = nullptr;
  raw_ptr<MdTextButton> cancel_button_ = nullptr;

  // The extra view shown in the row of buttons; may be nullptr.
  raw_ptr<View> extra_view_ = nullptr;

  // Container view for the button row.
  raw_ptr<ButtonRowContainer> button_row_container_ = nullptr;

  // List of "filler" views used to keep columns in sync for TableLayout.
  std::array<View*, kNumButtons> filler_views_ = {nullptr, nullptr, nullptr};

  // Used to prevent unnecessary or potentially harmful changes during
  // SetupLayout(). Everything will be manually updated afterwards.
  bool adding_or_removing_views_ = false;

  std::unique_ptr<InputEventActivationProtector> input_protector_;

  gfx::RoundedCornersF background_radii_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, DialogClientView, ClientView)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, DialogClientView)

#endif  // UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
