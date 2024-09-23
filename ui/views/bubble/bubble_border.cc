// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border.h"

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/bubble/bubble_border_arrow_utils.h"
#include "ui/views/view.h"
#include "ui/wm/core/shadow_controller.h"

namespace views {

namespace {

// GetShadowValues and GetBorderAndShadowFlags cache their results. The shadow
// values depend on the shadow elevation, color and shadow type, so we create a
// tuple to key the cache.
using ShadowCacheKey = std::tuple<int, SkColor, BubbleBorder::Shadow>;

// The shadow type for default shadow colors.
constexpr int kDefaultShadowType = -1;

ui::Shadow::ElevationToColorsMap ShadowElevationToColorsMap(
    BubbleBorder::Shadow shadow,
    const ui::ColorProvider* color_provider) {
  ui::Shadow::ElevationToColorsMap colors_map;
  if (color_provider) {
    switch (shadow) {
      case BubbleBorder::Shadow::STANDARD_SHADOW:
        colors_map[3] = std::make_pair(
            color_provider->GetColor(
                ui::kColorShadowValueKeyShadowElevationThree),
            color_provider->GetColor(
                ui::kColorShadowValueAmbientShadowElevationThree));
        colors_map[16] = std::make_pair(
            color_provider->GetColor(
                ui::kColorShadowValueKeyShadowElevationSixteen),
            color_provider->GetColor(
                ui::kColorShadowValueAmbientShadowElevationSixteen));
        break;
#if BUILDFLAG(IS_CHROMEOS)
      case BubbleBorder::Shadow::CHROMEOS_SYSTEM_UI_SHADOW:
        colors_map =
            wm::ShadowController::GenerateShadowColorsMap(color_provider);
        break;
#endif
      default:
        NOTREACHED_IN_MIGRATION() << "Invalid bubble border shadow type.";
        break;
    }
  }

  const SkColor default_color =
      color_provider ? color_provider->GetColor(ui::kColorShadowBase)
                     : gfx::kPlaceholderColor;
  colors_map[kDefaultShadowType] = std::make_pair(default_color, default_color);
  return colors_map;
}

enum class BubbleArrowPart { kFill, kBorder };

SkPath GetVisibleArrowPath(BubbleBorder::Arrow arrow,
                           const gfx::Rect& bounds,
                           BubbleArrowPart part) {
  constexpr size_t kNumPoints = 4;
  gfx::RectF bounds_f(bounds);
  SkPoint points[kNumPoints];
  switch (GetBubbleArrowSide(arrow)) {
    case BubbleArrowSide::kRight:
      points[0] = {bounds_f.x(), bounds_f.y()};
      points[1] = {bounds_f.right(),
                   bounds_f.y() + BubbleBorder::kVisibleArrowRadius - 1};
      points[2] = {bounds_f.right(),
                   bounds_f.y() + BubbleBorder::kVisibleArrowRadius};
      points[3] = {bounds_f.x(), bounds_f.bottom() - 1};
      break;
    case BubbleArrowSide::kLeft:
      points[0] = {bounds_f.right(), bounds_f.bottom() - 1};
      points[1] = {bounds_f.x(),
                   bounds_f.y() + BubbleBorder::kVisibleArrowRadius};
      points[2] = {bounds_f.x(),
                   bounds_f.y() + BubbleBorder::kVisibleArrowRadius - 1};
      points[3] = {bounds_f.right(), bounds_f.y()};
      break;
    case BubbleArrowSide::kTop:
      points[0] = {bounds_f.x(), bounds_f.bottom()};
      points[1] = {bounds_f.x() + BubbleBorder::kVisibleArrowRadius - 1,
                   bounds_f.y()};
      points[2] = {bounds_f.x() + BubbleBorder::kVisibleArrowRadius,
                   bounds_f.y()};
      points[3] = {bounds_f.right() - 1, bounds_f.bottom()};
      break;
    case BubbleArrowSide::kBottom:
      points[0] = {bounds_f.right() - 1, bounds_f.y()};
      points[1] = {bounds_f.x() + BubbleBorder::kVisibleArrowRadius,
                   bounds_f.bottom()};
      points[2] = {bounds_f.x() + BubbleBorder::kVisibleArrowRadius - 1,
                   bounds_f.bottom()};
      points[3] = {bounds_f.x(), bounds_f.y()};
      break;
  }

  return SkPath::Polygon(points, kNumPoints, part == BubbleArrowPart::kFill);
}

const gfx::ShadowValues& GetShadowValues(
    const ui::ColorProvider* color_provider,
    const std::optional<int>& elevation,
    BubbleBorder::Shadow shadow_type) {
  // If the color provider does not exist the shadow values are being created in
  // order to calculate Insets. In that case the color plays no role so always
  // set those colors to gfx::kPlaceholderColor.

  SkColor color = color_provider
                      ? color_provider->GetColor(ui::kColorShadowBase)
                      : gfx::kPlaceholderColor;

  // The shadows are always the same for any elevation and color combination, so
  // construct them once and cache.
  static base::NoDestructor<std::map<ShadowCacheKey, gfx::ShadowValues>>
      shadow_map;
  ShadowCacheKey key(elevation.value_or(-1), color, shadow_type);

  if (shadow_map->find(key) != shadow_map->end()) {
    return shadow_map->find(key)->second;
  }

  gfx::ShadowValues shadows;
  if (elevation.has_value()) {
    DCHECK_GE(elevation.value(), 0);
    auto shadow_colors_map =
        ShadowElevationToColorsMap(shadow_type, color_provider);
    const auto iter = shadow_colors_map.find(elevation.value());
    const auto key_ambient_colors = (iter != shadow_colors_map.end())
                                        ? iter->second
                                        : shadow_colors_map[kDefaultShadowType];
    switch (shadow_type) {
      case BubbleBorder::Shadow::STANDARD_SHADOW:
        shadows = gfx::ShadowValue::MakeShadowValues(elevation.value(),
                                                     key_ambient_colors.first,
                                                     key_ambient_colors.second);
        break;
#if BUILDFLAG(IS_CHROMEOS)
      case BubbleBorder::CHROMEOS_SYSTEM_UI_SHADOW:
        if (key_ambient_colors.first == key_ambient_colors.second) {
          shadows = gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
              elevation.value(), key_ambient_colors.first);
        } else {
          shadows = gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
              elevation.value(), key_ambient_colors.first,
              key_ambient_colors.second);
        }
        break;
#endif
      default:
        NOTREACHED_IN_MIGRATION() << "Invalid bubble border shadow type";
        break;
    }
  } else {
    constexpr gfx::Vector2d kOffset(0, 2);
    constexpr int kSmallShadowBlur = 4;
    const SkColor small_shadow_color =
        color_provider
            ? color_provider->GetColor(ui::kColorBubbleBorderShadowSmall)
            : gfx::kPlaceholderColor;
    const SkColor large_shadow_color =
        color_provider
            ? color_provider->GetColor(ui::kColorBubbleBorderShadowLarge)
            : gfx::kPlaceholderColor;
    // gfx::ShadowValue counts blur pixels both inside and outside the shape,
    // whereas these blur values only describe the outside portion, hence they
    // must be doubled.
    shadows = gfx::ShadowValues({
        {kOffset, 2 * kSmallShadowBlur, small_shadow_color},
        {kOffset, 2 * BubbleBorder::kShadowBlur, large_shadow_color},
    });
  }

  shadow_map->insert({key, shadows});
  return shadow_map->find(key)->second;
}

bool ShouldDrawStrokeForArgs(const std::optional<bool>& draw_border_stroke,
                             const std::optional<int>& elevation,
                             BubbleBorder::Shadow shadow_type) {
  return draw_border_stroke.value_or(!elevation.has_value() &&
                                     shadow_type != BubbleBorder::NO_SHADOW);
}

const cc::PaintFlags& GetBorderAndShadowFlags(
    const ui::ColorProvider* color_provider,
    const std::optional<int>& elevation,
    BubbleBorder::Shadow shadow_type) {
  // The flags are always the same for any elevation and color combination, so
  // construct them once and cache.
  static base::NoDestructor<std::map<ShadowCacheKey, cc::PaintFlags>> flag_map;
  ShadowCacheKey key(elevation.value_or(-1),
                     color_provider->GetColor(ui::kColorShadowBase),
                     shadow_type);

  if (flag_map->find(key) != flag_map->end())
    return flag_map->find(key)->second;

  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(ui::kColorBubbleBorder));
  flags.setAntiAlias(true);
  flags.setLooper(gfx::CreateShadowDrawLooper(
      GetShadowValues(color_provider, elevation, shadow_type)));
  flag_map->insert({key, flags});
  return flag_map->find(key)->second;
}

template <typename T>
void DrawBorderAndShadowImpl(
    T rect,
    void (cc::PaintCanvas::*draw)(const T&, const cc::PaintFlags&),
    gfx::Canvas* canvas,
    const ui::ColorProvider* color_provider,
    bool draw_stroke = true,
    const std::optional<int>& elevation = std::nullopt,
    BubbleBorder::Shadow shadow_type = BubbleBorder::STANDARD_SHADOW) {
  if (draw_stroke) {
    // Provide a 1 px border outside the bounds.
    constexpr int kBorderStrokeThicknessPx = 1;
    const SkScalar one_pixel =
        SkFloatToScalar(kBorderStrokeThicknessPx / canvas->image_scale());
    rect.outset(one_pixel, one_pixel);
  }

  (canvas->sk_canvas()->*draw)(
      rect, GetBorderAndShadowFlags(color_provider, elevation, shadow_type));
}

}  // namespace

BubbleBorder::BubbleBorder(Arrow arrow, Shadow shadow, ui::ColorId color_id)
    : arrow_(arrow), shadow_(shadow), color_id_(color_id) {
  DCHECK_LT(shadow_, SHADOW_COUNT);
}

BubbleBorder::~BubbleBorder() = default;

// static
gfx::Insets BubbleBorder::GetBorderAndShadowInsets(
    const std::optional<int>& elevation,
    const std::optional<bool>& draw_border_stroke,
    BubbleBorder::Shadow shadow_type) {
  return gfx::Insets(
             ShouldDrawStrokeForArgs(draw_border_stroke, elevation, shadow_type)
                 ? kBorderThicknessDip
                 : 0) -
         gfx::ShadowValue::GetMargin(
             GetShadowValues(nullptr, elevation, shadow_type));
}

void BubbleBorder::SetCornerRadius(int corner_radius) {
  corner_radius_ = corner_radius;
}

void BubbleBorder::SetColor(SkColor color) {
  requested_color_ = color;
  UpdateColor(nullptr);
}

gfx::Rect BubbleBorder::GetBounds(const gfx::Rect& anchor_rect,
                                  const gfx::Size& contents_size) const {
  const gfx::Size size(GetSizeForContentsSize(contents_size));
  // In floating mode, the bounds of the bubble border and the |anchor_rect|
  // have coinciding central points.
  if (arrow_ == FLOAT) {
    gfx::Rect rect(anchor_rect.CenterPoint(), size);
    rect.Offset(gfx::Vector2d(-size.width() / 2, -size.height() / 2));
    return rect;
  }

  // If no arrow is used, in the vertical direction, the bubble is placed below
  // the |anchor_rect| while they have coinciding horizontal centers.
  if (arrow_ == NONE) {
    gfx::Rect rect(anchor_rect.bottom_center(), size);
    rect.Offset(gfx::Vector2d(-size.width() / 2, 0));
    return rect;
  }

  // In all other cases, the used arrow determines the placement of the bubble
  // with respect to the |anchor_rect|.
  gfx::Rect contents_bounds(contents_size);
  // Always apply the border part of the inset before calculating coordinates,
  // that ensures the bubble's border is aligned with the anchor's border.
  // For the purposes of positioning, the border is rounded up to a dip, which
  // may cause misalignment in scale factors greater than 1.
  // TODO(estade): when it becomes possible to provide px bounds instead of
  // dip bounds, fix this.
  const gfx::Insets border_insets(ShouldDrawStroke() ? kBorderThicknessDip : 0);
  const gfx::Insets insets = GetInsets();
  const gfx::Insets shadow_insets = insets - border_insets;
  // TODO(dfried): Collapse border into visible arrow where applicable.
  contents_bounds.Inset(-border_insets);
  DCHECK(!avoid_shadow_overlap_ || !visible_arrow_);

  // If |avoid_shadow_overlap_| is true, the shadow part of the inset is also
  // applied now, to ensure that the shadow itself doesn't overlap the anchor.
  if (avoid_shadow_overlap_)
    contents_bounds.Inset(-shadow_insets);

  // Adjust the contents to align with the arrow. The `anchor_point` is the
  // point on `anchor_rect` to offset from; it is also used as part of the
  // visible arrow calculation if present.
  gfx::Point anchor_point =
      GetArrowAnchorPointFromAnchorRect(arrow_, anchor_rect);

  contents_bounds += GetContentBoundsOffsetToArrowAnchorPoint(
      contents_bounds, arrow_, anchor_point);

  // With NO_SHADOW, there should be further insets, but the same logic is
  // used to position the bubble origin according to |anchor_rect|.
  DCHECK(shadow_ != NO_SHADOW || insets_.has_value() ||
         shadow_insets.IsEmpty() || visible_arrow_);
  if (!avoid_shadow_overlap_)
    contents_bounds.Inset(-shadow_insets);

  // |arrow_offset_| is used to adjust bubbles that would normally be
  // partially offscreen.
  if (is_arrow_on_horizontal(arrow_))
    contents_bounds += gfx::Vector2d(-arrow_offset_, 0);
  else
    contents_bounds += gfx::Vector2d(0, -arrow_offset_);

  // If no visible arrow is shown, return the content bounds.
  if (!visible_arrow_)
    return contents_bounds;

  // Finally, get the needed movement vector of |contents_bounds| to create the
  // space needed to place the visible arrow. adjustments because we don't want
  // the positioning to be altered. Offset by the size of the arrow's inset on
  // each side (only one side will be nonzero) to create space for the visible
  // arrow.
  contents_bounds +=
      GetContentsBoundsOffsetToPlaceVisibleArrow(arrow_, /*include_gap=*/true);

  // We have an anchor point which is appropriate for the arrow type, but
  // when anchoring to a small view it looks better to track from the middle
  // of the view rather than a corner. We may still adjust this point if
  // it's too close to the edge of the bubble (in this case by adjusting the
  // bubble by a few pixels rather than the anchor point).
  const gfx::Point anchor_center = anchor_rect.CenterPoint();
  const gfx::Point contents_center = contents_bounds.CenterPoint();
  if (IsVerticalArrow(arrow_)) {
    const int right_bound =
        contents_bounds.right() -
        (kVisibleArrowBuffer + kVisibleArrowRadius + shadow_insets.right());
    const int left_bound = contents_bounds.x() + kVisibleArrowBuffer +
                           kVisibleArrowRadius + shadow_insets.left();
    if (anchor_point.x() > anchor_center.x() &&
        anchor_center.x() > contents_center.x()) {
      anchor_point.set_x(anchor_center.x());
    } else if (anchor_point.x() > right_bound) {
      anchor_point.set_x(std::max(anchor_rect.x(), right_bound));
    } else if (anchor_point.x() < anchor_center.x() &&
               anchor_center.x() < contents_center.x()) {
      anchor_point.set_x(anchor_center.x());
    } else if (anchor_point.x() < left_bound) {
      anchor_point.set_x(std::min(anchor_rect.right(), left_bound));
    }
    if (anchor_point.x() < left_bound) {
      contents_bounds -= gfx::Vector2d(left_bound - anchor_point.x(), 0);
    } else if (anchor_point.x() > right_bound) {
      contents_bounds += gfx::Vector2d(anchor_point.x() - right_bound, 0);
    }
  } else {
    const int bottom_bound =
        contents_bounds.bottom() -
        (kVisibleArrowBuffer + kVisibleArrowRadius + shadow_insets.bottom());
    const int top_bound = contents_bounds.y() + kVisibleArrowBuffer +
                          kVisibleArrowRadius + shadow_insets.top();
    if (anchor_point.y() > anchor_center.y() &&
        anchor_center.y() > contents_center.y()) {
      anchor_point.set_y(anchor_center.y());
    } else if (anchor_point.y() > bottom_bound) {
      anchor_point.set_y(std::max(anchor_rect.y(), bottom_bound));
    } else if (anchor_point.y() < anchor_center.y() &&
               anchor_center.y() < contents_center.y()) {
      anchor_point.set_y(anchor_center.y());
    } else if (anchor_point.y() < top_bound) {
      anchor_point.set_y(std::min(anchor_rect.bottom(), top_bound));
    }
    if (anchor_point.y() < top_bound) {
      contents_bounds -= gfx::Vector2d(0, top_bound - anchor_point.y());
    } else if (anchor_point.y() > bottom_bound) {
      contents_bounds += gfx::Vector2d(0, anchor_point.y() - bottom_bound);
    }
  }

  CalculateVisibleArrowRect(contents_bounds, anchor_point);

  return contents_bounds;
}

// static
gfx::Vector2d BubbleBorder::GetContentsBoundsOffsetToPlaceVisibleArrow(
    BubbleBorder::Arrow arrow,
    bool include_gap) {
  DCHECK(has_arrow(arrow));

  const gfx::Insets visible_arrow_insets =
      GetVisibleArrowInsets(arrow, include_gap);
  return gfx::Vector2d(
      visible_arrow_insets.left() - visible_arrow_insets.right(),
      visible_arrow_insets.top() - visible_arrow_insets.bottom());
}

void BubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  if (shadow_ == NO_SHADOW) {
    PaintNoShadow(view, canvas);
    return;
  }

  gfx::ScopedCanvas scoped(canvas);
  SkRRect r_rect = GetClientRect(view);
  canvas->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference,
                                 true /*doAntiAlias*/);
  DrawBorderAndShadowImpl(r_rect, &cc::PaintCanvas::drawRRect, canvas,
                          view.GetColorProvider(), ShouldDrawStroke(),
                          md_shadow_elevation_, shadow_);

  if (visible_arrow_)
    PaintVisibleArrow(view, canvas);
}

// static
void BubbleBorder::DrawBorderAndShadow(
    SkRect rect,
    gfx::Canvas* canvas,
    const ui::ColorProvider* color_provider) {
  DrawBorderAndShadowImpl(rect, &cc::PaintCanvas::drawRect, canvas,
                          color_provider);
}

gfx::Insets BubbleBorder::GetInsets() const {
  // Visible arrow is not compatible with OS-drawn shadow or with manual insets.
  DCHECK((!insets_ && shadow_ != NO_SHADOW) || !visible_arrow_);
  if (insets_.has_value()) {
    return insets_.value();
  }
  gfx::Insets insets;

  switch (shadow_) {
    case STANDARD_SHADOW:
#if BUILDFLAG(IS_CHROMEOS)
    case CHROMEOS_SYSTEM_UI_SHADOW:
#endif
      insets = GetBorderAndShadowInsets(md_shadow_elevation_,
                                        draw_border_stroke_, shadow_);
      break;
    default:
      break;
  }

  if (visible_arrow_) {
    const gfx::Insets arrow_insets = GetVisibleArrowInsets(arrow_, false);
    insets = gfx::Insets::TLBR(std::max(insets.top(), arrow_insets.top()),
                               std::max(insets.left(), arrow_insets.left()),
                               std::max(insets.bottom(), arrow_insets.bottom()),
                               std::max(insets.right(), arrow_insets.right()));
  }
  return insets;
}

gfx::Size BubbleBorder::GetMinimumSize() const {
  return GetSizeForContentsSize(gfx::Size());
}

void BubbleBorder::OnViewThemeChanged(View* view) {
  UpdateColor(view);
}

gfx::Size BubbleBorder::GetSizeForContentsSize(
    const gfx::Size& contents_size) const {
  // Enlarge the contents size by the thickness of the border images.
  gfx::Size size(contents_size);
  const gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

bool BubbleBorder::AddArrowToBubbleCornerAndPointTowardsAnchor(
    const gfx::Rect& anchor_rect,
    gfx::Rect& popup_bounds,
    int popup_min_y) {
  // This function should only be called for a visible arrow.
  DCHECK(arrow_ != Arrow::NONE && arrow_ != Arrow::FLOAT);
  CHECK_GE(popup_bounds.y(), popup_min_y);

  // The total size of the arrow in its normal direction.
  const int kVisibleArrowDiamater = 2 * kVisibleArrowRadius;

  // To store the resulting x and y position of the arrow.
  int x_position, y_position;

  // The definition of an vertical arrow is inconsistent.
  if (IsVerticalArrow(arrow_)) {
    // The optimal x position depends on the |arrow_|.
    // For a center-aligned arrow, the optimal position of the arrow points
    // towards the center of the element. If the arrow is right-aligned, it
    // points towards the right edge of the element, and to the left otherwise.
    int x_optimal_position =
        (int{arrow_} & ArrowMask::CENTER)
            ? anchor_rect.CenterPoint().x() - kVisibleArrowRadius
            : ((int{arrow_} & ArrowMask::RIGHT)
                   ? anchor_rect.right() - kVisibleArrowDiamater
                   : anchor_rect.x());

    // The most left position for the arrow is the left edge of the bubble
    // plus the minimum spacing of the arrow from the edge.
    int leftmost_position_on_bubble = popup_bounds.x() + kVisibleArrowBuffer;

    // Analogous, the most right position is the right side minus the diameter
    // of the arrow and the spacing of the arrow from the edge.
    int rightmost_position_on_bubble =
        popup_bounds.right() - kVisibleArrowDiamater - kVisibleArrowBuffer;

    // If the right-most position is smaller than the left-most position, the
    // bubble's width is not sufficient to add an arrow.
    if (leftmost_position_on_bubble > rightmost_position_on_bubble) {
      // Make the arrow invisible because there is not enough space to show it.
      set_visible_arrow(false);
      return false;
    }

    // Make sure the x position is limited to the range defined by the bubble.
    x_position = std::clamp(x_optimal_position, leftmost_position_on_bubble,
                            rightmost_position_on_bubble);

    // Calculate the y position of the arrow to be either on top of below the
    // bubble.
    y_position = (int{arrow_} & ArrowMask::BOTTOM)
                     ? popup_bounds.bottom()
                     : popup_bounds.y() - kVisibleArrowLength;
  } else {
    // Adjust y position of the popup to keep the arrow pointing exactly in
    // the middle of the anchor element, still respecting
    // the |kVisibleArrowBuffer| restrictions.
    int popup_y_upper_bound = anchor_rect.CenterPoint().y() -
                              (kVisibleArrowRadius + kVisibleArrowBuffer);
    int popup_y_lower_bound = anchor_rect.CenterPoint().y() +
                              (kVisibleArrowRadius + kVisibleArrowBuffer) -
                              popup_bounds.height();

    // The popup height is not enough to accommodate the arrow.
    if (popup_y_upper_bound < popup_y_lower_bound) {
      set_visible_arrow(false);
      return false;
    }

    int popup_y_adjusted =
        std::clamp(popup_bounds.y(), popup_y_lower_bound, popup_y_upper_bound);
    popup_bounds.set_y(popup_y_adjusted);

    // For an horizontal arrow, the x position is either the left or the right
    // edge of the bubble, taking the length of the arrow into account.
    x_position = (int{arrow_} & ArrowMask::RIGHT)
                     ? popup_bounds.right()
                     : popup_bounds.x() - kVisibleArrowLength;

    // Calculate the top- and bottom-most position for the bubble.
    int topmost_position_on_bubble = popup_bounds.y() + kVisibleArrowBuffer;

    int bottommost_position_on_bubble =
        popup_bounds.bottom() - kVisibleArrowDiamater - kVisibleArrowBuffer;

    // If the top-most position is below the bottom-most position, the bubble
    // has not enough height to place an arrow.
    if (topmost_position_on_bubble > bottommost_position_on_bubble) {
      // Make the arrow invisible because there is not enough space to show it.
      set_visible_arrow(false);
      return false;
    }

    // Align the arrow with the horizontal center of the element.
    // Here, there is no differentiation between the different positions of a
    // left or right aligned arrow.
    y_position =
        std::clamp(anchor_rect.CenterPoint().y() - kVisibleArrowRadius,
                   topmost_position_on_bubble, bottommost_position_on_bubble);
  }

  visible_arrow_rect_.set_size(GetVisibleArrowSize(arrow_));
  visible_arrow_rect_.set_origin({x_position, y_position});

  // The arrow is positioned around the popup, but the popup is still in its
  // original position and the arrow may overlap the anchor element. To make
  // the whole tandem visually pointing to the anchor it must be shifted
  // in the opposite direction.
  gfx::Vector2d popup_offset =
      GetContentsBoundsOffsetToPlaceVisibleArrow(arrow_, false);
  popup_bounds.set_origin(popup_bounds.origin() + popup_offset);
  visible_arrow_rect_.set_origin(visible_arrow_rect_.origin() + popup_offset);

  // Adjust positions if the shifted popup violates the min y restrictions.
  int min_y_overlay = popup_min_y - popup_bounds.y();
  if (min_y_overlay > 0) {
    // gfx::Vector2d min_y_offset{0, min_y_overlay};
    popup_bounds.Offset(0, min_y_overlay);
    visible_arrow_rect_.Offset(0, min_y_overlay);
  }

  set_visible_arrow(true);
  return true;
}

void BubbleBorder::CalculateVisibleArrowRect(
    const gfx::Rect& contents_bounds,
    const gfx::Point& anchor_point) const {
  const gfx::Insets insets = GetInsets();

  gfx::Point new_origin;
  switch (GetBubbleArrowSide(arrow_)) {
    case BubbleArrowSide::kTop:
      new_origin =
          gfx::Point(anchor_point.x() - kVisibleArrowRadius + 1,
                     contents_bounds.y() + insets.top() - kVisibleArrowLength);
      break;

    case BubbleArrowSide::kBottom:
      new_origin = gfx::Point(anchor_point.x() - kVisibleArrowRadius + 1,
                              contents_bounds.bottom() - insets.bottom());
      break;

    case BubbleArrowSide::kRight:
      new_origin = gfx::Point(contents_bounds.right() - insets.right(),
                              anchor_point.y() - kVisibleArrowRadius + 1);
      break;

    case BubbleArrowSide::kLeft:
      new_origin =
          gfx::Point(contents_bounds.x() + insets.left() - kVisibleArrowLength,
                     anchor_point.y() - kVisibleArrowRadius + 1);
      break;
  }
  visible_arrow_rect_.set_origin(new_origin);
  visible_arrow_rect_.set_size(GetVisibleArrowSize(arrow_));
}

SkRRect BubbleBorder::GetClientRect(const View& view) const {
  gfx::RectF bounds(view.GetLocalBounds());
  bounds.Inset(gfx::InsetsF(GetInsets()));

  // Give precedence to customized rounded corners when non-empty.
  const gfx::RoundedCornersF corners =
      rounded_corners_.IsEmpty() ? gfx::RoundedCornersF(corner_radius_)
                                 : rounded_corners_;

  return SkRRect(gfx::RRectF(bounds, corners));
}

bool BubbleBorder::ShouldDrawStroke() const {
  return ShouldDrawStrokeForArgs(draw_border_stroke_, md_shadow_elevation_,
                                 shadow_);
}

void BubbleBorder::UpdateColor(View* view) {
  const SkColor computed_color =
      view ? view->GetColorProvider()->GetColor(color_id_)
           : gfx::kPlaceholderColor;
  color_ = requested_color_.value_or(computed_color);
  if (view)
    view->SchedulePaint();
}

void BubbleBorder::PaintNoShadow(const View& view, gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->sk_canvas()->clipRRect(GetClientRect(view), SkClipOp::kDifference,
                                 true /*doAntiAlias*/);
  canvas->sk_canvas()->drawColor(SkColors::kTransparent, SkBlendMode::kSrc);
}

void BubbleBorder::PaintVisibleArrow(const View& view, gfx::Canvas* canvas) {
  gfx::Point arrow_origin = visible_arrow_rect_.origin();
  View::ConvertPointFromScreen(&view, &arrow_origin);
  const gfx::Rect arrow_bounds(arrow_origin, visible_arrow_rect_.size());

  // Clip the canvas to a box that's big enough to hold the shadow in every
  // dimension but won't overlap the bubble itself.
  gfx::ScopedCanvas scoped(canvas);
  gfx::Rect clip_rect = arrow_bounds;
  const BubbleArrowSide side = GetBubbleArrowSide(arrow_);
  clip_rect.Inset(gfx::Insets::TLBR(side == BubbleArrowSide::kBottom ? 0 : -2,
                                    side == BubbleArrowSide::kRight ? 0 : -2,
                                    side == BubbleArrowSide::kTop ? 0 : -2,
                                    side == BubbleArrowSide::kLeft ? 0 : -2));
  canvas->ClipRect(clip_rect);

  // Unlike the flags for drawing the border, these are not cached because
  // arrows are currently rare. Should this change over time, we might want to
  // cache these flags, too.
  cc::PaintFlags flags;
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);

  if (ShouldDrawStroke()) {
    flags.setColor(view.GetColorProvider()->GetColor(ui::kColorBubbleBorder));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.2);
    flags.setAntiAlias(true);
    flags.setLooper(gfx::CreateShadowDrawLooper(GetShadowValues(
        view.GetColorProvider(), md_shadow_elevation_, shadow_)));
    canvas->DrawPath(
        GetVisibleArrowPath(arrow_, arrow_bounds, BubbleArrowPart::kBorder),
        flags);
  }

  flags.setColor(color());
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setStrokeWidth(1.0);
  flags.setAntiAlias(true);
  flags.setLooper(gfx::CreateShadowDrawLooper(
      GetShadowValues(view.GetColorProvider(), md_shadow_elevation_, shadow_)));
  canvas->DrawPath(
      GetVisibleArrowPath(arrow_, arrow_bounds, BubbleArrowPart::kFill), flags);
}

void BubbleBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  // Fill the contents with a round-rect region to match the border images.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(border_->color());
  gfx::RectF bounds(view->GetLocalBounds());
  bounds.Inset(gfx::InsetsF(border_->GetInsets()));

  // Give precedence to customized rounded corners when non-empty.
  const gfx::RoundedCornersF corners =
      border_->rounded_corners().IsEmpty()
          ? gfx::RoundedCornersF(border_->corner_radius())
          : border_->rounded_corners();

  canvas->sk_canvas()->drawRRect(SkRRect(gfx::RRectF(bounds, corners)), flags);
}

}  // namespace views
