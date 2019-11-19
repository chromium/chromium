// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scroll_view.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/overscroll/scroll_input_handler.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class ScrollCornerView : public View {
 public:
  ScrollCornerView() = default;

  void OnPaint(gfx::Canvas* canvas) override {
    ui::NativeTheme::ExtraParams ignored;
    GetNativeTheme()->Paint(canvas->sk_canvas(),
                            ui::NativeTheme::kScrollbarCorner,
                            ui::NativeTheme::kNormal,
                            GetLocalBounds(),
                            ignored);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollCornerView);
};

// Returns true if any descendants of |view| have a layer (not including
// |view|).
bool DoesDescendantHaveLayer(View* view) {
  return std::any_of(view->children().cbegin(), view->children().cend(),
                     [](View* child) {
                       return child->layer() || DoesDescendantHaveLayer(child);
                     });
}

// Returns the position for the view so that it isn't scrolled off the visible
// region.
int CheckScrollBounds(int viewport_size, int content_size, int current_pos) {
  return base::ClampToRange(current_pos, 0,
                            std::max(content_size - viewport_size, 0));
}

// Make sure the content is not scrolled out of bounds
void ConstrainScrollToBounds(View* viewport,
                             View* view,
                             bool scroll_with_layers_enabled) {
  if (!view)
    return;

  // Note that even when ScrollView::ScrollsWithLayers() is true, the header row
  // scrolls by repainting.
  const bool scrolls_with_layers =
      scroll_with_layers_enabled && viewport->layer() != nullptr;
  if (scrolls_with_layers) {
    DCHECK(view->layer());
    DCHECK_EQ(0, view->x());
    DCHECK_EQ(0, view->y());
  }
  gfx::ScrollOffset offset = scrolls_with_layers
                                 ? view->layer()->CurrentScrollOffset()
                                 : gfx::ScrollOffset(-view->x(), -view->y());

  int x = CheckScrollBounds(viewport->width(), view->width(), offset.x());
  int y = CheckScrollBounds(viewport->height(), view->height(), offset.y());

  if (scrolls_with_layers) {
    view->layer()->SetScrollOffset(gfx::ScrollOffset(x, y));
  } else {
    // This is no op if bounds are the same
    view->SetBounds(-x, -y, view->width(), view->height());
  }
}

// Used by ScrollToPosition() to make sure the new position fits within the
// allowed scroll range.
int AdjustPosition(int current_position,
                   int new_position,
                   int content_size,
                   int viewport_size) {
  if (-current_position == new_position)
    return new_position;
  if (new_position < 0)
    return 0;
  const int max_position = std::max(0, content_size - viewport_size);
  return (new_position > max_position) ? max_position : new_position;
}

}  // namespace

// Viewport contains the contents View of the ScrollView.
class ScrollView::Viewport : public View {
 public:
  explicit Viewport(ScrollView* scroll_view) : scroll_view_(scroll_view) {}
  ~Viewport() override = default;

  void ScrollRectToVisible(const gfx::Rect& rect) override {
    if (children().empty() || !parent())
      return;

    View* contents = children().front();
    gfx::Rect scroll_rect(rect);

    if (scroll_view_->ScrollsWithLayers()) {
      // With layer scrolling, there's no need to "undo" the offset done in the
      // child's View::ScrollRectToVisible() before it calls this.
      DCHECK_EQ(0, contents->x());
      DCHECK_EQ(0, contents->y());
    } else {
      scroll_rect.Offset(-contents->x(), -contents->y());
    }

    scroll_view_->ScrollContentsRegionToBeVisible(scroll_rect);
  }

  // TODO(https://crbug.com/947053): this override should not be necessary, but
  // there are some assumptions that this calls Layout().
  void ChildPreferredSizeChanged(View* child) override {
    if (parent())
      parent()->Layout();
  }

  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (details.is_add && IsContentsViewport() && Contains(details.parent))
      scroll_view_->UpdateViewportLayerForClipping();
  }

  void OnChildLayerChanged(View* child) override {
    if (IsContentsViewport())
      scroll_view_->UpdateViewportLayerForClipping();
  }

 private:
  bool IsContentsViewport() const {
    return parent() && scroll_view_->contents_viewport_ == this;
  }

  ScrollView* scroll_view_;

  DISALLOW_COPY_AND_ASSIGN(Viewport);
};

ScrollView::ScrollView()
    : horiz_sb_(PlatformStyle::CreateScrollBar(true)),
      vert_sb_(PlatformStyle::CreateScrollBar(false)),
      corner_view_(std::make_unique<ScrollCornerView>()),
      scroll_with_layers_enabled_(base::FeatureList::IsEnabled(
          ::features::kUiCompositorScrollWithLayers)) {
  set_notify_enter_exit_on_child(true);

  // Since |contents_viewport_| is accessed during the AddChildView call, make
  // sure the field is initialized.
  auto contents_viewport = std::make_unique<Viewport>(this);
  contents_viewport_ = contents_viewport.get();
  AddChildView(std::move(contents_viewport));
  header_viewport_ = AddChildView(std::make_unique<Viewport>(this));

  // Don't add the scrollbars as children until we discover we need them
  // (ShowOrHideScrollBar).
  horiz_sb_->SetVisible(false);
  horiz_sb_->set_controller(this);
  vert_sb_->SetVisible(false);
  vert_sb_->set_controller(this);
  corner_view_->SetVisible(false);

  // Just make sure the more_content indicators aren't visible for now. They'll
  // be added as child controls and appropriately made visible depending on
  // |show_edges_with_hidden_content_|.
  more_content_left_->SetVisible(false);
  more_content_top_->SetVisible(false);
  more_content_right_->SetVisible(false);
  more_content_bottom_->SetVisible(false);

  if (scroll_with_layers_enabled_)
    EnableViewPortLayer();

  // If we're scrolling with layers, paint the overflow indicators to the layer.
  if (ScrollsWithLayers()) {
    more_content_left_->SetPaintToLayer();
    more_content_top_->SetPaintToLayer();
    more_content_right_->SetPaintToLayer();
    more_content_bottom_->SetPaintToLayer();
  }
  UpdateBackground();

  focus_ring_ = FocusRing::Install(this);
  focus_ring_->SetHasFocusPredicate([](View* view) -> bool {
    auto* v = static_cast<ScrollView*>(view);
    return v->draw_focus_indicator_;
  });
}

ScrollView::~ScrollView() = default;

// static
std::unique_ptr<ScrollView> ScrollView::CreateScrollViewWithBorder() {
  auto scroll_view = std::make_unique<ScrollView>();
  scroll_view->AddBorder();
  return scroll_view;
}

// static
ScrollView* ScrollView::GetScrollViewForContents(View* contents) {
  View* grandparent =
      contents->parent() ? contents->parent()->parent() : nullptr;
  if (!grandparent || grandparent->GetClassName() != ScrollView::kViewClassName)
    return nullptr;

  auto* scroll_view = static_cast<ScrollView*>(grandparent);
  DCHECK_EQ(contents, scroll_view->contents());
  return scroll_view;
}

void ScrollView::SetContentsImpl(std::unique_ptr<View> a_view) {
  // Protect against clients passing a contents view that has its own Layer.
  DCHECK(!a_view->layer());
  if (ScrollsWithLayers()) {
    bool fills_opaquely = true;
    if (!a_view->background()) {
      // Contents views may not be aware they need to fill their entire bounds -
      // play it safe here to avoid graphical glitches
      // (https://crbug.com/826472). If there's no solid background, mark the
      // view as not filling its bounds opaquely.
      if (GetBackgroundColor() != SK_ColorTRANSPARENT)
        a_view->SetBackground(CreateSolidBackground(GetBackgroundColor()));
      else
        fills_opaquely = false;
    }
    a_view->SetPaintToLayer();
    a_view->layer()->SetDidScrollCallback(base::BindRepeating(
        &ScrollView::OnLayerScrolled, base::Unretained(this)));
    a_view->layer()->SetScrollable(contents_viewport_->bounds().size());
    a_view->layer()->SetFillsBoundsOpaquely(fills_opaquely);
  }
  SetHeaderOrContents(contents_viewport_, std::move(a_view), &contents_);
}

void ScrollView::SetContents(std::nullptr_t) {
  SetContentsImpl(nullptr);
}

void ScrollView::SetHeaderImpl(std::unique_ptr<View> a_header) {
  SetHeaderOrContents(header_viewport_, std::move(a_header), &header_);
}

void ScrollView::SetHeader(std::nullptr_t) {
  SetHeaderImpl(nullptr);
}

void ScrollView::SetBackgroundColor(SkColor color) {
  if (background_color_data_.color == color)
    return;
  background_color_data_.color = color;
  use_color_id_ = false;
  UpdateBackground();
  OnPropertyChanged(&background_color_data_, kPropertyEffectsPaint);
}

void ScrollView::SetBackgroundThemeColorId(ui::NativeTheme::ColorId color_id) {
  background_color_data_.color_id = color_id;
  use_color_id_ = true;
  UpdateBackground();
}

gfx::Rect ScrollView::GetVisibleRect() const {
  if (!contents_)
    return gfx::Rect();
  gfx::ScrollOffset offset = CurrentOffset();
  return gfx::Rect(offset.x(), offset.y(), contents_viewport_->width(),
                   contents_viewport_->height());
}

void ScrollView::SetHideHorizontalScrollBar(bool visible) {
  if (hide_horizontal_scrollbar_ == visible)
    return;
  hide_horizontal_scrollbar_ = visible;
  OnPropertyChanged(&hide_horizontal_scrollbar_, kPropertyEffectsPaint);
}

void ScrollView::SetDrawOverflowIndicator(bool draw_overflow_indicator) {
  if (draw_overflow_indicator_ == draw_overflow_indicator)
    return;
  draw_overflow_indicator_ = draw_overflow_indicator;
  OnPropertyChanged(&draw_overflow_indicator_, kPropertyEffectsPaint);
}

void ScrollView::ClipHeightTo(int min_height, int max_height) {
  min_height_ = min_height;
  max_height_ = max_height;
}

int ScrollView::GetScrollBarLayoutWidth() const {
  return vert_sb_ && !vert_sb_->OverlapsContent() ? vert_sb_->GetThickness()
                                                  : 0;
}

int ScrollView::GetScrollBarLayoutHeight() const {
  return horiz_sb_ && !horiz_sb_->OverlapsContent() ? horiz_sb_->GetThickness()
                                                    : 0;
}

ScrollBar* ScrollView::SetHorizontalScrollBar(
    std::unique_ptr<ScrollBar> horiz_sb) {
  DCHECK(horiz_sb);
  horiz_sb->SetVisible(horiz_sb_->GetVisible());
  horiz_sb->set_controller(this);
  horiz_sb_ = std::move(horiz_sb);
  return horiz_sb_.get();
}

ScrollBar* ScrollView::SetVerticalScrollBar(
    std::unique_ptr<ScrollBar> vert_sb) {
  DCHECK(vert_sb);
  vert_sb->SetVisible(vert_sb_->GetVisible());
  vert_sb->set_controller(this);
  vert_sb_ = std::move(vert_sb);
  return vert_sb_.get();
}

void ScrollView::SetHasFocusIndicator(bool has_focus_indicator) {
  if (has_focus_indicator == draw_focus_indicator_)
    return;
  draw_focus_indicator_ = has_focus_indicator;

    focus_ring_->SchedulePaint();
  SchedulePaint();
  OnPropertyChanged(&draw_focus_indicator_, kPropertyEffectsPaint);
}

gfx::Size ScrollView::CalculatePreferredSize() const {
  if (!is_bounded())
    return View::CalculatePreferredSize();

  gfx::Size size = contents_->GetPreferredSize();
  size.SetToMax(gfx::Size(size.width(), min_height_));
  size.SetToMin(gfx::Size(size.width(), max_height_));
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

int ScrollView::GetHeightForWidth(int width) const {
  if (!is_bounded())
    return View::GetHeightForWidth(width);

  gfx::Insets insets = GetInsets();
  width = std::max(0, width - insets.width());
  int height = contents_->GetHeightForWidth(width) + insets.height();
  return base::ClampToRange(height, min_height_, max_height_);
}

void ScrollView::Layout() {
  // When horizontal scrollbar is disabled, it should not matter
  // if its OverlapsContent matches vertical bar's.
  if (!hide_horizontal_scrollbar_) {
#if defined(OS_MACOSX)
    // On Mac, scrollbars may update their style one at a time, so they may
    // temporarily be of different types. Refuse to lay out at this point.
    if (horiz_sb_->OverlapsContent() != vert_sb_->OverlapsContent())
      return;
#endif
    DCHECK_EQ(horiz_sb_->OverlapsContent(), vert_sb_->OverlapsContent());
  }

  if (focus_ring_)
    focus_ring_->Layout();

  gfx::Rect available_rect = GetContentsBounds();
  if (is_bounded()) {
    int content_width = available_rect.width();
    int content_height = contents_->GetHeightForWidth(content_width);
    if (content_height > height()) {
      content_width = std::max(content_width - GetScrollBarLayoutWidth(), 0);
      content_height = contents_->GetHeightForWidth(content_width);
    }
    contents_->SetSize(gfx::Size(content_width, content_height));
  }

  // Place an overflow indicator on each of the four edges of the content
  // bounds.
  PositionOverflowIndicators();

  // Most views will want to auto-fit the available space. Most of them want to
  // use all available width (without overflowing) and only overflow in
  // height. Examples are HistoryView, MostVisitedView, DownloadTabView, etc.
  // Other views want to fit in both ways. An example is PrintView. To make both
  // happy, assume a vertical scrollbar but no horizontal scrollbar. To override
  // this default behavior, the inner view has to calculate the available space,
  // used ComputeScrollBarsVisibility() to use the same calculation that is done
  // here and sets its bound to fit within.
  gfx::Rect viewport_bounds = available_rect;
  const int contents_x = viewport_bounds.x();
  const int contents_y = viewport_bounds.y();
  if (viewport_bounds.IsEmpty()) {
    // There's nothing to layout.
    return;
  }

  const int header_height =
      std::min(viewport_bounds.height(),
               header_ ? header_->GetPreferredSize().height() : 0);
  viewport_bounds.set_height(
      std::max(0, viewport_bounds.height() - header_height));
  viewport_bounds.set_y(viewport_bounds.y() + header_height);
  // viewport_size is the total client space available.
  gfx::Size viewport_size = viewport_bounds.size();

  // Assume both a vertical and horizontal scrollbar exist before calling
  // contents_->Layout(). This is because some contents_ will set their own size
  // to the contents_viewport_'s bounds. Failing to pre-allocate space for
  // the scrollbars will [non-intuitively] cause scrollbars to appear in
  // ComputeScrollBarsVisibility. This solution is also not perfect - if
  // scrollbars turn out *not* to be necessary, the contents will have slightly
  // less horizontal/vertical space than it otherwise would have had access to.
  // Unfortunately, there's no way to determine this without introducing a
  // circular dependency.
  const int horiz_sb_layout_height = GetScrollBarLayoutHeight();
  const int vert_sb_layout_width = GetScrollBarLayoutWidth();
  viewport_bounds.set_width(viewport_bounds.width() - vert_sb_layout_width);
  viewport_bounds.set_height(viewport_bounds.height() - horiz_sb_layout_height);

  // Update the bounds right now so the inner views can fit in it.
  contents_viewport_->SetBoundsRect(viewport_bounds);

  // Give |contents_| a chance to update its bounds if it depends on the
  // viewport.
  if (contents_)
    contents_->Layout();

  bool should_layout_contents = false;
  bool horiz_sb_required = false;
  bool vert_sb_required = false;
  if (contents_) {
    gfx::Size content_size = contents_->size();
    ComputeScrollBarsVisibility(viewport_size,
                                content_size,
                                &horiz_sb_required,
                                &vert_sb_required);
  }
  // Overlay scrollbars don't need a corner view.
  bool corner_view_required =
      horiz_sb_required && vert_sb_required && !vert_sb_->OverlapsContent();
  // Take action.
  SetControlVisibility(horiz_sb_.get(), horiz_sb_required);
  SetControlVisibility(vert_sb_.get(), vert_sb_required);
  SetControlVisibility(corner_view_.get(), corner_view_required);

  // Default.
  if (!horiz_sb_required) {
    viewport_bounds.set_height(viewport_bounds.height() +
                               horiz_sb_layout_height);
    should_layout_contents = true;
  }
  // Default.
  if (!vert_sb_required) {
    viewport_bounds.set_width(viewport_bounds.width() + vert_sb_layout_width);
    should_layout_contents = true;
  }

  if (horiz_sb_required) {
    gfx::Rect horiz_sb_bounds(contents_x, viewport_bounds.bottom(),
                              viewport_bounds.right() - contents_x,
                              horiz_sb_layout_height);
    if (horiz_sb_->OverlapsContent()) {
      horiz_sb_bounds.Inset(
          gfx::Insets(-horiz_sb_->GetThickness(), 0, 0,
                      vert_sb_required ? vert_sb_->GetThickness() : 0));
    }

    horiz_sb_->SetBoundsRect(horiz_sb_bounds);
  }
  if (vert_sb_required) {
    gfx::Rect vert_sb_bounds(viewport_bounds.right(), contents_y,
                             vert_sb_layout_width,
                             viewport_bounds.bottom() - contents_y);
    if (vert_sb_->OverlapsContent()) {
      // In the overlay scrollbar case, the scrollbar only covers the viewport
      // (and not the header).
      vert_sb_bounds.Inset(
          gfx::Insets(header_height, -vert_sb_->GetThickness(),
                      horiz_sb_required ? horiz_sb_->GetThickness() : 0, 0));
    }

    vert_sb_->SetBoundsRect(vert_sb_bounds);
  }
  if (corner_view_required) {
    // Show the resize corner.
    corner_view_->SetBounds(vert_sb_->bounds().x(), horiz_sb_->bounds().y(),
                            vert_sb_layout_width, horiz_sb_layout_height);
  }

  // Update to the real client size with the visible scrollbars.
  contents_viewport_->SetBoundsRect(viewport_bounds);
  if (should_layout_contents && contents_)
    contents_->Layout();

  // Even when |contents_| needs to scroll, it can still be narrower or wider
  // the viewport. So ensure the scrolling layer can fill the viewport, so that
  // events will correctly hit it, and overscroll looks correct.
  if (contents_ && ScrollsWithLayers()) {
    gfx::Size container_size = contents_ ? contents_->size() : gfx::Size();
    container_size.SetToMax(viewport_bounds.size());
    contents_->SetBoundsRect(gfx::Rect(container_size));
    contents_->layer()->SetScrollable(viewport_bounds.size());

    // Flip the viewport with layer transforms under RTL. Note the net effect is
    // to flip twice, so the text is not mirrored. This is necessary because
    // compositor scrolling is not RTL-aware. So although a toolkit-views layout
    // will flip, increasing a horizontal gfx::ScrollOffset will move content to
    // the left, regardless of RTL. A gfx::ScrollOffset must be positive, so to
    // move (unscrolled) content to the right, we need to flip the viewport
    // layer. That would flip all the content as well, so flip (and translate)
    // the content layer. Compensating in this way allows the scrolling/offset
    // logic to remain the same when scrolling via layers or bounds offsets.
    if (base::i18n::IsRTL()) {
      gfx::Transform flip;
      flip.Translate(viewport_bounds.width(), 0);
      flip.Scale(-1, 1);
      contents_viewport_->layer()->SetTransform(flip);

      // Add `contents_->width() - viewport_width` to the translation step. This
      // is to prevent the top-left of the (flipped) contents aligning to the
      // top-left of the viewport. Instead, the top-right should align in RTL.
      gfx::Transform shift;
      shift.Translate(2 * contents_->width() - viewport_bounds.width(), 0);
      shift.Scale(-1, 1);
      contents_->layer()->SetTransform(shift);
    }
  }

  header_viewport_->SetBounds(contents_x, contents_y,
                              viewport_bounds.width(), header_height);
  if (header_)
    header_->Layout();

  ConstrainScrollToBounds(header_viewport_, header_,
                          scroll_with_layers_enabled_);
  ConstrainScrollToBounds(contents_viewport_, contents_,
                          scroll_with_layers_enabled_);
  SchedulePaint();
  UpdateScrollBarPositions();
  if (contents_)
    UpdateOverflowIndicatorVisibility(CurrentOffset());
}

bool ScrollView::OnKeyPressed(const ui::KeyEvent& event) {
  bool processed = false;

  // Give vertical scrollbar priority
  if (vert_sb_->GetVisible())
    processed = vert_sb_->OnKeyPressed(event);

  if (!processed && horiz_sb_->GetVisible())
    processed = horiz_sb_->OnKeyPressed(event);

  return processed;
}

bool ScrollView::OnMouseWheel(const ui::MouseWheelEvent& e) {
  bool processed = false;

  // TODO(https://crbug.com/615948): Use composited scrolling.
  if (vert_sb_->GetVisible())
    processed = vert_sb_->OnMouseWheel(e);

  if (horiz_sb_->GetVisible())
    processed = horiz_sb_->OnMouseWheel(e) || processed;

  return processed;
}

void ScrollView::OnScrollEvent(ui::ScrollEvent* event) {
  if (!contents_)
    return;

  ui::ScrollInputHandler* compositor_scroller =
      GetWidget()->GetCompositor()->scroll_input_handler();
  if (compositor_scroller) {
    DCHECK(scroll_with_layers_enabled_);
    if (compositor_scroller->OnScrollEvent(*event, contents_->layer())) {
      event->SetHandled();
      event->StopPropagation();
    }
  }

  // A direction might not be known when the event stream starts, notify both
  // scrollbars that they may be about scroll, or that they may need to cancel
  // UI feedback once the scrolling direction is known.
  if (horiz_sb_)
    horiz_sb_->ObserveScrollEvent(*event);
  if (vert_sb_)
    vert_sb_->ObserveScrollEvent(*event);
}

void ScrollView::OnGestureEvent(ui::GestureEvent* event) {
  // If the event happened on one of the scrollbars, then those events are
  // sent directly to the scrollbars. Otherwise, only scroll events are sent to
  // the scrollbars.
  bool scroll_event = event->type() == ui::ET_GESTURE_SCROLL_UPDATE ||
                      event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
                      event->type() == ui::ET_GESTURE_SCROLL_END ||
                      event->type() == ui::ET_SCROLL_FLING_START;

  // TODO(https://crbug.com/615948): Use composited scrolling.
  if (vert_sb_->GetVisible()) {
    if (vert_sb_->bounds().Contains(event->location()) || scroll_event)
      vert_sb_->OnGestureEvent(event);
  }
  if (!event->handled() && horiz_sb_->GetVisible()) {
    if (horiz_sb_->bounds().Contains(event->location()) || scroll_event)
      horiz_sb_->OnGestureEvent(event);
  }
}

void ScrollView::OnThemeChanged() {
  UpdateBorder();
  if (use_color_id_)
    UpdateBackground();
}

void ScrollView::ScrollToPosition(ScrollBar* source, int position) {
  if (!contents_)
    return;

  gfx::ScrollOffset offset = CurrentOffset();
  if (source == horiz_sb_.get() && horiz_sb_->GetVisible()) {
    position = AdjustPosition(offset.x(), position, contents_->width(),
                              contents_viewport_->width());
    if (offset.x() == position)
      return;
    offset.set_x(position);
  } else if (source == vert_sb_.get() && vert_sb_->GetVisible()) {
    position = AdjustPosition(offset.y(), position, contents_->height(),
                              contents_viewport_->height());
    if (offset.y() == position)
      return;
    offset.set_y(position);
  }
  ScrollToOffset(offset);

  if (!ScrollsWithLayers())
    contents_->SchedulePaintInRect(contents_->GetVisibleBounds());
}

int ScrollView::GetScrollIncrement(ScrollBar* source, bool is_page,
                                   bool is_positive) {
  bool is_horizontal = source->IsHorizontal();
  if (is_page) {
    return is_horizontal ? contents_viewport_->width() :
                           contents_viewport_->height();
  }
  return is_horizontal ? contents_viewport_->width() / 5 :
                         contents_viewport_->height() / 5;
}

bool ScrollView::DoesViewportOrScrollViewHaveLayer() const {
  return layer() || contents_viewport_->layer();
}

void ScrollView::UpdateViewportLayerForClipping() {
  if (scroll_with_layers_enabled_)
    return;

  const bool has_layer = DoesViewportOrScrollViewHaveLayer();
  const bool needs_layer = DoesDescendantHaveLayer(contents_viewport_);
  if (has_layer == needs_layer)
    return;
  if (needs_layer)
    EnableViewPortLayer();
  else
    contents_viewport_->DestroyLayer();
}

void ScrollView::SetHeaderOrContents(View* parent,
                                     std::unique_ptr<View> new_view,
                                     View** member) {
  delete *member;
  if (new_view.get())
    *member = parent->AddChildView(std::move(new_view));
  else
    *member = nullptr;
  // TODO(https://crbug.com/947053): this should call InvalidateLayout(), but
  // there are some assumptions that it call Layout(). These assumptions should
  // be updated.
  Layout();
}

void ScrollView::ScrollContentsRegionToBeVisible(const gfx::Rect& rect) {
  if (!contents_ || (!horiz_sb_->GetVisible() && !vert_sb_->GetVisible()))
    return;

  // Figure out the maximums for this scroll view.
  const int contents_max_x =
      std::max(contents_viewport_->width(), contents_->width());
  const int contents_max_y =
      std::max(contents_viewport_->height(), contents_->height());

  int x = base::ClampToRange(rect.x(), 0, contents_max_x);
  int y = base::ClampToRange(rect.y(), 0, contents_max_y);

  // Figure out how far and down the rectangle will go taking width
  // and height into account.  This will be "clipped" by the viewport.
  const int max_x = std::min(contents_max_x,
      x + std::min(rect.width(), contents_viewport_->width()));
  const int max_y = std::min(contents_max_y,
      y + std::min(rect.height(), contents_viewport_->height()));

  // See if the rect is already visible. Note the width is (max_x - x)
  // and the height is (max_y - y) to take into account the clipping of
  // either viewport or the content size.
  const gfx::Rect vis_rect = GetVisibleRect();
  if (vis_rect.Contains(gfx::Rect(x, y, max_x - x, max_y - y)))
    return;

  // Shift contents_'s X and Y so that the region is visible. If we
  // need to shift up or left from where we currently are then we need
  // to get it so that the content appears in the upper/left
  // corner. This is done by setting the offset to -X or -Y.  For down
  // or right shifts we need to make sure it appears in the
  // lower/right corner. This is calculated by taking max_x or max_y
  // and scaling it back by the size of the viewport.
  const int new_x =
      (vis_rect.x() > x) ? x : std::max(0, max_x - contents_viewport_->width());
  const int new_y =
      (vis_rect.y() > y) ? y : std::max(0, max_y -
                                        contents_viewport_->height());

  ScrollToOffset(gfx::ScrollOffset(new_x, new_y));
}

void ScrollView::ComputeScrollBarsVisibility(const gfx::Size& vp_size,
                                             const gfx::Size& content_size,
                                             bool* horiz_is_shown,
                                             bool* vert_is_shown) const {
  if (hide_horizontal_scrollbar_) {
    *horiz_is_shown = false;
    *vert_is_shown = content_size.height() > vp_size.height();
    return;
  }

  // Try to fit both ways first, then try vertical bar only, then horizontal
  // bar only, then defaults to both shown.
  if (content_size.width() <= vp_size.width() &&
      content_size.height() <= vp_size.height()) {
    *horiz_is_shown = false;
    *vert_is_shown = false;
  } else if (content_size.width() <=
             vp_size.width() - GetScrollBarLayoutWidth()) {
    *horiz_is_shown = false;
    *vert_is_shown = true;
  } else if (content_size.height() <=
             vp_size.height() - GetScrollBarLayoutHeight()) {
    *horiz_is_shown = true;
    *vert_is_shown = false;
  } else {
    *horiz_is_shown = true;
    *vert_is_shown = true;
  }
}

// Make sure that a single scrollbar is created and visible as needed
void ScrollView::SetControlVisibility(View* control, bool should_show) {
  if (!control)
    return;
  if (should_show) {
    if (!control->GetVisible()) {
      AddChildView(control);
      control->SetVisible(true);
    }
  } else {
    RemoveChildView(control);
    control->SetVisible(false);
  }
}

void ScrollView::UpdateScrollBarPositions() {
  if (!contents_)
    return;

  const gfx::ScrollOffset offset = CurrentOffset();
  if (horiz_sb_->GetVisible()) {
    int vw = contents_viewport_->width();
    int cw = contents_->width();
    horiz_sb_->Update(vw, cw, offset.x());
  }
  if (vert_sb_->GetVisible()) {
    int vh = contents_viewport_->height();
    int ch = contents_->height();
    vert_sb_->Update(vh, ch, offset.y());
  }
}

gfx::ScrollOffset ScrollView::CurrentOffset() const {
  return ScrollsWithLayers()
             ? contents_->layer()->CurrentScrollOffset()
             : gfx::ScrollOffset(-contents_->x(), -contents_->y());
}

void ScrollView::ScrollToOffset(const gfx::ScrollOffset& offset) {
  if (ScrollsWithLayers()) {
    contents_->layer()->SetScrollOffset(offset);

    // TODO(tapted): Remove this call to OnLayerScrolled(). It's unnecessary,
    // but will only be invoked (asynchronously) when a Compositor is present
    // and commits a frame, which isn't true in some tests.
    // See http://crbug.com/637521.
    OnLayerScrolled(offset, contents_->layer()->element_id());
  } else {
    contents_->SetPosition(gfx::Point(-offset.x(), -offset.y()));
    ScrollHeader();
  }
  UpdateOverflowIndicatorVisibility(offset);
  UpdateScrollBarPositions();
}

bool ScrollView::ScrollsWithLayers() const {
  if (!scroll_with_layers_enabled_)
    return false;
  // Just check for the presence of a layer since it's cheaper than querying the
  // Feature flag each time.
  return contents_viewport_->layer() != nullptr;
}

void ScrollView::EnableViewPortLayer() {
  if (DoesViewportOrScrollViewHaveLayer())
    return;

  contents_viewport_->SetPaintToLayer();
  contents_viewport_->layer()->SetMasksToBounds(true);
  more_content_left_->SetPaintToLayer();
  more_content_top_->SetPaintToLayer();
  more_content_right_->SetPaintToLayer();
  more_content_bottom_->SetPaintToLayer();
  UpdateBackground();
}

void ScrollView::OnLayerScrolled(const gfx::ScrollOffset&,
                                 const cc::ElementId&) {
  UpdateScrollBarPositions();
  ScrollHeader();
}

void ScrollView::ScrollHeader() {
  if (!header_)
    return;

  int x_offset = CurrentOffset().x();
  if (header_->x() != -x_offset) {
    header_->SetX(-x_offset);
    header_->SchedulePaintInRect(header_->GetVisibleBounds());
  }
}

void ScrollView::AddBorder() {
  draw_border_ = true;
  UpdateBorder();
}

void ScrollView::UpdateBorder() {
  if (!draw_border_ || !GetWidget())
    return;

  SetBorder(CreateSolidBorder(
      1,
      GetNativeTheme()->GetSystemColor(
          draw_focus_indicator_
              ? ui::NativeTheme::kColorId_FocusedBorderColor
              : ui::NativeTheme::kColorId_UnfocusedBorderColor)));
}

void ScrollView::UpdateBackground() {
  const SkColor background_color = GetBackgroundColor();

  SetBackground(CreateSolidBackground(background_color));
  // In addition to setting the background of |this|, set the background on
  // the viewport as well. This way if the viewport has a layer
  // SetFillsBoundsOpaquely() is honored.
  contents_viewport_->SetBackground(CreateSolidBackground(background_color));
  if (contents_ && ScrollsWithLayers())
    contents_->SetBackground(CreateSolidBackground(background_color));
  if (contents_viewport_->layer()) {
    contents_viewport_->layer()->SetFillsBoundsOpaquely(background_color !=
                                                        SK_ColorTRANSPARENT);
  }
  SchedulePaint();
}

SkColor ScrollView::GetBackgroundColor() const {
  return use_color_id_
             ? GetNativeTheme()->GetSystemColor(background_color_data_.color_id)
             : background_color_data_.color;
}

void ScrollView::PositionOverflowIndicators() {
  const gfx::Rect bounds = GetContentsBounds();
  const int x = bounds.x();
  const int y = bounds.y();
  const int w = bounds.width();
  const int h = bounds.height();
  const int t = Separator::kThickness;
  more_content_left_->SetBounds(x, y, t, h);
  more_content_top_->SetBounds(x, y, w, t);
  more_content_right_->SetBounds(bounds.right() - t, y, t, h);
  more_content_bottom_->SetBounds(x, bounds.bottom() - t, w, t);
}

void ScrollView::UpdateOverflowIndicatorVisibility(
    const gfx::ScrollOffset& offset) {
  SetControlVisibility(more_content_top_.get(),
                       !draw_border_ && !header_ && vert_sb_->GetVisible() &&
                           offset.y() > vert_sb_->GetMinPosition() &&
                           draw_overflow_indicator_);
  SetControlVisibility(
      more_content_bottom_.get(),
      !draw_border_ && vert_sb_->GetVisible() && !horiz_sb_->GetVisible() &&
          offset.y() < vert_sb_->GetMaxPosition() && draw_overflow_indicator_);
  SetControlVisibility(more_content_left_.get(),
                       !draw_border_ && horiz_sb_->GetVisible() &&
                           offset.x() > horiz_sb_->GetMinPosition() &&
                           draw_overflow_indicator_);
  SetControlVisibility(
      more_content_right_.get(),
      !draw_border_ && horiz_sb_->GetVisible() && !vert_sb_->GetVisible() &&
          offset.x() < horiz_sb_->GetMaxPosition() && draw_overflow_indicator_);
}

BEGIN_METADATA(ScrollView)
METADATA_PARENT_CLASS(View)
ADD_READONLY_PROPERTY_METADATA(ScrollView, int, MinHeight)
ADD_READONLY_PROPERTY_METADATA(ScrollView, int, MaxHeight)
ADD_PROPERTY_METADATA(ScrollView, SkColor, BackgroundColor)
ADD_PROPERTY_METADATA(ScrollView, bool, DrawOverflowIndicator)
ADD_PROPERTY_METADATA(ScrollView, bool, HasFocusIndicator)
ADD_PROPERTY_METADATA(ScrollView, bool, HideHorizontalScrollBar)
END_METADATA()

// VariableRowHeightScrollHelper ----------------------------------------------

VariableRowHeightScrollHelper::VariableRowHeightScrollHelper(
    Controller* controller) : controller_(controller) {
}

VariableRowHeightScrollHelper::~VariableRowHeightScrollHelper() = default;

int VariableRowHeightScrollHelper::GetPageScrollIncrement(
    ScrollView* scroll_view, bool is_horizontal, bool is_positive) {
  if (is_horizontal)
    return 0;
  // y coordinate is most likely negative.
  int y = abs(scroll_view->contents()->y());
  int vis_height = scroll_view->contents()->parent()->height();
  if (is_positive) {
    // Align the bottom most row to the top of the view.
    int bottom = std::min(scroll_view->contents()->height() - 1,
                          y + vis_height);
    RowInfo bottom_row_info = GetRowInfo(bottom);
    // If 0, ScrollView will provide a default value.
    return std::max(0, bottom_row_info.origin - y);
  } else {
    // Align the row on the previous page to to the top of the view.
    int last_page_y = y - vis_height;
    RowInfo last_page_info = GetRowInfo(std::max(0, last_page_y));
    if (last_page_y != last_page_info.origin)
      return std::max(0, y - last_page_info.origin - last_page_info.height);
    return std::max(0, y - last_page_info.origin);
  }
}

int VariableRowHeightScrollHelper::GetLineScrollIncrement(
    ScrollView* scroll_view, bool is_horizontal, bool is_positive) {
  if (is_horizontal)
    return 0;
  // y coordinate is most likely negative.
  int y = abs(scroll_view->contents()->y());
  RowInfo row = GetRowInfo(y);
  if (is_positive) {
    return row.height - (y - row.origin);
  } else if (y == row.origin) {
    row = GetRowInfo(std::max(0, row.origin - 1));
    return y - row.origin;
  } else {
    return y - row.origin;
  }
}

VariableRowHeightScrollHelper::RowInfo
    VariableRowHeightScrollHelper::GetRowInfo(int y) {
  return controller_->GetRowInfo(y);
}

// FixedRowHeightScrollHelper -----------------------------------------------

FixedRowHeightScrollHelper::FixedRowHeightScrollHelper(int top_margin,
                                                       int row_height)
    : VariableRowHeightScrollHelper(nullptr),
      top_margin_(top_margin),
      row_height_(row_height) {
  DCHECK_GT(row_height, 0);
}

VariableRowHeightScrollHelper::RowInfo
    FixedRowHeightScrollHelper::GetRowInfo(int y) {
  if (y < top_margin_)
    return RowInfo(0, top_margin_);
  return RowInfo((y - top_margin_) / row_height_ * row_height_ + top_margin_,
                 row_height_);
}

}  // namespace views
