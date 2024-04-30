// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import "ui/views/controls/scrollbar/cocoa_scroll_bar.h"

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "cc/paint/paint_shader.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"

namespace views {

namespace {

// The thickness of the normal, overlay, and expanded overlay scrollbars.
constexpr int kScrollbarThickness = 15;
constexpr int kOverlayScrollbarThickness = 12;
constexpr int kExpandedOverlayScrollbarThickness = 16;

// Opacity of the overlay scrollbar.
constexpr float kOverlayOpacity = 0.8f;

}  // namespace

//////////////////////////////////////////////////////////////////
// CocoaScrollBarThumb

class CocoaScrollBarThumb : public BaseScrollBarThumb {
 public:
  explicit CocoaScrollBarThumb(CocoaScrollBar* scroll_bar);

  CocoaScrollBarThumb(const CocoaScrollBarThumb&) = delete;
  CocoaScrollBarThumb& operator=(const CocoaScrollBarThumb&) = delete;

  ~CocoaScrollBarThumb() override;

  // Returns true if the thumb is in hovered state.
  bool IsStateHovered() const;

  // Returns true if the thumb is in pressed state.
  bool IsStatePressed() const;

  void UpdateIsMouseOverTrack(bool mouse_over_track);

 protected:
  // View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  // The CocoaScrollBar that owns us.
  raw_ptr<CocoaScrollBar> cocoa_scroll_bar_;  // weak.
};

CocoaScrollBarThumb::CocoaScrollBarThumb(CocoaScrollBar* scroll_bar)
    : BaseScrollBarThumb(scroll_bar), cocoa_scroll_bar_(scroll_bar) {
  DCHECK(scroll_bar);

  // This is necessary, otherwise the thumb will be rendered below the views if
  // those views paint to their own layers.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

CocoaScrollBarThumb::~CocoaScrollBarThumb() = default;

bool CocoaScrollBarThumb::IsStateHovered() const {
  return GetState() == Button::STATE_HOVERED;
}

bool CocoaScrollBarThumb::IsStatePressed() const {
  return GetState() == Button::STATE_PRESSED;
}

void CocoaScrollBarThumb::UpdateIsMouseOverTrack(bool mouse_over_track) {
  // The state should not change if the thumb is pressed. The thumb will be
  // set back to its hover or normal state when the mouse is released.
  if (IsStatePressed())
    return;

  SetState(mouse_over_track ? Button::STATE_HOVERED : Button::STATE_NORMAL);
}

gfx::Size CocoaScrollBarThumb::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  int thickness = cocoa_scroll_bar_->ScrollbarThickness();
  return gfx::Size(thickness, thickness);
}

void CocoaScrollBarThumb::OnPaint(gfx::Canvas* canvas) {
  auto params = cocoa_scroll_bar_->GetPainterParams();
  auto& scrollbar = absl::get<ui::NativeTheme::ScrollbarExtraParams>(params);
  // Set the hover state based only on the thumb.
  scrollbar.is_hovering = IsStateHovered() || IsStatePressed();
  ui::NativeTheme::Part thumb_part =
      scrollbar.orientation ==
              ui::NativeTheme::ScrollbarOrientation::kHorizontal
          ? ui::NativeTheme::kScrollbarHorizontalThumb
          : ui::NativeTheme::kScrollbarVerticalThumb;
  GetNativeTheme()->Paint(canvas->sk_canvas(), GetColorProvider(), thumb_part,
                          ui::NativeTheme::kNormal, GetLocalBounds(), params);
}

bool CocoaScrollBarThumb::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the mouse press if the scrollbar is hidden.
  if (cocoa_scroll_bar_->IsScrollbarFullyHidden())
    return false;

  return BaseScrollBarThumb::OnMousePressed(event);
}

void CocoaScrollBarThumb::OnMouseReleased(const ui::MouseEvent& event) {
  BaseScrollBarThumb::OnMouseReleased(event);
  scroll_bar()->OnMouseReleased(event);
}

void CocoaScrollBarThumb::OnMouseEntered(const ui::MouseEvent& event) {
  BaseScrollBarThumb::OnMouseEntered(event);
  scroll_bar()->OnMouseEntered(event);
}

void CocoaScrollBarThumb::OnMouseExited(const ui::MouseEvent& event) {
  // The thumb should remain pressed when dragged, even if the mouse leaves
  // the scrollview. The thumb will be set back to its hover or normal state
  // when the mouse is released.
  if (GetState() != Button::STATE_PRESSED)
    SetState(Button::STATE_NORMAL);
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar class

CocoaScrollBar::CocoaScrollBar(ScrollBar::Orientation orientation)
    : ScrollBar(orientation),
      hide_scrollbar_timer_(FROM_HERE,
                            base::Milliseconds(500),
                            base::BindRepeating(&CocoaScrollBar::HideScrollbar,
                                                base::Unretained(this))),
      thickness_animation_(this) {
  SetThumb(new CocoaScrollBarThumb(this));
  bridge_ = [[ViewsScrollbarBridge alloc] initWithDelegate:this];
  scroller_style_ = [ViewsScrollbarBridge preferredScrollerStyle];

  thickness_animation_.SetSlideDuration(base::Milliseconds(240));

  SetPaintToLayer();
  has_scrolltrack_ = scroller_style_ == NSScrollerStyleLegacy;
  layer()->SetOpacity(scroller_style_ == NSScrollerStyleOverlay ? 0.0f : 1.0f);
}

CocoaScrollBar::~CocoaScrollBar() {
  [bridge_ clearDelegate];
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, ScrollBar:

gfx::Rect CocoaScrollBar::GetTrackBounds() const {
  return GetLocalBounds();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, ScrollBar:

int CocoaScrollBar::GetThickness() const {
  return ScrollbarThickness();
}

bool CocoaScrollBar::OverlapsContent() const {
  return scroller_style_ == NSScrollerStyleOverlay;
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::View:

void CocoaScrollBar::Layout(PassKey) {
  // Set the thickness of the thumb according to the track bounds.
  // The length of the thumb is set by ScrollBar::Update().
  gfx::Rect thumb_bounds(GetThumb()->bounds());
  gfx::Rect track_bounds(GetTrackBounds());
  if (GetOrientation() == Orientation::kHorizontal) {
    GetThumb()->SetBounds(thumb_bounds.x(),
                          track_bounds.y(),
                          thumb_bounds.width(),
                          track_bounds.height());
  } else {
    GetThumb()->SetBounds(track_bounds.x(),
                          thumb_bounds.y(),
                          track_bounds.width(),
                          thumb_bounds.height());
  }
}

gfx::Size CocoaScrollBar::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  return gfx::Size();
}

void CocoaScrollBar::OnPaint(gfx::Canvas* canvas) {
  if (!has_scrolltrack_)
    return;
  auto params = GetPainterParams();
  auto& scrollbar = absl::get<ui::NativeTheme::ScrollbarExtraParams>(params);
  // Transparency of the track is handled by the View opacity, so always draw
  // using the non-overlay path.
  scrollbar.is_overlay = false;
  ui::NativeTheme::Part track_part =
      scrollbar.orientation ==
              ui::NativeTheme::ScrollbarOrientation::kHorizontal
          ? ui::NativeTheme::kScrollbarHorizontalTrack
          : ui::NativeTheme::kScrollbarVerticalTrack;
  GetNativeTheme()->Paint(canvas->sk_canvas(), GetColorProvider(), track_part,
                          ui::NativeTheme::kNormal, GetLocalBounds(), params);
}

bool CocoaScrollBar::GetCanProcessEventsWithinSubtree() const {
  // If using overlay scrollbars, do not process events when fully hidden.
  return scroller_style_ == NSScrollerStyleOverlay
             ? !IsScrollbarFullyHidden()
             : ScrollBar::GetCanProcessEventsWithinSubtree();
}

bool CocoaScrollBar::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the mouse press if the scrollbar is hidden.
  if (IsScrollbarFullyHidden())
    return false;

  return ScrollBar::OnMousePressed(event);
}

void CocoaScrollBar::OnMouseReleased(const ui::MouseEvent& event) {
  ResetOverlayScrollbar();
  ScrollBar::OnMouseReleased(event);
}

void CocoaScrollBar::OnMouseEntered(const ui::MouseEvent& event) {
  GetCocoaScrollBarThumb()->UpdateIsMouseOverTrack(true);

  if (scroller_style_ == NSScrollerStyleLegacy)
    return;

  // If the scrollbar thumb did not completely fade away, then reshow it when
  // the mouse enters the scrollbar thumb.
  if (!IsScrollbarFullyHidden())
    ShowScrollbar();

  // Expand the scrollbar. If the scrollbar is hidden, don't animate it.
  if (!is_expanded_) {
    SetScrolltrackVisible(true);
    is_expanded_ = true;
    if (IsScrollbarFullyHidden()) {
      thickness_animation_.Reset(1.0);
      UpdateScrollbarThickness();
    } else {
      thickness_animation_.Show();
    }
  }

  hide_scrollbar_timer_.Reset();
}

void CocoaScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  GetCocoaScrollBarThumb()->UpdateIsMouseOverTrack(false);
  ResetOverlayScrollbar();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::ScrollBar:

void CocoaScrollBar::Update(int viewport_size,
                            int content_size,
                            int contents_scroll_offset) {
  // TODO(tapted): Pass in overscroll amounts from the Layer and "Squish" the
  // scroller thumb accordingly.
  ScrollBar::Update(viewport_size, content_size, contents_scroll_offset);

  // Only reveal the scroller when |contents_scroll_offset| changes. Note this
  // is different to GetPosition() which can change due to layout. A layout
  // change can also change the offset; show the scroller in these cases. This
  // is consistent with WebContents (Cocoa will also show a scroller with any
  // mouse-initiated layout, but not programmatic size changes).
  if (contents_scroll_offset == last_contents_scroll_offset_)
    return;

  last_contents_scroll_offset_ = contents_scroll_offset;

  if (GetCocoaScrollBarThumb()->IsStatePressed())
    did_start_dragging_ = true;

  if (scroller_style_ == NSScrollerStyleOverlay) {
    ShowScrollbar();
    hide_scrollbar_timer_.Reset();
  }
}

void CocoaScrollBar::ObserveScrollEvent(const ui::ScrollEvent& event) {
  // Do nothing if the delayed hide timer is running. This means there has been
  // some recent scrolling in this direction already.
  if (scroller_style_ != NSScrollerStyleOverlay ||
      hide_scrollbar_timer_.IsRunning()) {
    return;
  }

  // Otherwise, when starting the event stream, show an overlay scrollbar to
  // indicate possible scroll directions, but do not start the hide timer.
  if (event.momentum_phase() == ui::EventMomentumPhase::MAY_BEGIN) {
    // Show only if the direction isn't yet known.
    if (event.x_offset() == 0 && event.y_offset() == 0)
      ShowScrollbar();
    return;
  }

  // If the direction matches, do nothing. This is needed in addition to the
  // hide timer check because Update() is called asynchronously, after event
  // processing. So when |event| is the first event in a particular direction
  // the hide timer will not have started.
  if ((GetOrientation() == Orientation::kHorizontal ? event.x_offset()
                                                    : event.y_offset()) != 0) {
    return;
  }

  // Otherwise, scrolling has started, but not in this scroller direction. If
  // already faded out, don't start another fade animation since that would
  // immediately finish the first fade animation.
  if (layer()->GetTargetOpacity() != 0) {
    // If canceling rather than picking a direction, fade out after a delay.
    if (event.momentum_phase() == ui::EventMomentumPhase::END)
      hide_scrollbar_timer_.Reset();
    else
      HideScrollbar();  // Fade out immediately.
  }
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::ViewsScrollbarBridge:

void CocoaScrollBar::OnScrollerStyleChanged() {
  NSScrollerStyle scroller_style =
      [ViewsScrollbarBridge preferredScrollerStyle];
  if (scroller_style_ == scroller_style)
    return;

  // Cancel all of the animations.
  thickness_animation_.Reset();
  layer()->GetAnimator()->AbortAllAnimations();

  scroller_style_ = scroller_style;

  // Ensure that the ScrollView updates the scrollbar's layout.
  if (parent())
    parent()->InvalidateLayout();

  if (scroller_style_ == NSScrollerStyleOverlay) {
    // Hide the scrollbar, but don't fade out.
    layer()->SetOpacity(0.0f);
    ResetOverlayScrollbar();
    GetThumb()->SchedulePaint();
  } else {
    is_expanded_ = false;
    SetScrolltrackVisible(true);
    ShowScrollbar();
  }
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::ImplicitAnimationObserver:

void CocoaScrollBar::OnImplicitAnimationsCompleted() {
  DCHECK_EQ(scroller_style_, NSScrollerStyleOverlay);
  ResetOverlayScrollbar();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::AnimationDelegate:

void CocoaScrollBar::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK(is_expanded_);
  UpdateScrollbarThickness();
}

void CocoaScrollBar::AnimationEnded(const gfx::Animation* animation) {
  // Remove the scrolltrack and set |is_expanded| to false at the end of
  // the shrink animation.
  if (!thickness_animation_.IsShowing()) {
    is_expanded_ = false;
    SetScrolltrackVisible(false);
  }
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, public:

int CocoaScrollBar::ScrollbarThickness() const {
  if (scroller_style_ == NSScrollerStyleLegacy)
    return kScrollbarThickness;

  return thickness_animation_.CurrentValueBetween(
      kOverlayScrollbarThickness, kExpandedOverlayScrollbarThickness);
}

bool CocoaScrollBar::IsScrollbarFullyHidden() const {
  return layer()->opacity() == 0.0f;
}

ui::NativeTheme::ExtraParams CocoaScrollBar::GetPainterParams() const {
  ui::NativeTheme::ScrollbarExtraParams scrollbar;
  if (GetOrientation() == Orientation::kHorizontal) {
    scrollbar.orientation = ui::NativeTheme::ScrollbarOrientation::kHorizontal;
  } else if (base::i18n::IsRTL()) {
    scrollbar.orientation =
        ui::NativeTheme::ScrollbarOrientation::kVerticalOnLeft;
  } else {
    scrollbar.orientation =
        ui::NativeTheme::ScrollbarOrientation::kVerticalOnRight;
  }
  scrollbar.is_overlay = GetScrollerStyle() == NSScrollerStyleOverlay;
  scrollbar.scale_from_dip = 1.0f;
  return ui::NativeTheme::ExtraParams(scrollbar);
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, private:

void CocoaScrollBar::HideScrollbar() {
  DCHECK_EQ(scroller_style_, NSScrollerStyleOverlay);

  // Don't disappear if the scrollbar is hovered, or pressed but not dragged.
  // This behavior matches the Cocoa scrollbars, but differs from the Blink
  // scrollbars which would just disappear.
  CocoaScrollBarThumb* thumb = GetCocoaScrollBarThumb();
  if (IsMouseHovered() || thumb->IsStateHovered() ||
      (thumb->IsStatePressed() && !did_start_dragging_)) {
    hide_scrollbar_timer_.Reset();
    return;
  }

  did_start_dragging_ = false;

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(base::Milliseconds(240));
  animation.AddObserver(this);
  layer()->SetOpacity(0.0f);
}

void CocoaScrollBar::ShowScrollbar() {
  // If the scrollbar is still expanded but has not completely faded away,
  // then shrink it back to its original state.
  if (is_expanded_ && !IsHoverOrPressedState() &&
      layer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::OPACITY)) {
    DCHECK_EQ(scroller_style_, NSScrollerStyleOverlay);
    thickness_animation_.Hide();
  }

  // Updates the scrolltrack and repaint it, if necessary.
  double opacity =
      scroller_style_ == NSScrollerStyleOverlay ? kOverlayOpacity : 1.0f;
  layer()->SetOpacity(opacity);
  hide_scrollbar_timer_.Stop();
}

bool CocoaScrollBar::IsHoverOrPressedState() const {
  CocoaScrollBarThumb* thumb = GetCocoaScrollBarThumb();
  return thumb->IsStateHovered() ||
         thumb->IsStatePressed() ||
         IsMouseHovered();
}

void CocoaScrollBar::UpdateScrollbarThickness() {
  int thickness = ScrollbarThickness();
  if (GetOrientation() == Orientation::kHorizontal) {
    SetBounds(x(), bounds().bottom() - thickness, width(), thickness);
  } else {
    SetBounds(bounds().right() - thickness, y(), thickness, height());
  }
}

void CocoaScrollBar::ResetOverlayScrollbar() {
  if (!IsHoverOrPressedState() && IsScrollbarFullyHidden() &&
      !thickness_animation_.IsClosing()) {
    if (is_expanded_) {
      is_expanded_ = false;
      thickness_animation_.Reset();
      UpdateScrollbarThickness();
    }
    SetScrolltrackVisible(false);
  }
}

void CocoaScrollBar::SetScrolltrackVisible(bool visible) {
  has_scrolltrack_ = visible;
  SchedulePaint();
}

CocoaScrollBarThumb* CocoaScrollBar::GetCocoaScrollBarThumb() const {
  return static_cast<CocoaScrollBarThumb*>(GetThumb());
}

// static
base::RetainingOneShotTimer* ScrollBar::GetHideTimerForTesting(
    ScrollBar* scroll_bar) {
  return &static_cast<CocoaScrollBar*>(scroll_bar)->hide_scrollbar_timer_;
}

BEGIN_METADATA(CocoaScrollBar)
END_METADATA

}  // namespace views
