// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_DECORATION_SOURCE_H_
#define UI_DECORATION_DECORATION_SOURCE_H_

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/image/image_skia.h"

namespace ui::decoration {

// Generates the nine-patch image for a single kind of decoration (a shadow, a
// highlight border, ...) and specifies how much space that decoration needs
// around the content it frames.
//
// A DecorationSource knows nothing about layers, animations, or compositing;
// Decoration handles all of that. This split allows decorations to be composed
// out of several sources and keeps the layer plumbing implemented and tested
// exactly once.
class DecorationSource : public ui::SafeCastable {
 public:
  // Everything Decoration needs in order to configure the underlying
  // cc::NinePatchLayer.
  //
  // Visual layout of the decoration layer:
  //
  // ┌────────────────────────────────────────────────────────┐
  // │ Decoration Layer (content_bounds outset by `margins`)  │
  // │   ┌────────────────────────────────────────────────┐   │
  // │   │ Enclosed Content Bounds                        │   │
  // │   │   ┌────────────────────────────────────────┐   │   │
  // │   │   │ `occlusion_rect`                       │   │   │
  // │   │   │ (fully occluded by opaque content)     │   │   │
  // │   │   └────────────────────────────────────────┘   │   │
  // │   └────────────────────────────────────────────────┘   │
  // └────────────────────────────────────────────────────────┘
  //
  // The 9-patch assets and insets, cached by Decoration.
  struct Appearance {
    // The 9-patch bitmap used to render the decoration border and corners.
    gfx::ImageSkia nine_patch_image;

    // Insets from the edge of `nine_patch_image` to the stretchable center
    // aperture grid.
    gfx::Insets aperture_insets;

    // Outset margins from the content bounds to the decoration layer bounds.
    // Represents the exterior decoration region (values are typically
    // negative to expand outward).
    gfx::Insets margins;

    bool operator==(const Appearance& other) const {
      return aperture_insets == other.aperture_insets &&
             margins == other.margins &&
             nine_patch_image.BackedBySameObjectAs(other.nine_patch_image);
    }
  };

  struct Details {
    Appearance appearance;

    // Region hidden by the opaque content in content's space.
    gfx::Rect occlusion_rect;

    bool operator==(const Details& other) const = default;
  };

  using OnDetailsChangedCallback = base::RepeatingCallback<void(
      std::optional<base::TimeDelta> cross_fade_duration)>;

  DecorationSource();

  DecorationSource(const DecorationSource&) = delete;
  DecorationSource& operator=(const DecorationSource&) = delete;

  ~DecorationSource() override;

  // Returns the active decoration details for the given content bounds, whose
  // corner radii have already been clamped to fit the content size. Returns
  // std::nullopt if no decoration should be drawn.
  virtual std::optional<Details> GetDetails(
      const gfx::RRectF& content_bounds) = 0;

  void set_details_changed_callback(OnDetailsChangedCallback callback) {
    on_details_changed_callback_ = std::move(callback);
  }

 protected:
  // Subclasses must call this after changing any property that affects
  // GetDetails(). Repaints the decoration, cross-fading the old appearance
  // into the new one over `cross_fade_duration` when set.
  void NotifyDecorationChanged(
      std::optional<base::TimeDelta> cross_fade_duration = std::nullopt);

 private:
  OnDetailsChangedCallback on_details_changed_callback_;
};

}  // namespace ui::decoration

#endif  // UI_DECORATION_DECORATION_SOURCE_H_
