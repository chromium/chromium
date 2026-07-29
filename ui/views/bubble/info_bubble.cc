// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/info_bubble.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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

InfoBubble::InfoBubble(View* anchor,
                       BubbleBorder::Arrow arrow,
                       const std::u16string& message)
    : BubbleDialogDelegateView(anchor,
                               arrow,
                               views::BubbleBorder::DIALOG_SHADOW,
                               true) {
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  set_available_screen_bounds_callback(base::BindRepeating(
      [](const InfoBubble* bubble, const gfx::Rect& rect) {
        // Anchor widget can be null during destruction or if the anchor is
        // cleared.
        return bubble->anchor_widget()
                   ? bubble->anchor_widget()->GetWindowBoundsInScreen()
                   : gfx::Rect();
      },
      base::Unretained(this)));

  auto* layout_provider = LayoutProvider::Get();
  set_frame_margins({
      .contents =
          layout_provider->GetInsetsMetric(InsetsMetric::INSETS_TOOLTIP_BUBBLE),
      .title = gfx::Insets(),
      .footnote =
          layout_provider->GetInsetsMetric(InsetsMetric::INSETS_TOOLTIP_BUBBLE),
  });
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
  if (widget && !widget->IsClosed()) {
    widget->Close();
  }
}

std::unique_ptr<FrameView> InfoBubble::CreateFrameView(Widget* widget) {
  auto frame = BubbleDialogDelegateView::CreateFrameView(widget);
  static_cast<BubbleFrameView*>(frame.get())->SetContentMargins(margins());
  return frame;
}

gfx::Size InfoBubble::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  if (preferred_width_ == 0) {
    return BubbleDialogDelegateView::CalculatePreferredSize(available_size);
  }

  int pref_width = preferred_width_;
  pref_width -= GetBubbleFrameView()->GetInsets().width();
  pref_width -= 2 * kBubbleBorderVisibleWidth;
  return gfx::Size(pref_width, GetLayoutManager()->GetPreferredHeightForWidth(
                                   this, pref_width));
}

void InfoBubble::UpdatePosition() {
  Widget* const widget = GetWidget();
  if (!widget) {
    return;
  }

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
