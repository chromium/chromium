// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor_extra/decoration_util.h"

#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"

namespace gfx {
namespace {

// Creates an image with the given shadows painted around a round rect with
// the given corner radius. The image will be just large enough to paint the
// shadows appropriately with a 1px square region reserved for "content".
class ShadowNineboxSource : public CanvasImageSource {
 public:
  ShadowNineboxSource(const std::vector<ShadowValue>& shadows,
                      gfx::RoundedCornersF& rounded_corners)
      : CanvasImageSource(CalculateSize(shadows, rounded_corners)),
        shadows_(shadows),
        rounded_corners_(rounded_corners) {
    DCHECK(!shadows.empty());
  }

  ShadowNineboxSource(const ShadowNineboxSource&) = delete;
  ShadowNineboxSource& operator=(const ShadowNineboxSource&) = delete;

  ~ShadowNineboxSource() override {}

  // CanvasImageSource overrides:
  void Draw(Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setLooper(CreateShadowDrawLooper(shadows_));
    Insets insets = -ShadowValue::GetMargin(shadows_);
    gfx::Rect bounds(size());
    bounds.Inset(insets);

    SkVector radii[4] = {
        {rounded_corners_.upper_left(), rounded_corners_.upper_left()},
        {rounded_corners_.upper_right(), rounded_corners_.upper_right()},
        {rounded_corners_.lower_right(), rounded_corners_.lower_right()},
        {rounded_corners_.lower_left(), rounded_corners_.lower_left()}};
    SkRRect r_rect;
    r_rect.setRectRadii(gfx::RectToSkRect(bounds), radii);

    // Clip out the center so it's not painted with the shadow.
    canvas->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference, true);
    // Clipping alone is not enough --- due to anti aliasing there will still be
    // some of the fill color in the rounded corners. We must make the fill
    // color transparent.
    flags.setColor(SK_ColorTRANSPARENT);
    canvas->sk_canvas()->drawRRect(r_rect, flags);
  }

 private:
  static Size CalculateSize(const std::vector<ShadowValue>& shadows,
                            const gfx::RoundedCornersF& rounded_corners) {
    // The "content" area (the middle tile in the 3x3 grid) is a single pixel.
    gfx::Rect bounds(0, 0, 1, 1);

    // Add enough space to render the full range of blur and the corner
    // rounding.
    bounds.Inset(
        -ShadowDetails::GetNineboxApertureInsets(shadows, rounded_corners));
    return bounds.size();
  }

  const std::vector<ShadowValue> shadows_;

  const gfx::RoundedCornersF rounded_corners_;
};

// A shadow's appearance is determined by its rounded corner radius and shadow
// values. Make these attributes as the key for shadow details.
struct ShadowDetailsKey {
  bool operator==(const ShadowDetailsKey& other) const {
    return (rounded_corners == other.rounded_corners) &&
           (values == other.values);
  }

  bool operator<(const ShadowDetailsKey& other) const {
    if (rounded_corners != other.rounded_corners) {
      return gfx::RoundedCornersF::Compare(rounded_corners,
                                           other.rounded_corners);
    }
    return values < other.values;
  }

  gfx::RoundedCornersF rounded_corners;
  ShadowValues values;
};

// Map from shadow details key to a cached shadow.
using ShadowDetailsMap = base::flat_map<ShadowDetailsKey, ShadowDetails>;
base::LazyInstance<ShadowDetailsMap>::DestructorAtExit g_shadow_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ShadowDetails::ShadowDetails(const gfx::ShadowValues& values,
                             const gfx::ImageSkia& nine_patch_image)
    : values(values), nine_patch_image(nine_patch_image) {}

ShadowDetails::ShadowDetails(const ShadowDetails& other) = default;
ShadowDetails& ShadowDetails::operator=(const ShadowDetails& other) = default;

ShadowDetails::ShadowDetails(ShadowDetails&& other) = default;
ShadowDetails& ShadowDetails::operator=(ShadowDetails&& other) = default;

ShadowDetails::~ShadowDetails() = default;

bool ShadowDetails::operator==(const ShadowDetails& other) const {
  return values == other.values &&
         nine_patch_image.BackedBySameObjectAs(other.nine_patch_image);
}

const ShadowDetails& ShadowDetails::Get(
    int elevation,
    const gfx::RoundedCornersF& rounded_corners,
    bool is_pill_shaped) {
  return Get(rounded_corners, ShadowValue::MakeMdShadowValues(
                                  elevation, SK_ColorBLACK, is_pill_shaped));
}

const ShadowDetails& ShadowDetails::Get(
    const gfx::RoundedCornersF& rounded_corners,
    const gfx::ShadowValues& values) {
  ShadowDetailsKey key{rounded_corners, values};
  auto iter = g_shadow_cache.Get().find(key);
  if (iter != g_shadow_cache.Get().end()) {
    return iter->second;
  }

  // Evict the details whose ninebox image does not have any shadow owners.
  base::EraseIf(g_shadow_cache.Get(), [](auto& pair) {
    return pair.second.nine_patch_image.IsUniquelyOwned();
  });

  auto source =
      std::make_unique<ShadowNineboxSource>(values, key.rounded_corners);
  const gfx::Size image_size = source->size();
  auto nine_patch_image = ImageSkia(std::move(source), image_size);
  auto [inserted_iter, success] =
      g_shadow_cache.Get().try_emplace(key, values, nine_patch_image);
  DCHECK(success);
  return inserted_iter->second;
}

// static
gfx::Insets ShadowDetails::GetNineboxApertureInsets(
    const gfx::ShadowValues& shadows,
    const gfx::RoundedCornersF& rounded_corners) {
  DCHECK(!shadows.empty());

  // We need enough space to render the full range of blur and the corner
  // rounding.
  const gfx::Insets blur_region = ShadowValue::GetBlurRegion(shadows);
  const bool is_pill_shaped = shadows.front().is_pill_shaped();
#if DCHECK_IS_ON()
  // `is_pill_shaped` describes the shape of the content around which the
  // shadows are drawn, so their values must match.
  for (const auto& shadow : shadows) {
    DCHECK_EQ(is_pill_shaped, shadow.is_pill_shaped());
  }
#endif  // DCHECK_IS_ON()
  const gfx::Insets corner_insets = GetInsetsForRoundedCorners(rounded_corners);
  if (!is_pill_shaped) {
    return blur_region + corner_insets;
  }

  // For pill shaped content, instead of allocating space separately for blur
  // and rounded corners, we take advantage of the fact that blur propagates
  // perpendicular to the edge. The inner blur can thus be drawn within the
  // space already occupied by the corner's curvature.
  //
  // This produces a slightly lighter shadow, but is necessary to produce an
  // image for PillShaped shadow that can be represented as non-overlapping
  // patches in NinePatchLayer.
  //
  // TODO(crbug.com/516866009) Ideally, we should use the same image for
  // pilled vs non-pilled content. Investigate why different shadows are
  // generated.
  const gfx::Insets margins = ShadowValue::GetMargin(shadows);
  const gfx::Insets outer_blur = -margins;
  const gfx::Insets inner_blur = blur_region - outer_blur;
  return gfx::Insets::TLBR(
      outer_blur.top() + std::max(inner_blur.top(), corner_insets.top()),
      outer_blur.left() + std::max(inner_blur.left(), corner_insets.left()),
      outer_blur.bottom() +
          std::max(inner_blur.bottom(), corner_insets.bottom()),
      outer_blur.right() + std::max(inner_blur.right(), corner_insets.right()));
}

gfx::Insets ShadowDetails::GetInsetsForRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  return gfx::Insets::TLBR(
      base::ClampRound(std::max(rounded_corners.upper_left(),
                                rounded_corners.upper_right())),
      base::ClampRound(
          std::max(rounded_corners.upper_left(), rounded_corners.lower_left())),
      base::ClampRound(std::max(rounded_corners.lower_left(),
                                rounded_corners.lower_right())),
      base::ClampRound(std::max(rounded_corners.upper_right(),
                                rounded_corners.lower_right())));
}

size_t ShadowDetails::GetDetailsCacheSizeForTest() {
  return g_shadow_cache.Get().size();
}

}  // namespace gfx
