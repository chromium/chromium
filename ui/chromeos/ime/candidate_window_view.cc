// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ime/candidate_window_view.h"

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/chromeos/ime/candidate_view.h"
#include "ui/chromeos/ime/candidate_window_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/wm/core/window_animations.h"

namespace ui {
namespace ime {

namespace {

class CandidateWindowBorder : public views::BubbleBorder {
 public:
  CandidateWindowBorder()
      : views::BubbleBorder(views::BubbleBorder::TOP_CENTER,
                            views::BubbleBorder::BIG_SHADOW,
                            gfx::kPlaceholderColor),
        offset_(0) {
    set_use_theme_background_color(true);
  }
  ~CandidateWindowBorder() override {}

  void set_offset(int offset) { offset_ = offset; }

 private:
  // Overridden from views::BubbleBorder:
  gfx::Rect GetBounds(const gfx::Rect& anchor_rect,
                      const gfx::Size& content_size) const override {
    gfx::Rect bounds(content_size);
    bounds.set_origin(gfx::Point(
        anchor_rect.x() - offset_,
        is_arrow_on_top(arrow()) ?
        anchor_rect.bottom() : anchor_rect.y() - content_size.height()));

    // It cannot use the normal logic of arrow offset for horizontal offscreen,
    // because the arrow must be in the content's edge. But CandidateWindow has
    // to be visible even when |anchor_rect| is out of the screen.
    gfx::Rect work_area =
        display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
    if (bounds.right() > work_area.right())
      bounds.set_x(work_area.right() - bounds.width());
    if (bounds.x() < work_area.x())
      bounds.set_x(work_area.x());

    return bounds;
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(); }

  int offset_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindowBorder);
};

// Computes the page index. For instance, if the page size is 9, and the
// cursor is pointing to 13th candidate, the page index will be 1 (2nd
// page, as the index is zero-origin). Returns -1 on error.
int ComputePageIndex(const ui::CandidateWindow& candidate_window) {
  if (candidate_window.page_size() > 0)
    return candidate_window.cursor_position() / candidate_window.page_size();
  return -1;
}

}  // namespace

class InformationTextArea : public views::View {
 public:
  // InformationTextArea's border is drawn as a separator, it should appear
  // at either top or bottom.
  enum BorderPosition {
    TOP,
    BOTTOM
  };

  // Specify the alignment and initialize the control.
  InformationTextArea(gfx::HorizontalAlignment align, int min_width)
      : min_width_(min_width) {
    label_ = new views::Label;
    label_->SetHorizontalAlignment(align);
    label_->SetBorder(views::CreateEmptyBorder(2, 2, 2, 4));

    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(label_);
    SetBackground(views::CreateSolidBackground(
        color_utils::AlphaBlend(SK_ColorBLACK,
                                GetNativeTheme()->GetSystemColor(
                                    ui::NativeTheme::kColorId_WindowBackground),
                                0.0625f)));
  }

  // Sets the text alignment.
  void SetAlignment(gfx::HorizontalAlignment alignment) {
    label_->SetHorizontalAlignment(alignment);
  }

  // Sets the displayed text.
  void SetText(const base::string16& text) {
    label_->SetText(text);
  }

  // Sets the border thickness for top/bottom.
  void SetBorderFromPosition(BorderPosition position) {
    SetBorder(views::CreateSolidSidedBorder(
        (position == TOP) ? 1 : 0, 0, (position == BOTTOM) ? 1 : 0, 0,
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_MenuBorderColor)));
  }

 protected:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = views::View::CalculatePreferredSize();
    size.SetToMax(gfx::Size(min_width_, 0));
    return size;
  }

 private:
  views::Label* label_;
  int min_width_;

  DISALLOW_COPY_AND_ASSIGN(InformationTextArea);
};

CandidateWindowView::CandidateWindowView(gfx::NativeView parent,
                                         int window_shell_id)
    : selected_candidate_index_in_page_(-1),
      should_show_at_composition_head_(false),
      should_show_upper_side_(false),
      was_candidate_window_open_(false),
      window_shell_id_(window_shell_id) {
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());

  SetBorder(views::CreateSolidBorder(
      1, GetNativeTheme()->GetSystemColor(
             ui::NativeTheme::kColorId_MenuBorderColor)));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auxiliary_text_ = new InformationTextArea(gfx::ALIGN_RIGHT, 0);
  preedit_ = new InformationTextArea(gfx::ALIGN_LEFT, kMinPreeditAreaWidth);
  candidate_area_ = new views::View;
  auxiliary_text_->SetVisible(false);
  preedit_->SetVisible(false);
  candidate_area_->SetVisible(false);
  preedit_->SetBorderFromPosition(InformationTextArea::BOTTOM);
  if (candidate_window_.orientation() == ui::CandidateWindow::VERTICAL) {
    AddChildView(preedit_);
    AddChildView(candidate_area_);
    AddChildView(auxiliary_text_);
    auxiliary_text_->SetBorderFromPosition(InformationTextArea::TOP);
    candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
  } else {
    AddChildView(preedit_);
    AddChildView(auxiliary_text_);
    AddChildView(candidate_area_);
    auxiliary_text_->SetAlignment(gfx::ALIGN_LEFT);
    auxiliary_text_->SetBorderFromPosition(InformationTextArea::BOTTOM);
    candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
  }
}

CandidateWindowView::~CandidateWindowView() {
}

views::Widget* CandidateWindowView::InitWidget() {
  views::Widget* widget = BubbleDialogDelegateView::CreateBubble(this);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);

  GetBubbleFrameView()->SetBubbleBorder(
      std::make_unique<CandidateWindowBorder>());
  GetBubbleFrameView()->OnThemeChanged();
  return widget;
}

void CandidateWindowView::UpdateVisibility() {
  if (candidate_area_->GetVisible() || auxiliary_text_->GetVisible() ||
      preedit_->GetVisible()) {
    SizeToContents();
  } else {
    GetWidget()->Close();
  }
}

void CandidateWindowView::HideLookupTable() {
  candidate_area_->SetVisible(false);
  auxiliary_text_->SetVisible(false);
  UpdateVisibility();
}

void CandidateWindowView::HidePreeditText() {
  preedit_->SetVisible(false);
  UpdateVisibility();
}

void CandidateWindowView::ShowPreeditText() {
  preedit_->SetVisible(true);
  UpdateVisibility();
}

void CandidateWindowView::UpdatePreeditText(const base::string16& text) {
  preedit_->SetText(text);
}

void CandidateWindowView::ShowLookupTable() {
  candidate_area_->SetVisible(true);
  auxiliary_text_->SetVisible(candidate_window_.is_auxiliary_text_visible());
  UpdateVisibility();
}

void CandidateWindowView::UpdateCandidates(
    const ui::CandidateWindow& new_candidate_window) {
  // Updating the candidate views is expensive. We'll skip this if possible.
  if (!candidate_window_.IsEqual(new_candidate_window)) {
    if (candidate_window_.orientation() != new_candidate_window.orientation()) {
      // If the new layout is vertical, the aux text should appear at the
      // bottom. If horizontal, it should appear between preedit and candidates.
      if (new_candidate_window.orientation() == ui::CandidateWindow::VERTICAL) {
        ReorderChildView(auxiliary_text_, -1);
        auxiliary_text_->SetAlignment(gfx::ALIGN_RIGHT);
        auxiliary_text_->SetBorderFromPosition(InformationTextArea::TOP);
        candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
      } else {
        ReorderChildView(auxiliary_text_, 1);
        auxiliary_text_->SetAlignment(gfx::ALIGN_LEFT);
        auxiliary_text_->SetBorderFromPosition(InformationTextArea::BOTTOM);
        candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
      }
    }

    // Initialize candidate views if necessary.
    MaybeInitializeCandidateViews(new_candidate_window);

    should_show_at_composition_head_
        = new_candidate_window.show_window_at_composition();
    // Compute the index of the current page.
    const int current_page_index = ComputePageIndex(new_candidate_window);
    if (current_page_index < 0)
      return;

    // Update the candidates in the current page.
    const size_t start_from =
        current_page_index * new_candidate_window.page_size();

    int max_shortcut_width = 0;
    int max_candidate_width = 0;
    for (size_t i = 0; i < candidate_views_.size(); ++i) {
      const size_t index_in_page = i;
      const size_t candidate_index = start_from + index_in_page;
      CandidateView* candidate_view = candidate_views_[index_in_page].get();
      // Set the candidate text.
      if (candidate_index < new_candidate_window.candidates().size()) {
        const ui::CandidateWindow::Entry& entry =
            new_candidate_window.candidates()[candidate_index];
        candidate_view->SetEntry(entry);
        candidate_view->SetEnabled(true);
        candidate_view->SetInfolistIcon(!entry.description_title.empty());
      } else {
        // Disable the empty row.
        candidate_view->SetEntry(ui::CandidateWindow::Entry());
        candidate_view->SetEnabled(false);
        candidate_view->SetInfolistIcon(false);
      }
      if (new_candidate_window.orientation() == ui::CandidateWindow::VERTICAL) {
        int shortcut_width = 0;
        int candidate_width = 0;
        candidate_views_[i]->GetPreferredWidths(
            &shortcut_width, &candidate_width);
        max_shortcut_width = std::max(max_shortcut_width, shortcut_width);
        max_candidate_width = std::max(max_candidate_width, candidate_width);
      }
    }
    if (new_candidate_window.orientation() == ui::CandidateWindow::VERTICAL) {
      for (const auto& view : candidate_views_)
        view->SetWidths(max_shortcut_width, max_candidate_width);
    }

    std::unique_ptr<CandidateWindowBorder> border =
        std::make_unique<CandidateWindowBorder>();
    if (new_candidate_window.orientation() == ui::CandidateWindow::VERTICAL)
      border->set_offset(max_shortcut_width);
    else
      border->set_offset(0);
    GetBubbleFrameView()->SetBubbleBorder(std::move(border));
    GetBubbleFrameView()->OnThemeChanged();
  }
  // Update the current candidate window. We'll use candidate_window_ from here.
  // Note that SelectCandidateAt() uses candidate_window_.
  candidate_window_.CopyFrom(new_candidate_window);

  // Select the current candidate in the page.
  if (candidate_window_.is_cursor_visible()) {
    if (candidate_window_.page_size()) {
      const int current_candidate_in_page =
          candidate_window_.cursor_position() % candidate_window_.page_size();
      SelectCandidateAt(current_candidate_in_page);
    }
  } else {
    // Unselect the currently selected candidate.
    if (0 <= selected_candidate_index_in_page_ &&
        static_cast<size_t>(selected_candidate_index_in_page_) <
        candidate_views_.size()) {
      candidate_views_[selected_candidate_index_in_page_]->SetHighlighted(
          false);
      selected_candidate_index_in_page_ = -1;
    }
  }

  // Updates auxiliary text
  auxiliary_text_->SetVisible(candidate_window_.is_auxiliary_text_visible());
  auxiliary_text_->SetText(base::UTF8ToUTF16(
      candidate_window_.auxiliary_text()));
}

void CandidateWindowView::SetCursorBounds(const gfx::Rect& cursor_bounds,
                                          const gfx::Rect& composition_head) {
  if (candidate_window_.show_window_at_composition())
    SetAnchorRect(composition_head);
  else
    SetAnchorRect(cursor_bounds);
}

void CandidateWindowView::MaybeInitializeCandidateViews(
    const ui::CandidateWindow& candidate_window) {
  const ui::CandidateWindow::Orientation orientation =
      candidate_window.orientation();
  const size_t page_size = candidate_window.page_size();

  // Reset all candidate_views_ when orientation changes.
  if (orientation != candidate_window_.orientation())
    candidate_views_.clear();

  while (page_size < candidate_views_.size())
    candidate_views_.pop_back();

  while (page_size > candidate_views_.size()) {
    std::unique_ptr<CandidateView> new_candidate =
        std::make_unique<CandidateView>(this, orientation);
    candidate_area_->AddChildView(new_candidate.get());
    candidate_views_.push_back(std::move(new_candidate));
  }
}

void CandidateWindowView::SelectCandidateAt(int index_in_page) {
  const int current_page_index = ComputePageIndex(candidate_window_);
  if (current_page_index < 0) {
    return;
  }

  const int cursor_absolute_index =
      candidate_window_.page_size() * current_page_index + index_in_page;
  // Ignore click on out of range views.
  if (cursor_absolute_index < 0 ||
      candidate_window_.candidates().size() <=
      static_cast<size_t>(cursor_absolute_index)) {
    return;
  }

  // Remember the currently selected candidate index in the current page.
  selected_candidate_index_in_page_ = index_in_page;

  // Select the candidate specified by index_in_page.
  candidate_views_[index_in_page]->SetHighlighted(true);

  // Update the cursor indexes in the model.
  candidate_window_.set_cursor_position(cursor_absolute_index);
}

const char* CandidateWindowView::GetClassName() const {
  return "CandidateWindowView";
}

int CandidateWindowView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void CandidateWindowView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  for (size_t i = 0; i < candidate_views_.size(); ++i) {
    if (sender == candidate_views_[i].get()) {
      for (Observer& observer : observers_)
        observer.OnCandidateCommitted(i);
      return;
    }
  }
}

}  // namespace ime
}  // namespace ui
