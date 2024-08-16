// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scroll_view.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/overscroll/scroll_input_handler.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Returns the combined scroll amount given separate x and y offsets. This is
// used in the "treat all scroll events as horizontal" case when there is both
// an x and y offset and we do not want them to add in unintuitive ways.
//
// The current approach is to return whichever offset has the larger absolute
// value, which should at least handle the case in which the gesture is mostly
// vertical or horizontal. It does mean that for a gesture at 135° or 315° from
// the x axis there is a breakpoint where scroll direction reverses, but we do
// not typically expect users to try to scroll a horizontal-scroll-only view at
// this exact angle.
template <class T>
T CombineScrollOffsets(T x, T y) {
  return std::abs(x) >= std::abs(y) ? x : y;
}

class ScrollCornerView : public View {
  METADATA_HEADER(ScrollCornerView, View)

 public:
  ScrollCornerView() = default;
  ScrollCornerView(const ScrollCornerView&) = delete;
  ScrollCornerView& operator=(const ScrollCornerView&) = delete;

  void OnPaint(gfx::Canvas* canvas) override {
#if BUILDFLAG(IS_APPLE)
    ui::NativeTheme::ExtraParams params(
        absl::in_place_type<ui::NativeTheme::ScrollbarExtraParams>);
#else
    ui::NativeTheme::ExtraParams params(
        absl::in_place_type<ui::NativeTheme::ScrollbarTrackExtraParams>);
#endif
    GetNativeTheme()->Paint(canvas->sk_canvas(), GetColorProvider(),
                            ui::NativeTheme::kScrollbarCorner,
                            ui::NativeTheme::kNormal, GetLocalBounds(), params);
  }
};

BEGIN_METADATA(ScrollCornerView)
END_METADATA

// Returns true if any descendants of |view| have a layer (not including
// |view|).
bool DoesDescendantHaveLayer(View* view) {
  return base::ranges::any_of(view->children(), [](View* child) {
    return child->layer() || DoesDescendantHaveLayer(child);
  });
}

// Returns the position for the view so that it isn't scrolled off the visible
// region.
int CheckScrollBounds(int viewport_size, int content_size, int current_pos) {
  return std::clamp(current_pos, 0, std::max(content_size - viewport_size, 0));
}

// Make sure the content is not scrolled out of bounds
void ConstrainScrollToBounds(View* viewport,
                             View* view,
                             bool scroll_with_layers_enabled) {
  if (!view) {
    return;
  }

  // Note that even when ScrollView::ScrollsWithLayers() is true, the header row
  // scrolls by repainting.
  const bool scrolls_with_layers =
      scroll_with_layers_enabled && viewport->layer() != nullptr;
  if (scrolls_with_layers) {
    DCHECK(view->layer());
    DCHECK_EQ(0, view->x());
    DCHECK_EQ(0, view->y());
  }
  gfx::PointF offset = scrolls_with_layers
                           ? view->layer()->CurrentScrollOffset()
                           : gfx::PointF(-view->x(), -view->y());

  int x = CheckScrollBounds(viewport->width(), view->width(), offset.x());
  int y = CheckScrollBounds(viewport->height(), view->height(), offset.y());

  if (scrolls_with_layers) {
    view->layer()->SetScrollOffset(gfx::PointF(x, y));
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
  if (-current_position == new_position) {
    return new_position;
  }
  if (new_position < 0) {
    return 0;
  }
  const int max_position = std::max(0, content_size - viewport_size);
  return (new_position > max_position) ? max_position : new_position;
}

}  // namespace

// Viewport contains the contents View of the ScrollView.
class ScrollView::Viewport : public View {
  METADATA_HEADER(Viewport, View)

 public:
  explicit Viewport(ScrollView* scroll_view) : scroll_view_(scroll_view) {}
  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;
  ~Viewport() override = default;

  void ScrollRectToVisible(const gfx::Rect& rect) override {
    if (children().empty() || !parent()) {
      return;
    }

    // If scrolling is disabled, it may have been handled by a parent View class
    // so fall back to it.
    if (!scroll_view_->IsHorizontalScrollEnabled() &&
        !scroll_view_->IsVerticalScrollEnabled()) {
      View::ScrollRectToVisible(rect);
      return;
    }

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

  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (details.is_add && GetIsContentsViewport() && Contains(details.parent)) {
      scroll_view_->UpdateViewportLayerForClipping();
      UpdateContentsViewportLayer();
    }
  }

  void OnChildLayerChanged(View* child) override {
    // If scroll_with_layers is enabled, explicitly disallowing to change the
    // layer on contents after the contents of ScrollView are set.
    DCHECK(!scroll_view_->scroll_with_layers_enabled_ ||
           child != scroll_view_->contents_)
        << "Layer of contents cannot be changed manually after the contents "
           "are set when scroll_with_layers is enabled.";

    if (GetIsContentsViewport()) {
      scroll_view_->UpdateViewportLayerForClipping();
      UpdateContentsViewportLayer();
    }
  }

  void InitializeContentsViewportLayer() {
    const ui::LayerType layer_type = CalculateLayerTypeForContentsViewport();
    SetContentsViewportLayer(layer_type);
  }

 private:
  void UpdateContentsViewportLayer() {
    if (!layer()) {
      return;
    }

    const ui::LayerType new_layer_type =
        CalculateLayerTypeForContentsViewport();

    bool layer_needs_update = layer()->type() != new_layer_type;
    if (layer_needs_update) {
      SetContentsViewportLayer(new_layer_type);
    }
  }

  // Calculates the layer type to use for |contents_viewport_|.
  ui::LayerType CalculateLayerTypeForContentsViewport() const {
    // Since contents_viewport_ is transparent, layer of contents_viewport_
    // can be NOT_DRAWN if contents_ have a TEXTURED layer.

    // When scroll_with_layers is enabled, we can always determine the layer
    // type of contents_viewport based on the type of layer that will be enabled
    // on contents.
    if (scroll_view_->scroll_with_layers_enabled_) {
      return scroll_view_->layer_type_ == ui::LAYER_TEXTURED
                 ? ui::LAYER_NOT_DRAWN
                 : ui::LAYER_TEXTURED;
    }

    // Getting contents of viewport through view hierarchy tree rather than
    // scroll_view->contents_, as this method can be called after the view
    // hierarchy is changed but before contents_ variable is updated. Hence
    // scroll_view->contents_ will have stale value in such situation.
    const View* contents =
        !this->children().empty() ? this->children()[0] : nullptr;

    auto has_textured_layer{[](const View* contents) {
      return contents->layer() &&
             contents->layer()->type() == ui::LAYER_TEXTURED;
    }};

    if (!contents || has_textured_layer(contents))
      return ui::LAYER_NOT_DRAWN;
    else
      return ui::LAYER_TEXTURED;
  }

  // Initializes or updates the layer of |contents_viewport|.
  void SetContentsViewportLayer(ui::LayerType layer_type) {
    // Only LAYER_NOT_DRAWN and LAYER_TEXTURED are allowed since
    // contents_viewport is a container view.
    DCHECK(layer_type == ui::LAYER_TEXTURED ||
           layer_type == ui::LAYER_NOT_DRAWN);

    SetPaintToLayer(layer_type);
  }

  bool GetIsContentsViewport() const {
    return parent() && scroll_view_->contents_viewport_ == this;
  }

  raw_ptr<ScrollView> scroll_view_;
};

BEGIN_METADATA(ScrollView, Viewport)
ADD_READONLY_PROPERTY_METADATA(bool, IsContentsViewport)
END_METADATA

ScrollView::ScrollView()
    : ScrollView(base::FeatureList::IsEnabled(
                     ::features::kUiCompositorScrollWithLayers)
                     ? ScrollWithLayers::kEnabled
                     : ScrollWithLayers::kDisabled) {}

ScrollView::ScrollView(ScrollWithLayers scroll_with_layers)
    : horiz_sb_(AddChildView(
          PlatformStyle::CreateScrollBar(ScrollBar::Orientation::kHorizontal))),
      vert_sb_(AddChildView(
          PlatformStyle::CreateScrollBar(ScrollBar::Orientation::kVertical))),
      corner_view_(std::make_unique<ScrollCornerView>()),
      scroll_with_layers_enabled_(scroll_with_layers ==
                                  ScrollWithLayers::kEnabled) {
  SetNotifyEnterExitOnChild(true);

  // Since |contents_viewport_| is accessed during the AddChildView call, make
  // sure the field is initialized.
  auto contents_viewport = std::make_unique<Viewport>(this);
  contents_viewport_ = contents_viewport.get();
  // Add content view port as the first child, so that the scollbars can
  // overlay it.
  AddChildViewAt(std::move(contents_viewport), 0);
  header_viewport_ = AddChildView(std::make_unique<Viewport>(this));

  horiz_sb_->SetVisible(false);
  horiz_sb_->set_controller(this);
  vert_sb_->SetVisible(false);
  vert_sb_->set_controller(this);
  corner_view_->SetVisible(false);

  // "Ignored" removes the scrollbar from the accessibility tree.
  // "IsLeaf" removes their children (e.g. the buttons and thumb).
  horiz_sb_->GetViewAccessibility().SetIsIgnored(true);
  horiz_sb_->GetViewAccessibility().SetIsLeaf(true);
  vert_sb_->GetViewAccessibility().SetIsIgnored(true);
  vert_sb_->GetViewAccessibility().SetIsLeaf(true);

  GetViewAccessibility().SetIsScrollable(true);
  GetViewAccessibility().SetScrollXMin(horiz_sb_->GetMinPosition());
  GetViewAccessibility().SetScrollXMax(horiz_sb_->GetMaxPosition());
  GetViewAccessibility().SetScrollYMin(vert_sb_->GetMinPosition());
  GetViewAccessibility().SetScrollYMax(vert_sb_->GetMaxPosition());
  GetViewAccessibility().SetRole(ax::mojom::Role::kScrollView);

  // Just make sure the more_content indicators aren't visible for now. They'll
  // be added as child controls and appropriately made visible depending on
  // |show_edges_with_hidden_content_|.
  more_content_left_->SetVisible(false);
  more_content_top_->SetVisible(false);
  more_content_right_->SetVisible(false);
  more_content_bottom_->SetVisible(false);

  if (scroll_with_layers_enabled_) {
    EnableViewportLayer();
  }

  // If we're scrolling with layers, paint the overflow indicators to the layer.
  if (ScrollsWithLayers()) {
    more_content_left_->SetPaintToLayer();
    more_content_top_->SetPaintToLayer();
    more_content_right_->SetPaintToLayer();
    more_content_bottom_->SetPaintToLayer();
  }

  FocusRing::Install(this);
  views::FocusRing::Get(this)->SetHasFocusPredicate(
      base::BindRepeating([](const View* view) {
        const auto* v = views::AsViewClass<ScrollView>(view);
        CHECK(v);
        return v->draw_focus_indicator_;
      }));
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
  if (!grandparent || !IsViewClass<ScrollView>(grandparent)) {
    return nullptr;
  }

  auto* scroll_view = static_cast<ScrollView*>(grandparent);
  DCHECK_EQ(contents, scroll_view->contents());
  return scroll_view;
}

void ScrollView::SetContentsImpl(std::unique_ptr<View> a_view) {
  // Protect against clients passing a contents view that has its own Layer.
  DCHECK(!a_view || !a_view->layer());

  if (a_view && ScrollsWithLayers()) {
    a_view->SetPaintToLayer(layer_type_);
    a_view->layer()->SetDidScrollCallback(base::BindRepeating(
        &ScrollView::OnLayerScrolled, base::Unretained(this)));
    a_view->layer()->SetScrollable(contents_viewport_->bounds().size());
  }
  contents_ = ReplaceChildView(
      contents_viewport_, contents_.ExtractAsDangling(), std::move(a_view));
  UpdateBackground();
}

void ScrollView::SetContents(std::nullptr_t) {
  SetContentsImpl(nullptr);
}

void ScrollView::SetContentsLayerType(ui::LayerType layer_type) {
  // This function should only be called when scroll with layers is enabled and
  // before `contents_` is set.
  DCHECK(ScrollsWithLayers() && !contents_);

  // Currently only allow LAYER_TEXTURED and LAYER_NOT_DRAWN. If other types of
  // layer are needed, consult with the owner.
  DCHECK(layer_type == ui::LAYER_TEXTURED || layer_type == ui::LAYER_NOT_DRAWN);

  if (layer_type_ == layer_type) {
    return;
  }

  layer_type_ = layer_type;
}

void ScrollView::SetHeaderImpl(std::unique_ptr<View> a_header) {
  header_ = ReplaceChildView(header_viewport_, header_.ExtractAsDangling(),
                             std::move(a_header));
}

void ScrollView::SetHeader(std::nullptr_t) {
  SetHeaderImpl(nullptr);
}

void ScrollView::SetPreferredViewportMargins(const gfx::Insets& margins) {
  preferred_viewport_margins_ = margins;
}

void ScrollView::SetViewportRoundedCornerRadius(
    const gfx::RoundedCornersF& radii) {
  DCHECK(contents_viewport_->layer())
      << "Please ensure you have enabled ScrollWithLayers.";

  contents_viewport_->layer()->SetRoundedCornerRadius(radii);
}

void ScrollView::SetBackgroundColor(const std::optional<SkColor>& color) {
  if (background_color_ == color && !background_color_id_) {
    return;
  }
  background_color_ = color;
  background_color_id_ = std::nullopt;
  UpdateBackground();
  OnPropertyChanged(&background_color_, kPropertyEffectsPaint);
}

void ScrollView::SetBackgroundThemeColorId(
    const std::optional<ui::ColorId>& color_id) {
  if (background_color_id_ == color_id && !background_color_) {
    return;
  }
  background_color_id_ = color_id;
  background_color_ = std::nullopt;
  UpdateBackground();
  OnPropertyChanged(&background_color_id_, kPropertyEffectsPaint);
}

gfx::Rect ScrollView::GetVisibleRect() const {
  if (!contents_) {
    return gfx::Rect();
  }
  gfx::PointF offset = CurrentOffset();
  return gfx::Rect(offset.x(), offset.y(), contents_viewport_->width(),
                   contents_viewport_->height());
}

void ScrollView::SetHorizontalScrollBarMode(
    ScrollBarMode horizontal_scroll_bar_mode) {
  if (horizontal_scroll_bar_mode_ == horizontal_scroll_bar_mode) {
    return;
  }
  horizontal_scroll_bar_mode_ = horizontal_scroll_bar_mode;
  OnPropertyChanged(&horizontal_scroll_bar_mode_, kPropertyEffectsPaint);

  // "Ignored" removes the scrollbar from the accessibility tree.
  // "IsLeaf" removes their children (e.g. the buttons and thumb).
  bool is_disabled = horizontal_scroll_bar_mode == ScrollBarMode::kDisabled;
  horiz_sb_->GetViewAccessibility().SetIsIgnored(is_disabled);
  horiz_sb_->GetViewAccessibility().SetIsLeaf(is_disabled);
}

void ScrollView::SetVerticalScrollBarMode(
    ScrollBarMode vertical_scroll_bar_mode) {
  if (vertical_scroll_bar_mode_ == vertical_scroll_bar_mode) {
    return;
  }

  // Enabling vertical scrolling is incompatible with all scrolling being
  // interpreted as horizontal.
  DCHECK(!treat_all_scroll_events_as_horizontal_ ||
         vertical_scroll_bar_mode == ScrollBarMode::kDisabled);

  vertical_scroll_bar_mode_ = vertical_scroll_bar_mode;
  OnPropertyChanged(&vertical_scroll_bar_mode_, kPropertyEffectsPaint);

  // "Ignored" removes the scrollbar from the accessibility tree.
  // "IsLeaf" removes their children (e.g. the buttons and thumb).
  bool is_disabled = vertical_scroll_bar_mode == ScrollBarMode::kDisabled;
  vert_sb_->GetViewAccessibility().SetIsIgnored(is_disabled);
  vert_sb_->GetViewAccessibility().SetIsLeaf(is_disabled);
}

void ScrollView::SetTreatAllScrollEventsAsHorizontal(
    bool treat_all_scroll_events_as_horizontal) {
  if (treat_all_scroll_events_as_horizontal_ ==
      treat_all_scroll_events_as_horizontal) {
    return;
  }
  treat_all_scroll_events_as_horizontal_ =
      treat_all_scroll_events_as_horizontal;
  OnPropertyChanged(&treat_all_scroll_events_as_horizontal_,
                    kPropertyEffectsNone);

  // Since this effectively disables vertical scrolling, don't show a
  // vertical scrollbar.
  SetVerticalScrollBarMode(ScrollBarMode::kDisabled);
}

void ScrollView::SetAllowKeyboardScrolling(bool allow_keyboard_scrolling) {
  if (allow_keyboard_scrolling_ == allow_keyboard_scrolling) {
    return;
  }
  allow_keyboard_scrolling_ = allow_keyboard_scrolling;
  OnPropertyChanged(&allow_keyboard_scrolling_, kPropertyEffectsNone);
}

void ScrollView::SetDrawOverflowIndicator(bool draw_overflow_indicator) {
  if (draw_overflow_indicator_ == draw_overflow_indicator) {
    return;
  }
  draw_overflow_indicator_ = draw_overflow_indicator;
  OnPropertyChanged(&draw_overflow_indicator_, kPropertyEffectsPaint);
}

View* ScrollView::SetCustomOverflowIndicator(OverflowIndicatorAlignment side,
                                             std::unique_ptr<View> indicator,
                                             int thickness,
                                             bool fills_opaquely) {
  if (thickness < 0) {
    thickness = 0;
  }

  if (ScrollsWithLayers()) {
    indicator->SetPaintToLayer();
    indicator->layer()->SetFillsBoundsOpaquely(fills_opaquely);
  }

  View* indicator_ptr = indicator.get();
  switch (side) {
    case OverflowIndicatorAlignment::kLeft:
      more_content_left_ = std::move(indicator);
      more_content_left_thickness_ = thickness;
      break;
    case OverflowIndicatorAlignment::kTop:
      more_content_top_ = std::move(indicator);
      more_content_top_thickness_ = thickness;
      break;
    case OverflowIndicatorAlignment::kRight:
      more_content_right_ = std::move(indicator);
      more_content_right_thickness_ = thickness;
      break;
    case OverflowIndicatorAlignment::kBottom:
      more_content_bottom_ = std::move(indicator);
      more_content_bottom_thickness_ = thickness;
      break;
    default:
      NOTREACHED();
  }

  UpdateOverflowIndicatorVisibility(CurrentOffset());
  PositionOverflowIndicators();

  return indicator_ptr;
}

void ScrollView::ClipHeightTo(int min_height, int max_height) {
  if (min_height != min_height_ || max_height != max_height_)
    PreferredSizeChanged();

  min_height_ = min_height;
  max_height_ = max_height;
}

int ScrollView::GetScrollBarLayoutWidth() const {
  return vert_sb_->OverlapsContent() ? 0 : vert_sb_->GetThickness();
}

int ScrollView::GetScrollBarLayoutHeight() const {
  return horiz_sb_->OverlapsContent() ? 0 : horiz_sb_->GetThickness();
}

ScrollBar* ScrollView::SetHorizontalScrollBar(
    std::unique_ptr<ScrollBar> horiz_sb) {
  horiz_sb->SetVisible(horiz_sb_->GetVisible());
  horiz_sb->set_controller(this);
  RemoveChildViewT(horiz_sb_.ExtractAsDangling());
  horiz_sb_ = AddChildView(std::move(horiz_sb));
  GetViewAccessibility().SetScrollXMin(horiz_sb_->GetMinPosition());
  GetViewAccessibility().SetScrollXMax(horiz_sb_->GetMaxPosition());
  return horiz_sb_;
}

ScrollBar* ScrollView::SetVerticalScrollBar(
    std::unique_ptr<ScrollBar> vert_sb) {
  DCHECK(vert_sb);
  vert_sb->SetVisible(vert_sb_->GetVisible());
  vert_sb->set_controller(this);
  RemoveChildViewT(vert_sb_.ExtractAsDangling());
  vert_sb_ = AddChildView(std::move(vert_sb));
  GetViewAccessibility().SetScrollYMin(vert_sb_->GetMinPosition());
  GetViewAccessibility().SetScrollYMax(vert_sb_->GetMaxPosition());
  return vert_sb_;
}

void ScrollView::SetHasFocusIndicator(bool has_focus_indicator) {
  if (has_focus_indicator == draw_focus_indicator_) {
    return;
  }
  draw_focus_indicator_ = has_focus_indicator;

  views::FocusRing::Get(this)->SchedulePaint();
  SchedulePaint();
  OnPropertyChanged(&draw_focus_indicator_, kPropertyEffectsPaint);
}

base::CallbackListSubscription ScrollView::AddContentsScrolledCallback(
    ScrollViewCallback callback) {
  return on_contents_scrolled_.Add(std::move(callback));
}

base::CallbackListSubscription ScrollView::AddContentsScrollEndedCallback(
    ScrollViewCallback callback) {
  return on_contents_scroll_ended_.Add(std::move(callback));
}

gfx::Size ScrollView::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  gfx::Insets insets = GetInsets();
  gfx::Size size =
      contents_ ? contents_->GetPreferredSize(available_size.Inset(insets))
                : gfx::Size();
  size.Enlarge(insets.width(), insets.height());

  if (is_bounded()) {
    size.SetToMax(gfx::Size(size.width(), min_height_));
    size.SetToMin(gfx::Size(size.width(), max_height_));
  }
  return size;
}

void ScrollView::Layout(PassKey) {
  // When either scrollbar is disabled, it should not matter
  // if its OverlapsContent matches other bar's.
  if (horizontal_scroll_bar_mode_ == ScrollBarMode::kEnabled &&
      vertical_scroll_bar_mode_ == ScrollBarMode::kEnabled) {
#if BUILDFLAG(IS_MAC)
    // On Mac, scrollbars may update their style one at a time, so they may
    // temporarily be of different types. Refuse to lay out at this point.
    if (horiz_sb_->OverlapsContent() != vert_sb_->OverlapsContent()) {
      return;
    }
#endif
    DCHECK_EQ(horiz_sb_->OverlapsContent(), vert_sb_->OverlapsContent());
  }

  if (views::FocusRing::Get(this)) {
    views::FocusRing::Get(this)->DeprecatedLayoutImmediately();
  }

  gfx::Rect available_rect = GetContentsBounds();
  if (is_bounded() && contents_) {
    int content_width = available_rect.width();
    int content_height = contents_->GetHeightForWidth(content_width);
    if (content_height > available_rect.height()) {
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
               header_ ? header_->GetPreferredSize({}).height() : 0);
  viewport_bounds.set_height(
      std::max(0, viewport_bounds.height() - header_height));
  viewport_bounds.set_y(viewport_bounds.y() + header_height);
  // viewport_size is the total client space available.
  gfx::Size viewport_size = viewport_bounds.size();

  // Assume both a vertical and horizontal scrollbar exist before calling
  // contents_->DeprecatedLayoutImmediately(). This is because some contents_
  // will set their own size to the contents_viewport_'s bounds. Failing to
  // pre-allocate space for the scrollbars will [non-intuitively] cause
  // scrollbars to appear in ComputeScrollBarsVisibility. This solution is also
  // not perfect - if scrollbars turn out *not* to be necessary, the contents
  // will have slightly less horizontal/vertical space than it otherwise would
  // have had access to. Unfortunately, there's no way to determine this without
  // introducing a circular dependency.
  const int horiz_sb_layout_height = GetScrollBarLayoutHeight();
  const int vert_sb_layout_width = GetScrollBarLayoutWidth();
  viewport_bounds.set_width(viewport_bounds.width() - vert_sb_layout_width);
  viewport_bounds.set_height(viewport_bounds.height() - horiz_sb_layout_height);

  // Update the bounds right now so the inner views can fit in it.
  contents_viewport_->SetBoundsRect(viewport_bounds);

  // Give |contents_| a chance to update its bounds if it depends on the
  // viewport.
  if (contents_) {
    contents_->DeprecatedLayoutImmediately();
  }

  bool should_layout_contents = false;
  bool horiz_sb_required = false;
  bool vert_sb_required = false;
  if (contents_) {
    gfx::Size content_size = contents_->size();
    ComputeScrollBarsVisibility(viewport_size, content_size, &horiz_sb_required,
                                &vert_sb_required);
  }
  // Overlay scrollbars don't need a corner view.
  bool corner_view_required =
      horiz_sb_required && vert_sb_required && !vert_sb_->OverlapsContent();
  // Take action.
  horiz_sb_->SetVisible(horiz_sb_required);
  vert_sb_->SetVisible(vert_sb_required);
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
          gfx::Insets::TLBR(-horiz_sb_->GetThickness(), 0, 0,
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
      vert_sb_bounds.Inset(gfx::Insets::TLBR(
          header_height, -vert_sb_->GetThickness(),
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
  if (should_layout_contents && contents_) {
    contents_->DeprecatedLayoutImmediately();
  }

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
    // will flip, increasing a horizontal scroll offset will move content to
    // the left, regardless of RTL. A scroll offset must be positive, so to
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

  header_viewport_->SetBounds(contents_x, contents_y, viewport_bounds.width(),
                              header_height);
  if (header_) {
    header_->DeprecatedLayoutImmediately();
  }

  ConstrainScrollToBounds(header_viewport_, header_,
                          scroll_with_layers_enabled_);
  ConstrainScrollToBounds(contents_viewport_, contents_,
                          scroll_with_layers_enabled_);
  SchedulePaint();
  UpdateScrollBarPositions();
  if (contents_) {
    UpdateOverflowIndicatorVisibility(CurrentOffset());
  }
}

bool ScrollView::OnKeyPressed(const ui::KeyEvent& event) {
  bool processed = false;

  if (!allow_keyboard_scrolling_) {
    return false;
  }

  // Give vertical scrollbar priority
  if (IsVerticalScrollEnabled()) {
    processed = vert_sb_->OnKeyPressed(event);
  }

  if (!processed && IsHorizontalScrollEnabled())
    processed = horiz_sb_->OnKeyPressed(event);

  return processed;
}

bool ScrollView::OnMouseWheel(const ui::MouseWheelEvent& e) {
  bool processed = false;

  const ui::MouseWheelEvent to_propagate =
      treat_all_scroll_events_as_horizontal_
          ? ui::MouseWheelEvent(
                e, CombineScrollOffsets(e.x_offset(), e.y_offset()), 0)
          : e;

  // TODO(crbug.com/40471184): Use composited scrolling.
  if (IsVerticalScrollEnabled())
    processed = vert_sb_->OnMouseWheel(to_propagate);

  if (IsHorizontalScrollEnabled()) {
    // When there is no vertical scrollbar, allow vertical scroll events to be
    // interpreted as horizontal scroll events.
    processed |= horiz_sb_->OnMouseWheel(to_propagate);
  }

  return processed;
}

void ScrollView::OnScrollEvent(ui::ScrollEvent* event) {
  if (!contents_) {
    return;
  }

  // Possibly force the scroll event to horizontal based on the configuration
  // option.
  ui::ScrollEvent e =
      treat_all_scroll_events_as_horizontal_
          ? ui::ScrollEvent(
                event->type(), event->location_f(), event->root_location_f(),
                event->time_stamp(), event->flags(),
                CombineScrollOffsets(event->x_offset(), event->y_offset()),
                0.0f,
                CombineScrollOffsets(event->y_offset_ordinal(),
                                     event->x_offset_ordinal()),
                0.0f, event->finger_count(), event->momentum_phase(),
                event->scroll_event_phase())
          : *event;

  ui::ScrollInputHandler* compositor_scroller =
      GetWidget()->GetCompositor()->scroll_input_handler();
  if (compositor_scroller) {
    DCHECK(scroll_with_layers_enabled_);
    if (compositor_scroller->OnScrollEvent(e, contents_->layer())) {
      e.SetHandled();
      e.StopPropagation();
    }
  }

  // A direction might not be known when the event stream starts, notify both
  // scrollbars that they may be about scroll, or that they may need to cancel
  // UI feedback once the scrolling direction is known.
  horiz_sb_->ObserveScrollEvent(e);
  vert_sb_->ObserveScrollEvent(e);

  // Need to copy state back to original event.
  if (e.handled()) {
    event->SetHandled();
  }
  if (e.stopped_propagation()) {
    event->StopPropagation();
  }
}

void ScrollView::OnGestureEvent(ui::GestureEvent* event) {
  // If the event happened on one of the scrollbars, then those events are
  // sent directly to the scrollbars. Otherwise, only scroll events are sent to
  // the scrollbars.
  bool scroll_event = event->type() == ui::EventType::kGestureScrollUpdate ||
                      event->type() == ui::EventType::kGestureScrollBegin ||
                      event->type() == ui::EventType::kGestureScrollEnd ||
                      event->type() == ui::EventType::kScrollFlingStart;

  // Note: we will not invert gesture events because it will be confusing to
  // have a vertical finger gesture on a touchscreen cause the scroll pane to
  // scroll horizontally.

  // TODO(crbug.com/40471184): Use composited scrolling.
  if (IsVerticalScrollEnabled() &&
      (scroll_event || (vert_sb_->GetVisible() &&
                        vert_sb_->bounds().Contains(event->location())))) {
    vert_sb_->OnGestureEvent(event);
  }
  if (!event->handled() && IsHorizontalScrollEnabled() &&
      (scroll_event || (horiz_sb_->GetVisible() &&
                        horiz_sb_->bounds().Contains(event->location())))) {
    horiz_sb_->OnGestureEvent(event);
  }
}

void ScrollView::OnThemeChanged() {
  View::OnThemeChanged();
  UpdateBorder();
  UpdateBackground();
}

bool ScrollView::HandleAccessibleAction(const ui::AXActionData& action_data) {
  if (!contents_) {
    return View::HandleAccessibleAction(action_data);
  }

  switch (action_data.action) {
    case ax::mojom::Action::kScrollLeft:
      return horiz_sb_->ScrollByAmount(ScrollBar::ScrollAmount::kPrevPage);
    case ax::mojom::Action::kScrollRight:
      return horiz_sb_->ScrollByAmount(ScrollBar::ScrollAmount::kNextPage);
    case ax::mojom::Action::kScrollUp:
      return vert_sb_->ScrollByAmount(ScrollBar::ScrollAmount::kPrevPage);
    case ax::mojom::Action::kScrollDown:
      return vert_sb_->ScrollByAmount(ScrollBar::ScrollAmount::kNextPage);
    case ax::mojom::Action::kSetScrollOffset:
      ScrollToOffset(gfx::PointF(action_data.target_point));
      return true;
    default:
      return View::HandleAccessibleAction(action_data);
  }
}

void ScrollView::ScrollToPosition(ScrollBar* source, int position) {
  if (!contents_) {
    return;
  }

  gfx::PointF offset = CurrentOffset();
  if (source == horiz_sb_ && IsHorizontalScrollEnabled()) {
    position = AdjustPosition(offset.x(), position, contents_->width(),
                              contents_viewport_->width());
    if (offset.x() == position) {
      return;
    }
    offset.set_x(position);
  } else if (source == vert_sb_ && IsVerticalScrollEnabled()) {
    position = AdjustPosition(offset.y(), position, contents_->height(),
                              contents_viewport_->height());
    if (offset.y() == position) {
      return;
    }
    offset.set_y(position);
  }
  ScrollToOffset(offset);

  if (!ScrollsWithLayers())
    contents_->SchedulePaintInRect(contents_->GetVisibleBounds());
}

int ScrollView::GetScrollIncrement(ScrollBar* source,
                                   bool is_page,
                                   bool is_positive) {
  bool is_horizontal =
      source->GetOrientation() == ScrollBar::Orientation::kHorizontal;
  if (is_page) {
    return is_horizontal ? contents_viewport_->width()
                         : contents_viewport_->height();
  }
  return is_horizontal ? contents_viewport_->width() / 5
                       : contents_viewport_->height() / 5;
}

void ScrollView::OnScrollEnded() {
  on_contents_scroll_ended_.Notify();
}

bool ScrollView::DoesViewportOrScrollViewHaveLayer() const {
  return layer() || contents_viewport_->layer();
}

void ScrollView::UpdateViewportLayerForClipping() {
  if (scroll_with_layers_enabled_) {
    return;
  }

  const bool has_layer = DoesViewportOrScrollViewHaveLayer();
  const bool needs_layer = DoesDescendantHaveLayer(contents_viewport_);
  if (has_layer == needs_layer) {
    return;
  }
  if (needs_layer)
    EnableViewportLayer();
  else
    contents_viewport_->DestroyLayer();
}

View* ScrollView::ReplaceChildView(View* parent,
                                   raw_ptr<View>::DanglingType old_view,
                                   std::unique_ptr<View> new_view) {
  if (old_view) {
    parent->RemoveChildViewT(old_view);
  }
  View* result = nullptr;
  if (new_view.get()) {
    result = parent->AddChildViewAt(std::move(new_view), 0);
  }
  InvalidateLayout();
  return result;
}

void ScrollView::ScrollContentsRegionToBeVisible(const gfx::Rect& rect) {
  if (!contents_) {
    return;
  }

  gfx::Rect contents_region = rect;
  contents_region.Inset(-preferred_viewport_margins_);

  // Figure out the maximums for this scroll view.
  const int contents_max_x =
      std::max(contents_viewport_->width(), contents_->width());
  const int contents_max_y =
      std::max(contents_viewport_->height(), contents_->height());

  int x = std::clamp(contents_region.x(), 0, contents_max_x);
  int y = std::clamp(contents_region.y(), 0, contents_max_y);

  // Figure out how far and down the rectangle will go taking width
  // and height into account.  This will be "clipped" by the viewport.
  const int max_x = std::min(
      contents_max_x,
      x + std::min(contents_region.width(), contents_viewport_->width()));
  const int max_y = std::min(
      contents_max_y,
      y + std::min(contents_region.height(), contents_viewport_->height()));

  // See if the rect is already visible. Note the width is (max_x - x)
  // and the height is (max_y - y) to take into account the clipping of
  // either viewport or the content size.
  const gfx::Rect vis_rect = GetVisibleRect();
  if (vis_rect.Contains(gfx::Rect(x, y, max_x - x, max_y - y))) {
    return;
  }

  // Shift contents_'s X and Y so that the region is visible. If we
  // need to shift up or left from where we currently are then we need
  // to get it so that the content appears in the upper/left
  // corner. This is done by setting the offset to -X or -Y.  For down
  // or right shifts we need to make sure it appears in the
  // lower/right corner. This is calculated by taking max_x or max_y
  // and scaling it back by the size of the viewport.
  const int new_x =
      (vis_rect.x() > x) ? x : std::max(0, max_x - contents_viewport_->width());
  const int new_y = (vis_rect.y() > y)
                        ? y
                        : std::max(0, max_y - contents_viewport_->height());

  ScrollToOffset(gfx::PointF(new_x, new_y));
}

void ScrollView::ComputeScrollBarsVisibility(const gfx::Size& vp_size,
                                             const gfx::Size& content_size,
                                             bool* horiz_is_shown,
                                             bool* vert_is_shown) const {
  const bool horizontal_enabled =
      horizontal_scroll_bar_mode_ == ScrollBarMode::kEnabled;
  const bool vertical_enabled =
      vertical_scroll_bar_mode_ == ScrollBarMode::kEnabled;
  if (!horizontal_enabled) {
    *horiz_is_shown = false;
    *vert_is_shown =
        vertical_enabled && content_size.height() > vp_size.height();
    return;
  }
  if (!vertical_enabled) {
    *vert_is_shown = false;
    *horiz_is_shown = content_size.width() > vp_size.width();
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
  if (!control) {
    return;
  }
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
  if (!contents_) {
    return;
  }

  const gfx::PointF offset = CurrentOffset();
  if (IsHorizontalScrollEnabled()) {
    int vw = contents_viewport_->width();
    int cw = contents_->width();
    horiz_sb_->Update(vw, cw, offset.x());
  }
  if (IsVerticalScrollEnabled()) {
    int vh = contents_viewport_->height();
    int ch = contents_->height();
    vert_sb_->Update(vh, ch, offset.y());
  }
  GetViewAccessibility().SetScrollXMin(horiz_sb_->GetMinPosition());
  GetViewAccessibility().SetScrollXMax(horiz_sb_->GetMaxPosition());
  GetViewAccessibility().SetScrollYMin(vert_sb_->GetMinPosition());
  GetViewAccessibility().SetScrollYMax(vert_sb_->GetMaxPosition());
}

gfx::PointF ScrollView::CurrentOffset() const {
  return ScrollsWithLayers() ? contents_->layer()->CurrentScrollOffset()
                             : gfx::PointF(-contents_->x(), -contents_->y());
}

void ScrollView::ScrollByOffset(const gfx::PointF& offset) {
  if (!contents_) {
    return;
  }

  gfx::PointF current_offset = CurrentOffset();
  ScrollToOffset(gfx::PointF(current_offset.x() + offset.x(),
                             current_offset.y() + offset.y()));
}

void ScrollView::ScrollToOffset(const gfx::PointF& offset) {
  if (ScrollsWithLayers()) {
    contents_->layer()->SetScrollOffset(offset);
  } else {
    contents_->SetPosition(gfx::Point(-offset.x(), -offset.y()));
  }
  GetViewAccessibility().SetScrollX(offset.x());
  GetViewAccessibility().SetScrollY(offset.y());
  OnScrolled(offset);
}

bool ScrollView::ScrollsWithLayers() const {
  if (!scroll_with_layers_enabled_) {
    return false;
  }
  // Just check for the presence of a layer since it's cheaper than querying the
  // Feature flag each time.
  return contents_viewport_->layer() != nullptr;
}

bool ScrollView::IsHorizontalScrollEnabled() const {
  return horizontal_scroll_bar_mode_ == ScrollBarMode::kHiddenButEnabled ||
         (horizontal_scroll_bar_mode_ == ScrollBarMode::kEnabled &&
          horiz_sb_->GetVisible());
}

bool ScrollView::IsVerticalScrollEnabled() const {
  return vertical_scroll_bar_mode_ == ScrollBarMode::kHiddenButEnabled ||
         (vertical_scroll_bar_mode_ == ScrollBarMode::kEnabled &&
          vert_sb_->GetVisible());
}

void ScrollView::EnableViewportLayer() {
  if (DoesViewportOrScrollViewHaveLayer()) {
    return;
  }
  contents_viewport_->InitializeContentsViewportLayer();
  contents_viewport_->layer()->SetMasksToBounds(true);
  more_content_left_->SetPaintToLayer();
  more_content_top_->SetPaintToLayer();
  more_content_right_->SetPaintToLayer();
  more_content_bottom_->SetPaintToLayer();
  UpdateBackground();
}

void ScrollView::OnLayerScrolled(const gfx::PointF& current_offset,
                                 const cc::ElementId&) {
  OnScrolled(current_offset);
}

void ScrollView::OnScrolled(const gfx::PointF& offset) {
  UpdateOverflowIndicatorVisibility(offset);
  UpdateScrollBarPositions();
  ScrollHeader();

  on_contents_scrolled_.Notify();

  NotifyAccessibilityEvent(ax::mojom::Event::kScrollPositionChanged,
                           /*send_native_event=*/true);
}

void ScrollView::ScrollHeader() {
  if (!header_) {
    return;
  }

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
  if (!draw_border_ || !GetWidget()) {
    return;
  }

  SetBorder(CreateSolidBorder(
      1, GetColorProvider()->GetColor(
             draw_focus_indicator_ ? ui::kColorFocusableBorderFocused
                                   : ui::kColorFocusableBorderUnfocused)));
}

void ScrollView::UpdateBackground() {
  if (!GetWidget()) {
    return;
  }

  const std::optional<SkColor> background_color = GetBackgroundColor();

  auto create_background = [background_color]() {
    return background_color ? CreateSolidBackground(background_color.value())
                            : nullptr;
  };

  SetBackground(create_background());
  // In addition to setting the background of |this|, set the background on
  // the viewport as well. This way if the viewport has a layer
  // SetFillsBoundsOpaquely() is honored.
  contents_viewport_->SetBackground(create_background());
  if (contents_ && ScrollsWithLayers()) {
    contents_->SetBackground(create_background());
    // Contents views may not be aware they need to fill their entire bounds -
    // play it safe here to avoid graphical glitches (https://crbug.com/826472).
    // If there's no solid background, mark the contents view as not filling its
    // bounds opaquely.
    contents_->layer()->SetFillsBoundsOpaquely(!!background_color);
  }
  if (contents_viewport_->layer()) {
    contents_viewport_->layer()->SetFillsBoundsOpaquely(!!background_color);
  }
}

std::optional<SkColor> ScrollView::GetBackgroundColor() const {
  return background_color_id_
             ? GetColorProvider()->GetColor(background_color_id_.value())
             : background_color_;
}

std::optional<ui::ColorId> ScrollView::GetBackgroundThemeColorId() const {
  return background_color_id_;
}

void ScrollView::PositionOverflowIndicators() {
  // TODO(crbug.com/40742414): Use a layout manager to position these.
  const gfx::Rect contents_bounds = GetContentsBounds();
  const int x = contents_bounds.x();
  const int y = contents_bounds.y();
  const int w = contents_bounds.width();
  const int h = contents_bounds.height();

  more_content_left_->SetBoundsRect(
      gfx::Rect(x, y, more_content_left_thickness_, h));
  more_content_top_->SetBoundsRect(
      gfx::Rect(x, y, w, more_content_top_thickness_));
  more_content_right_->SetBoundsRect(
      gfx::Rect(contents_bounds.right() - more_content_right_thickness_, y,
                more_content_right_thickness_, h));
  more_content_bottom_->SetBoundsRect(
      gfx::Rect(x, contents_bounds.bottom() - more_content_bottom_thickness_, w,
                more_content_bottom_thickness_));
}

void ScrollView::UpdateOverflowIndicatorVisibility(const gfx::PointF& offset) {
  SetControlVisibility(more_content_top_.get(),
                       !draw_border_ && !header_ && IsVerticalScrollEnabled() &&
                           offset.y() > vert_sb_->GetMinPosition() &&
                           draw_overflow_indicator_);
  SetControlVisibility(
      more_content_bottom_.get(),
      !draw_border_ && IsVerticalScrollEnabled() && !horiz_sb_->GetVisible() &&
          offset.y() < vert_sb_->GetMaxPosition() && draw_overflow_indicator_);

  SetControlVisibility(more_content_left_.get(),
                       !draw_border_ && IsHorizontalScrollEnabled() &&
                           offset.x() > horiz_sb_->GetMinPosition() &&
                           draw_overflow_indicator_);
  SetControlVisibility(
      more_content_right_.get(),
      !draw_border_ && IsHorizontalScrollEnabled() && !vert_sb_->GetVisible() &&
          offset.x() < horiz_sb_->GetMaxPosition() && draw_overflow_indicator_);
}

View* ScrollView::GetContentsViewportForTest() const {
  return contents_viewport_;
}

BEGIN_METADATA(ScrollView)
ADD_READONLY_PROPERTY_METADATA(int, MinHeight)
ADD_READONLY_PROPERTY_METADATA(int, MaxHeight)
ADD_PROPERTY_METADATA(bool, AllowKeyboardScrolling)
ADD_PROPERTY_METADATA(std::optional<SkColor>, BackgroundColor)
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, BackgroundThemeColorId)
ADD_PROPERTY_METADATA(bool, DrawOverflowIndicator)
ADD_PROPERTY_METADATA(bool, HasFocusIndicator)
ADD_PROPERTY_METADATA(ScrollView::ScrollBarMode, HorizontalScrollBarMode)
ADD_PROPERTY_METADATA(ScrollView::ScrollBarMode, VerticalScrollBarMode)
ADD_PROPERTY_METADATA(bool, TreatAllScrollEventsAsHorizontal)
END_METADATA

// VariableRowHeightScrollHelper ----------------------------------------------

VariableRowHeightScrollHelper::VariableRowHeightScrollHelper(
    Controller* controller)
    : controller_(controller) {}

VariableRowHeightScrollHelper::~VariableRowHeightScrollHelper() = default;

int VariableRowHeightScrollHelper::GetPageScrollIncrement(
    ScrollView* scroll_view,
    bool is_horizontal,
    bool is_positive) {
  if (is_horizontal) {
    return 0;
  }
  // y coordinate is most likely negative.
  int y = abs(scroll_view->contents()->y());
  int vis_height = scroll_view->contents()->parent()->height();
  if (is_positive) {
    // Align the bottom most row to the top of the view.
    int bottom =
        std::min(scroll_view->contents()->height() - 1, y + vis_height);
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
    ScrollView* scroll_view,
    bool is_horizontal,
    bool is_positive) {
  if (is_horizontal) {
    return 0;
  }
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

VariableRowHeightScrollHelper::RowInfo FixedRowHeightScrollHelper::GetRowInfo(
    int y) {
  if (y < top_margin_) {
    return RowInfo(0, top_margin_);
  }
  return RowInfo((y - top_margin_) / row_height_ * row_height_ + top_margin_,
                 row_height_);
}

}  // namespace views
