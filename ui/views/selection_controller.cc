// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/selection_controller.h"

#include <algorithm>
#include <vector>

#include "build/build_config.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/event.h"
#include "ui/gfx/render_text.h"
#include "ui/views/metrics.h"
#include "ui/views/selection_controller_delegate.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"

namespace views {

SelectionController::SelectionController(SelectionControllerDelegate* delegate)
    : delegate_(delegate) {
  // If selection clipboard is used, update it on a text selection.
  if (ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    set_handles_selection_clipboard(true);
  }

  DCHECK(delegate);
}

bool SelectionController::OnMousePressed(
    const ui::MouseEvent& event,
    bool handled,
    InitialFocusStateOnMousePress initial_focus_state) {
  gfx::RenderText* render_text = GetRenderText();
  DCHECK(render_text);

  TrackMouseClicks(event);
  if (handled)
    return true;

  if (event.IsOnlyLeftMouseButton()) {
    first_drag_location_ = event.location();
    if (delegate_->SupportsDrag())
      delegate_->SetTextBeingDragged(false);

    switch (aggregated_clicks_) {
      case 0:
        // If the click location is within an existing selection, it may be a
        // potential drag and drop.
        if (delegate_->SupportsDrag() &&
            render_text->IsPointInSelection(event.location())) {
          delegate_->SetTextBeingDragged(true);
        } else {
          delegate_->OnBeforePointerAction();
          const bool selection_changed = render_text->MoveCursorToPoint(
              event.location(), event.IsShiftDown());
          delegate_->OnAfterPointerAction(false, selection_changed);
        }
        break;
      case 1:
        // Select the word at the click location on a double click.
        SelectWord(event.location());
        double_click_word_ = render_text->selection();
        break;
      case 2:
        // Select all the text on a triple click.
        SelectAll();
        break;
      default:
        NOTREACHED();
    }
  }

  if (event.IsOnlyRightMouseButton()) {
    if (PlatformStyle::kSelectAllOnRightClickWhenUnfocused &&
        initial_focus_state == InitialFocusStateOnMousePress::kUnFocused) {
      SelectAll();
    } else if (PlatformStyle::kSelectWordOnRightClick &&
               !render_text->IsPointInSelection(event.location()) &&
               IsInsideText(event.location())) {
      SelectWord(event.location());
    }
  }

  if (handles_selection_clipboard_ && event.IsOnlyMiddleMouseButton() &&
      !delegate_->IsReadOnly()) {
    delegate_->OnBeforePointerAction();
    const bool selection_changed =
        render_text->MoveCursorToPoint(event.location(), false);
    const bool text_changed = delegate_->PasteSelectionClipboard();
    delegate_->OnAfterPointerAction(text_changed,
                                    selection_changed | text_changed);
  }

  return true;
}

bool SelectionController::OnMouseDragged(const ui::MouseEvent& event) {
  DCHECK(GetRenderText());
  // If |drag_selection_timer_| is running, |last_drag_location_| will be used
  // to update the selection.
  last_drag_location_ = event.location();

  // Don't adjust the cursor on a potential drag and drop.
  if (delegate_->HasTextBeingDragged() || !event.IsOnlyLeftMouseButton())
    return true;

  // A timer is used to continuously scroll while selecting beyond side edges.
  const int x = event.location().x();
  const int width = delegate_->GetViewWidth();
  const int drag_selection_delay = delegate_->GetDragSelectionDelay();
  if ((x >= 0 && x <= width) || drag_selection_delay == 0) {
    drag_selection_timer_.Stop();
    SelectThroughLastDragLocation();
  } else if (!drag_selection_timer_.IsRunning()) {
    // Select through the edge of the visible text, then start the scroll timer.
    last_drag_location_.set_x(std::clamp(x, 0, width));
    SelectThroughLastDragLocation();

    drag_selection_timer_.Start(
        FROM_HERE, base::Milliseconds(drag_selection_delay), this,
        &SelectionController::SelectThroughLastDragLocation);
  }

  return true;
}

void SelectionController::OnMouseReleased(const ui::MouseEvent& event) {
  gfx::RenderText* render_text = GetRenderText();
  DCHECK(render_text);

  drag_selection_timer_.Stop();

  // Cancel suspected drag initiations, the user was clicking in the selection.
  if (delegate_->HasTextBeingDragged()) {
    delegate_->OnBeforePointerAction();
    const bool selection_changed =
        render_text->MoveCursorToPoint(event.location(), false);
    delegate_->OnAfterPointerAction(false, selection_changed);
  }

  if (delegate_->SupportsDrag())
    delegate_->SetTextBeingDragged(false);

  if (handles_selection_clipboard_ && !render_text->selection().is_empty())
    delegate_->UpdateSelectionClipboard();
}

void SelectionController::OnMouseCaptureLost() {
  gfx::RenderText* render_text = GetRenderText();
  DCHECK(render_text);

  drag_selection_timer_.Stop();

  if (handles_selection_clipboard_ && !render_text->selection().is_empty())
    delegate_->UpdateSelectionClipboard();
}

void SelectionController::OffsetDoubleClickWord(size_t offset) {
  double_click_word_.set_start(double_click_word_.start() + offset);
  double_click_word_.set_end(double_click_word_.end() + offset);
}

void SelectionController::TrackMouseClicks(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    base::TimeDelta time_delta = event.time_stamp() - last_click_time_;
    if (!last_click_time_.is_null() &&
        time_delta.InMilliseconds() <= GetDoubleClickInterval() &&
        !View::ExceededDragThreshold(event.root_location() -
                                     last_click_root_location_)) {
      // Upon clicking after a triple click, the count should go back to
      // double click and alternate between double and triple. This assignment
      // maps 0 to 1, 1 to 2, 2 to 1.
      aggregated_clicks_ = (aggregated_clicks_ % 2) + 1;
    } else {
      aggregated_clicks_ = 0;
    }
    last_click_time_ = event.time_stamp();
    last_click_root_location_ = event.root_location();
  }
}

void SelectionController::SelectWord(const gfx::Point& point) {
  gfx::RenderText* render_text = GetRenderText();
  DCHECK(render_text);
  delegate_->OnBeforePointerAction();
  render_text->MoveCursorToPoint(point, false);
  render_text->SelectWord();
  delegate_->OnAfterPointerAction(false, true);
}

void SelectionController::SelectAll() {
  gfx::RenderText* render_text = GetRenderText();
  DCHECK(render_text);
  delegate_->OnBeforePointerAction();
  render_text->SelectAll(false);
  delegate_->OnAfterPointerAction(false, true);
}

gfx::RenderText* SelectionController::GetRenderText() {
  return delegate_->GetRenderTextForSelectionController();
}

void SelectionController::SelectThroughLastDragLocation() {
  gfx::RenderText* render_text = GetRenderText();
  DCHECK(render_text);

  delegate_->OnBeforePointerAction();

  // Note that |first_drag_location_| is only used when
  // RenderText::kDragToEndIfOutsideVerticalBounds, which is platform-specific.
  render_text->MoveCursorToPoint(last_drag_location_, true,
                                 first_drag_location_);

  if (aggregated_clicks_ == 1) {
    render_text->SelectWord();
    // Expand the selection so the initially selected word remains selected.
    gfx::Range selection = render_text->selection();
    const size_t min =
        std::min(selection.GetMin(), double_click_word_.GetMin());
    const size_t max =
        std::max(selection.GetMax(), double_click_word_.GetMax());
    const bool reversed = selection.is_reversed();
    selection.set_start(reversed ? max : min);
    selection.set_end(reversed ? min : max);
    render_text->SelectRange(selection);
  }
  delegate_->OnAfterPointerAction(false, true);
}

bool SelectionController::IsInsideText(const gfx::Point& point) {
  gfx::RenderText* render_text = GetRenderText();
  std::vector<gfx::Rect> bounds_rects = render_text->GetSubstringBounds(
      gfx::Range(0, render_text->text().length()));

  for (const auto& bounds : bounds_rects)
    if (bounds.Contains(point))
      return true;

  return false;
}

}  // namespace views
