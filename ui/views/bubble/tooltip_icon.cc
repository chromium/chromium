// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/tooltip_icon.h"

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/info_bubble.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/style/platform_style.h"

namespace views {

TooltipIcon::TooltipIcon(const std::u16string& tooltip, int tooltip_icon_size)
    : tooltip_(tooltip),
      tooltip_icon_size_(tooltip_icon_size),

      bubble_(nullptr) {
  SetFocusBehavior(PlatformStyle::kDefaultFocusBehavior);
  set_suppress_default_focus_handling();
  FocusRing::Install(this);
  SetBorder(CreateEmptyBorder(
      LayoutProvider::Get()->GetInsetsMetric(INSETS_VECTOR_IMAGE_BUTTON)));
  InstallCircleHighlightPathGenerator(this);

  // The tooltip icon, despite visually being an icon with no text, actually
  // opens a bubble whenever the user mouses over it or focuses it, so it's
  // essentially a text control that hides itself when not in view without
  // altering the bubble's layout when shown. As such, have it behave like
  // static text for screenreader users, since that's the role it serves here
  // anyway.
  GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  GetViewAccessibility().SetName(tooltip_);
}

TooltipIcon::~TooltipIcon() {
  observers_.Notify(&Observer::OnTooltipIconDestroying, this);
  HideBubble();
}

void TooltipIcon::SetBubbleWidth(int preferred_width) {
  preferred_width_ = preferred_width;
  OnPropertyChanged(&preferred_width_, kPropertyEffectsPreferredSizeChanged);
}

int TooltipIcon::GetBubbleWidth() const {
  return preferred_width_;
}

void TooltipIcon::SetAnchorPointArrow(BubbleBorder::Arrow arrow) {
  anchor_point_arrow_ = arrow;
  OnPropertyChanged(&anchor_point_arrow_, kPropertyEffectsPaint);
}

BubbleBorder::Arrow TooltipIcon::GetAnchorPointArrow() const {
  return anchor_point_arrow_;
}

void TooltipIcon::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_inside_ = true;
  show_timer_.Start(FROM_HERE, base::Milliseconds(150), this,
                    &TooltipIcon::ShowBubble);
}

void TooltipIcon::OnMouseExited(const ui::MouseEvent& event) {
  show_timer_.Stop();
}

bool TooltipIcon::OnMousePressed(const ui::MouseEvent& event) {
  // Swallow the click so that the parent doesn't process it.
  return true;
}

void TooltipIcon::OnFocus() {
  ShowBubble();
#if BUILDFLAG(IS_WIN)
  // Tooltip text does not announce on Windows; crbug.com/1245470
  NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
#endif
}

void TooltipIcon::OnBlur() {
  HideBubble();
}

void TooltipIcon::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    ShowBubble();
    event->SetHandled();
  }
}

void TooltipIcon::OnThemeChanged() {
  ImageView::OnThemeChanged();
  SetDrawAsHovered(false);
}

void TooltipIcon::MouseMovedOutOfHost() {
  if (IsMouseHovered()) {
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
    return;
  }

  mouse_inside_ = false;
  HideBubble();
}

void TooltipIcon::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TooltipIcon::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TooltipIcon::SetDrawAsHovered(bool hovered) {
  SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kInfoOutlineIcon,
      GetColorProvider()->GetColor(hovered ? ui::kColorHelpIconActive
                                           : ui::kColorHelpIconInactive),
      tooltip_icon_size_));
}

void TooltipIcon::ShowBubble() {
  if (bubble_)
    return;

  SetDrawAsHovered(true);

  bubble_ = new InfoBubble(this, anchor_point_arrow_, tooltip_);
  bubble_->set_preferred_width(preferred_width_);
  // When shown due to a gesture event, close on deactivate (i.e. don't use
  // "focusless").
  bubble_->SetCanActivate(!mouse_inside_);

  bubble_->Show();
  observation_.Observe(bubble_->GetWidget());

  if (mouse_inside_) {
    View* frame = bubble_->GetWidget()->non_client_view()->frame_view();
    mouse_watcher_ = std::make_unique<MouseWatcher>(
        std::make_unique<MouseWatcherViewHost>(frame, gfx::Insets()), this);
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }

  observers_.Notify(&Observer::OnTooltipBubbleShown, this);
}

void TooltipIcon::HideBubble() {
  if (bubble_)
    bubble_->Hide();
}

void TooltipIcon::OnWidgetDestroyed(Widget* widget) {
  DCHECK(observation_.IsObservingSource(widget));
  observation_.Reset();

  SetDrawAsHovered(false);
  mouse_watcher_.reset();
  bubble_ = nullptr;
}

BEGIN_METADATA(TooltipIcon)
ADD_PROPERTY_METADATA(int, BubbleWidth)
ADD_PROPERTY_METADATA(BubbleBorder::Arrow, AnchorPointArrow)
END_METADATA

}  // namespace views
