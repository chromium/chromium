// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/info_bubble.h"

#include <memory>
#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
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

  InfoBubbleFrame(const InfoBubbleFrame&) = delete;
  InfoBubbleFrame& operator=(const InfoBubbleFrame&) = delete;

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
};

InfoBubble::InfoBubble(View* anchor,
                       BubbleBorder::Arrow arrow,
                       const std::u16string& message)
    : BubbleDialogDelegateView(anchor,
                               arrow,
                               views::BubbleBorder::DIALOG_SHADOW,
                               true) {
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  set_margins(LayoutProvider::Get()->GetInsetsMetric(
      InsetsMetric::INSETS_TOOLTIP_BUBBLE));
  SetCanActivate(false);
  SetAccessibleWindowRole(ax::mojom::Role::kAlertDialog);
  // TODO(pbos): This hacks around a bug where focus order in the parent dialog
  // breaks because it tries to focus InfoBubble without anything focusable in
  // it. FocusSearch should handle this case and this should be removable.
  set_focus_traversable_from_anchor_view(false);

  SetLayoutManager(std::make_unique<FillLayout>());
  label_ = AddChildView(std::make_unique<Label>(message));
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
}

InfoBubble::~InfoBubble() = default;

void InfoBubble::Show() {
  BubbleDialogDelegateView::CreateBubble(this);

  UpdatePosition();
}

void InfoBubble::Hide() {
  Widget* widget = GetWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
}

std::unique_ptr<NonClientFrameView> InfoBubble::CreateNonClientFrameView(
    Widget* widget) {
  DCHECK(!frame_);
  auto frame = std::make_unique<InfoBubbleFrame>(margins());
  frame->set_available_bounds(anchor_widget()->GetWindowBoundsInScreen());
  auto border = std::make_unique<BubbleBorder>(arrow(), GetShadow());
  border->SetColor(color());
  frame->SetBubbleBorder(std::move(border));
  frame_ = frame.get();
  return frame;
}

gfx::Size InfoBubble::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  if (preferred_width_ == 0)
    return BubbleDialogDelegateView::CalculatePreferredSize(available_size);

  int pref_width = preferred_width_;
  pref_width -= frame_->GetInsets().width();
  pref_width -= 2 * kBubbleBorderVisibleWidth;
  return gfx::Size(pref_width, GetLayoutManager()->GetPreferredHeightForWidth(
                                   this, pref_width));
}

void InfoBubble::OnWidgetBoundsChanged(Widget* widget,
                                       const gfx::Rect& new_bounds) {
  BubbleDialogDelegateView::OnWidgetBoundsChanged(widget, new_bounds);
  if (anchor_widget() == widget)
    frame_->set_available_bounds(widget->GetWindowBoundsInScreen());
}

void InfoBubble::UpdatePosition() {
  Widget* const widget = GetWidget();
  if (!widget)
    return;

  if (anchor_widget()->IsVisible() &&
      !GetAnchorView()->GetVisibleBounds().IsEmpty()) {
    widget->SetVisibilityChangedAnimationsEnabled(true);
    widget->ShowInactive();
  } else {
    widget->SetVisibilityChangedAnimationsEnabled(false);
    widget->Hide();
  }
}

BEGIN_METADATA(InfoBubble)
END_METADATA

}  // namespace views
