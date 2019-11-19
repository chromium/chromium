// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/chromeos/ime/candidate_view.h"
#include "ui/chromeos/ime/candidate_window_constants.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace ime {

namespace {

// VerticalCandidateLabel is used for rendering candidate text in
// the vertical candidate window.
class VerticalCandidateLabel : public views::Label {
 public:
  VerticalCandidateLabel() {}

 private:
  ~VerticalCandidateLabel() override {}

  // views::Label:
  // Returns the preferred size, but guarantees that the width has at
  // least kMinCandidateLabelWidth pixels.
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = Label::CalculatePreferredSize();
    size.SetToMax(gfx::Size(kMinCandidateLabelWidth, 0));
    size.SetToMin(gfx::Size(kMaxCandidateLabelWidth, size.height()));
    return size;
  }

  const char* GetClassName() const override { return "VerticalCandidateLabel"; }

  DISALLOW_COPY_AND_ASSIGN(VerticalCandidateLabel);
};

// Creates the shortcut label, and returns it (never returns NULL).
// The label text is not set in this function.
views::Label* CreateShortcutLabel(
    ui::CandidateWindow::Orientation orientation,
    const ui::NativeTheme& theme) {
  // Create the shortcut label. The label will be owned by
  // |wrapped_shortcut_label|, hence it's deleted when
  // |wrapped_shortcut_label| is deleted.
  views::Label* shortcut_label = new views::Label;

  // TODO(tapted): Get this FontList from views::style.
  if (orientation == ui::CandidateWindow::VERTICAL) {
    shortcut_label->SetFontList(shortcut_label->font_list().Derive(
        kFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
  } else {
    shortcut_label->SetFontList(
        shortcut_label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  }
  // TODO(satorux): Maybe we need to use language specific fonts for
  // candidate_label, like Chinese font for Chinese input method?

  // Setup paddings.
  const gfx::Insets kVerticalShortcutLabelInsets(1, 6, 1, 6);
  const gfx::Insets kHorizontalShortcutLabelInsets(1, 3, 1, 0);
  const gfx::Insets insets =
      (orientation == ui::CandidateWindow::VERTICAL ?
       kVerticalShortcutLabelInsets :
       kHorizontalShortcutLabelInsets);
  shortcut_label->SetBorder(views::CreateEmptyBorder(
      insets.top(), insets.left(), insets.bottom(), insets.right()));

  // Add decoration based on the orientation.
  if (orientation == ui::CandidateWindow::VERTICAL) {
    // Set the background color.
    SkColor blackish = color_utils::AlphaBlend(
        SK_ColorBLACK,
        theme.GetSystemColor(ui::NativeTheme::kColorId_WindowBackground),
        0.25f);
    shortcut_label->SetBackground(
        views::CreateSolidBackground(SkColorSetA(blackish, 0xE0)));
  }
  shortcut_label->SetElideBehavior(gfx::NO_ELIDE);

  return shortcut_label;
}

// Creates the candidate label, and returns it (never returns NULL).
// The label text is not set in this function.
views::Label* CreateCandidateLabel(
    ui::CandidateWindow::Orientation orientation) {
  views::Label* candidate_label = NULL;

  // Create the candidate label. The label will be added to |this| as a
  // child view, hence it's deleted when |this| is deleted.
  if (orientation == ui::CandidateWindow::VERTICAL) {
    candidate_label = new VerticalCandidateLabel;
  } else {
    candidate_label = new views::Label;
  }

  // Change the font size.
  candidate_label->SetFontList(
      candidate_label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  candidate_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  candidate_label->SetElideBehavior(gfx::NO_ELIDE);

  return candidate_label;
}

// Creates the annotation label, and return it (never returns NULL).
// The label text is not set in this function.
views::Label* CreateAnnotationLabel(
    ui::CandidateWindow::Orientation orientation,
    const ui::NativeTheme& theme) {
  // Create the annotation label.
  views::Label* annotation_label = new views::Label;

  // Change the font size and color.
  annotation_label->SetFontList(
      annotation_label->font_list().DeriveWithSizeDelta(kFontSizeDelta));
  annotation_label->SetEnabledColor(theme.GetSystemColor(
      ui::NativeTheme::kColorId_LabelDisabledColor));
  annotation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  annotation_label->SetElideBehavior(gfx::NO_ELIDE);

  return annotation_label;
}

}  // namespace

CandidateView::CandidateView(views::ButtonListener* listener,
                             ui::CandidateWindow::Orientation orientation)
    : views::Button(listener),
      orientation_(orientation),
      shortcut_label_(NULL),
      candidate_label_(NULL),
      annotation_label_(NULL),
      infolist_icon_(NULL),
      shortcut_width_(0),
      candidate_width_(0),
      highlighted_(false) {
  SetBorder(views::CreateEmptyBorder(1, 1, 1, 1));

  const ui::NativeTheme& theme = *GetNativeTheme();
  shortcut_label_ = CreateShortcutLabel(orientation, theme);
  candidate_label_ = CreateCandidateLabel(orientation);
  annotation_label_ = CreateAnnotationLabel(orientation, theme);

  AddChildView(shortcut_label_);
  AddChildView(candidate_label_);
  AddChildView(annotation_label_);

  if (orientation == ui::CandidateWindow::VERTICAL) {
    infolist_icon_ = new views::View;
    infolist_icon_->SetBackground(views::CreateSolidBackground(
        theme.GetSystemColor(ui::NativeTheme::kColorId_FocusedBorderColor)));
    AddChildView(infolist_icon_);
  }
}

void CandidateView::GetPreferredWidths(int* shortcut_width,
                                       int* candidate_width) {
  *shortcut_width = shortcut_label_->GetPreferredSize().width();
  *candidate_width = candidate_label_->GetPreferredSize().width();
}

void CandidateView::SetWidths(int shortcut_width, int candidate_width) {
  shortcut_width_ = shortcut_width;
  shortcut_label_->SetVisible(shortcut_width_ != 0);
  candidate_width_ = candidate_width;
}

void CandidateView::SetEntry(const ui::CandidateWindow::Entry& entry) {
  base::string16 label = entry.label;
  if (!label.empty() && orientation_ != ui::CandidateWindow::VERTICAL)
    label += base::ASCIIToUTF16(".");
  shortcut_label_->SetText(label);
  candidate_label_->SetText(entry.value);
  annotation_label_->SetText(entry.annotation);
}

void CandidateView::SetInfolistIcon(bool enable) {
  if (infolist_icon_)
    infolist_icon_->SetVisible(enable);
  SchedulePaint();
}

void CandidateView::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted)
    return;

  highlighted_ = highlighted;
  if (highlighted) {
    ui::NativeTheme* theme = GetNativeTheme();
    SetBackground(views::CreateSolidBackground(theme->GetSystemColor(
        ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused)));
    SetBorder(views::CreateSolidBorder(
        1,
        theme->GetSystemColor(ui::NativeTheme::kColorId_FocusedBorderColor)));

    // Cancel currently focused one.
    for (View* view : parent()->children()) {
      if (view != this)
        static_cast<CandidateView*>(view)->SetHighlighted(false);
    }
  } else {
    SetBackground(nullptr);
    SetBorder(views::CreateEmptyBorder(1, 1, 1, 1));
  }
  SchedulePaint();
}

void CandidateView::StateChanged(ButtonState old_state) {
  int text_style = state() == STATE_DISABLED ? views::style::STYLE_DISABLED
                                             : views::style::STYLE_PRIMARY;
  shortcut_label_->SetEnabledColor(views::style::GetColor(
      *shortcut_label_, views::style::CONTEXT_LABEL, text_style));
  if (state() == STATE_PRESSED)
    SetHighlighted(true);
}

const char* CandidateView::GetClassName() const {
  return "CandidateView";
}

bool CandidateView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location())) {
    // Moves the drag target to the sibling view.
    gfx::Point location_in_widget(event.location());
    ConvertPointToWidget(this, &location_in_widget);
    for (View* view : parent()->children()) {
      if (view == this)
        continue;
      gfx::Point location_in_sibling(location_in_widget);
      ConvertPointFromWidget(view, &location_in_sibling);
      if (view->HitTestPoint(location_in_sibling)) {
        GetWidget()->GetRootView()->SetMouseHandler(view);
        auto* sibling = static_cast<CandidateView*>(view);
        sibling->SetHighlighted(true);
        return view->OnMouseDragged(ui::MouseEvent(event, this, sibling));
      }
    }

    return false;
  }

  return views::Button::OnMouseDragged(event);
}

void CandidateView::Layout() {
  const int padding_width =
      orientation_ == ui::CandidateWindow::VERTICAL ? 4 : 6;
  int x = 0;
  shortcut_label_->SetBounds(x, 0, shortcut_width_, height());
  if (shortcut_width_ > 0)
    x += shortcut_width_ + padding_width;
  candidate_label_->SetBounds(x, 0, candidate_width_, height());
  x += candidate_width_ + padding_width;

  int right = bounds().right();
  if (infolist_icon_ && infolist_icon_->GetVisible()) {
    infolist_icon_->SetBounds(
        right - kInfolistIndicatorIconWidth - kInfolistIndicatorIconPadding,
        kInfolistIndicatorIconPadding,
        kInfolistIndicatorIconWidth,
        height() - kInfolistIndicatorIconPadding * 2);
    right -= kInfolistIndicatorIconWidth + kInfolistIndicatorIconPadding * 2;
  }
  annotation_label_->SetBounds(x, 0, right - x, height());
}

gfx::Size CandidateView::CalculatePreferredSize() const {
  const int padding_width =
      orientation_ == ui::CandidateWindow::VERTICAL ? 4 : 6;
  gfx::Size size;
  if (shortcut_label_->GetVisible()) {
    size = shortcut_label_->GetPreferredSize();
    size.SetToMax(gfx::Size(shortcut_width_, 0));
    size.Enlarge(padding_width, 0);
  }
  gfx::Size candidate_size = candidate_label_->GetPreferredSize();
  candidate_size.SetToMax(gfx::Size(candidate_width_, 0));
  size.Enlarge(candidate_size.width() + padding_width, 0);
  size.SetToMax(candidate_size);
  if (annotation_label_->GetVisible()) {
    gfx::Size annotation_size = annotation_label_->GetPreferredSize();
    size.Enlarge(annotation_size.width() + padding_width, 0);
    size.SetToMax(annotation_size);
  }

  // Reserves the margin for infolist_icon even if it's not visible.
  size.Enlarge(
      kInfolistIndicatorIconWidth + kInfolistIndicatorIconPadding * 2, 0);
  return size;
}

}  // namespace ime
}  // namespace ui
