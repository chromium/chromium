// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/info_bubble.h"

#include <memory>

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// The visible width of bubble borders (differs from the actual width) in px.
constexpr int kBubbleBorderVisibleWidth = 1;

}  // namespace

class InfoBubbleFrame : public BubbleFrameView {
 public:
  explicit InfoBubbleFrame(const gfx::Insets& content_margins)
      : BubbleFrameView(gfx::Insets(), content_margins) {}
  ~InfoBubbleFrame() override = default;

  gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const override {
    return available_bounds_;
  }

  void set_available_bounds(const gfx::Rect& available_bounds) {
    available_bounds_ = available_bounds;
  }

 private:
  // Bounds that this frame should try to keep bubbles within (screen coords).
  gfx::Rect available_bounds_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleFrame);
};

InfoBubble::InfoBubble(View* anchor, const base::string16& message)
    : anchor_(anchor), frame_(nullptr), preferred_width_(0) {
  DCHECK(anchor_);
  SetAnchorView(anchor_);

  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  set_margins(LayoutProvider::Get()->GetInsetsMetric(
      InsetsMetric::INSETS_TOOLTIP_BUBBLE));
  SetCanActivate(false);

  SetLayoutManager(std::make_unique<FillLayout>());
  Label* label = new Label(message);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddChildView(label);
}

InfoBubble::~InfoBubble() = default;

void InfoBubble::Show() {
  widget_ = BubbleDialogDelegateView::CreateBubble(this);

  UpdatePosition();
}

void InfoBubble::Hide() {
  Widget* widget = GetWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
}

NonClientFrameView* InfoBubble::CreateNonClientFrameView(Widget* widget) {
  DCHECK(!frame_);
  frame_ = new InfoBubbleFrame(margins());
  frame_->set_available_bounds(anchor_widget()->GetWindowBoundsInScreen());
  frame_->SetBubbleBorder(
      std::make_unique<BubbleBorder>(arrow(), GetShadow(), color()));
  return frame_;
}

gfx::Size InfoBubble::CalculatePreferredSize() const {
  if (preferred_width_ == 0)
    return BubbleDialogDelegateView::CalculatePreferredSize();

  int pref_width = preferred_width_;
  pref_width -= frame_->GetInsets().width();
  pref_width -= 2 * kBubbleBorderVisibleWidth;
  return gfx::Size(pref_width, GetHeightForWidth(pref_width));
}

void InfoBubble::OnWidgetDestroyed(Widget* widget) {
  if (widget == widget_)
    widget_ = nullptr;
}

void InfoBubble::OnWidgetBoundsChanged(Widget* widget,
                                       const gfx::Rect& new_bounds) {
  BubbleDialogDelegateView::OnWidgetBoundsChanged(widget, new_bounds);
  if (anchor_widget() == widget)
    frame_->set_available_bounds(widget->GetWindowBoundsInScreen());
}

void InfoBubble::UpdatePosition() {
  if (!widget_)
    return;

  if (!anchor_->GetVisibleBounds().IsEmpty()) {
    SizeToContents();
    widget_->SetVisibilityChangedAnimationsEnabled(true);
    widget_->ShowInactive();
  } else {
    widget_->SetVisibilityChangedAnimationsEnabled(false);
    widget_->Hide();
  }
}

BEGIN_METADATA(InfoBubble)
METADATA_PARENT_CLASS(BubbleDialogDelegateView)
END_METADATA()

}  // namespace views
