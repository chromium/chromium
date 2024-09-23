// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/shadow_util.h"

#include <map>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
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
                      float corner_radius)
      : CanvasImageSource(CalculateSize(shadows, corner_radius)),
        shadows_(shadows),
        corner_radius_(corner_radius) {
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
    SkRRect r_rect = SkRRect::MakeRectXY(gfx::RectToSkRect(bounds),
                                         corner_radius_, corner_radius_);

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
                            float corner_radius) {
    // The "content" area (the middle tile in the 3x3 grid) is a single pixel.
    gfx::Rect bounds(0, 0, 1, 1);
    // We need enough space to render the full range of blur.
    bounds.Inset(-ShadowValue::GetBlurRegion(shadows));
    // We also need space for the full roundrect corner rounding.
    bounds.Inset(-gfx::Insets(corner_radius));
    return bounds.size();
  }

  const std::vector<ShadowValue> shadows_;

  const float corner_radius_;
};

// A shadow's appearance is determined by its rounded corner radius and shadow
// values. Make these attributes as the key for shadow details.
struct ShadowDetailsKey {
  bool operator==(const ShadowDetailsKey& other) const {
    return (corner_radius == other.corner_radius) && (values == other.values);
  }

  bool operator<(const ShadowDetailsKey& other) const {
    return (corner_radius < other.corner_radius) ||
           ((corner_radius == other.corner_radius) && (values < other.values));
  }
  int corner_radius;
  ShadowValues values;
};

// Map from shadow details key to a cached shadow.
using ShadowDetailsMap = std::map<ShadowDetailsKey, ShadowDetails>;
base::LazyInstance<ShadowDetailsMap>::DestructorAtExit g_shadow_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ShadowDetails::ShadowDetails(const gfx::ShadowValues& values,
                             const gfx::ImageSkia& nine_patch_image)
    : values(values), nine_patch_image(nine_patch_image) {}
ShadowDetails::ShadowDetails(const ShadowDetails& other) = default;
ShadowDetails::~ShadowDetails() {}

const ShadowDetails& ShadowDetails::Get(int elevation,
                                        int corner_radius,
                                        ShadowStyle style) {
  switch (style) {
    case ShadowStyle::kMaterialDesign:
      return Get(corner_radius, ShadowValue::MakeMdShadowValues(elevation));
#if BUILDFLAG(IS_CHROMEOS)
    case ShadowStyle::kChromeOSSystemUI:
      return Get(corner_radius,
                 ShadowValue::MakeChromeOSSystemUIShadowValues(elevation));
#endif
  }
}

const ShadowDetails& ShadowDetails::Get(int elevation,
                                        int radius,
                                        SkColor key_color,
                                        SkColor ambient_color,
                                        ShadowStyle style) {
  switch (style) {
    case ShadowStyle::kMaterialDesign:
      return Get(radius, ShadowValue::MakeMdShadowValues(elevation, key_color,
                                                         ambient_color));
#if BUILDFLAG(IS_CHROMEOS)
    case ShadowStyle::kChromeOSSystemUI:
      return Get(radius, ShadowValue::MakeChromeOSSystemUIShadowValues(
                             elevation, key_color, ambient_color));
#endif
  }
}

const ShadowDetails& ShadowDetails::Get(int radius,
                                        const gfx::ShadowValues& values) {
  ShadowDetailsKey key{radius, values};
  auto iter = g_shadow_cache.Get().find(key);
  if (iter != g_shadow_cache.Get().end()) {
    return iter->second;
  }

  // Evict the details whose ninebox image does not have any shadow owners.
  std::erase_if(g_shadow_cache.Get(), [](auto& pair) {
    return pair.second.nine_patch_image.IsUniquelyOwned();
  });

  auto source =
      std::make_unique<ShadowNineboxSource>(values, key.corner_radius);
  const gfx::Size image_size = source->size();
  auto nine_patch_image = ImageSkia(std::move(source), image_size);
  auto insertion = g_shadow_cache.Get().emplace(
      key, ShadowDetails(values, nine_patch_image));
  DCHECK(insertion.second);
  const std::pair<const ShadowDetailsKey, ShadowDetails>& inserted_item =
      *(insertion.first);
  return inserted_item.second;
}

size_t ShadowDetails::GetDetailsCacheSizeForTest() {
  return g_shadow_cache.Get().size();
}

}  // namespace gfx
