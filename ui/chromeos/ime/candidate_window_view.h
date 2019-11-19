// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_IME_CANDIDATE_WINDOW_VIEW_H_
#define UI_CHROMEOS_IME_CANDIDATE_WINDOW_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace ui {
namespace ime {

class CandidateView;
class InformationTextArea;

// CandidateWindowView is the main container of the candidate window UI.
class UI_CHROMEOS_EXPORT CandidateWindowView
    : public views::BubbleDialogDelegateView,
      public views::ButtonListener {
 public:
  // The object can be monitored by the observer.
  class Observer {
   public:
    virtual ~Observer() {}
    // The function is called when a candidate is committed.
    virtual void OnCandidateCommitted(int index) = 0;
  };

  explicit CandidateWindowView(
      gfx::NativeView parent,
      int window_shell_id =
          -1 /* equals ash::ShellWindowId::kShellWindowId_Invalid */);
  ~CandidateWindowView() override;
  views::Widget* InitWidget();

  // Adds the given observer. The ownership is not transferred.
  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  // Removes the given observer.
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // Hides the lookup table.
  void HideLookupTable();

  // Hides the auxiliary text.
  void HideAuxiliaryText();

  // Hides the preedit text.
  void HidePreeditText();

  // Shows the lookup table.
  void ShowLookupTable();

  // Shows the auxiliary text.
  void ShowAuxiliaryText();

  // Shows the preedit text.
  void ShowPreeditText();

  // Updates the preedit text.
  void UpdatePreeditText(const base::string16& text);

  // Updates candidates of the candidate window from |candidate_window|.
  // Candidates are arranged per |orientation|.
  void UpdateCandidates(const ui::CandidateWindow& candidate_window);

  void SetCursorBounds(const gfx::Rect& cursor_bounds,
                       const gfx::Rect& composition_head);

 private:
  friend class CandidateWindowViewTest;

  // views::BubbleDialogDelegateView:
  const char* GetClassName() const override;
  int GetDialogButtons() const override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void SelectCandidateAt(int index_in_page);
  void UpdateVisibility();

  // Initializes the candidate views if needed.
  void MaybeInitializeCandidateViews(
      const ui::CandidateWindow& candidate_window);

  // The candidate window data model.
  ui::CandidateWindow candidate_window_;

  // The index in the current page of the candidate currently being selected.
  int selected_candidate_index_in_page_;

  // The observers of the object.
  base::ObserverList<Observer>::Unchecked observers_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.
  InformationTextArea* auxiliary_text_;
  InformationTextArea* preedit_;
  views::View* candidate_area_;

  // The candidate views are used for rendering candidates.
  std::vector<std::unique_ptr<CandidateView>> candidate_views_;

  // Current columns size in |candidate_area_|.
  gfx::Size previous_shortcut_column_size_;
  gfx::Size previous_candidate_column_size_;
  gfx::Size previous_annotation_column_size_;

  // The last cursor bounds.
  gfx::Rect cursor_bounds_;

  // The last composition head bounds.
  gfx::Rect composition_head_bounds_;

  // True if the candidate window should be shown with aligning with composition
  // text as opposed to the cursor.
  bool should_show_at_composition_head_;

  // True if the candidate window should be shown on the upper side of
  // composition text.
  bool should_show_upper_side_;

  // True if the candidate window was open.  This is used to determine when to
  // send OnCandidateWindowOpened and OnCandidateWindowClosed events.
  bool was_candidate_window_open_;

  // Corresponds to ash::ShellWindowId.
  const int window_shell_id_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindowView);
};

}  // namespace ime
}  // namespace ui

#endif  // UI_CHROMEOS_IME_CANDIDATE_WINDOW_VIEW_H_
