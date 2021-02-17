// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/shadow_util.h"

#include <map>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/skia_util.h"

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

  DISALLOW_COPY_AND_ASSIGN(ShadowNineboxSource);
};

// Map from elevation/corner radius pair to a cached shadow.
using ShadowDetailsMap = std::map<std::pair<int, int>, ShadowDetails>;
base::LazyInstance<ShadowDetailsMap>::DestructorAtExit g_shadow_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ShadowDetails::ShadowDetails() {}
ShadowDetails::ShadowDetails(const ShadowDetails& other) = default;
ShadowDetails::~ShadowDetails() {}

const ShadowDetails& ShadowDetails::Get(int elevation, int corner_radius) {
  auto iter =
      g_shadow_cache.Get().find(std::make_pair(elevation, corner_radius));
  if (iter != g_shadow_cache.Get().end())
    return iter->second;

  auto insertion = g_shadow_cache.Get().emplace(
      std::make_pair(elevation, corner_radius), ShadowDetails());
  DCHECK(insertion.second);
  ShadowDetails* shadow = &insertion.first->second;
  shadow->values = ShadowValue::MakeMdShadowValues(elevation);
  auto* source = new ShadowNineboxSource(shadow->values, corner_radius);
  shadow->ninebox_image = ImageSkia(base::WrapUnique(source), source->size());
  return *shadow;
}

}  // namespace gfx
