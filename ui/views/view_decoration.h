// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_DECORATION_H_
#define UI_VIEWS_VIEW_DECORATION_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/decoration/decoration.h"
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace ui::decoration {
class ShadowWithHighlightBorder;
}  // namespace ui::decoration

namespace views {

// Manages the decoration (an optional drop shadow and an optional highlight
// border) for a view. This forces `view` to paint to layer if it's not.
class VIEWS_EXPORT ViewDecoration : public ViewObserver {
 public:
  explicit ViewDecoration(View* view);

  ViewDecoration(const ViewDecoration&) = delete;
  ViewDecoration& operator=(const ViewDecoration&) = delete;

  ~ViewDecoration() override;

  // Configures the drop shadow on `view`.
  void SetShadow(
      int elevation,
      std::optional<ui::decoration::Shadow::Style> style = std::nullopt,
      std::optional<ui::decoration::Shadow::ElevationToColorsMap> color_map =
          std::nullopt);
  void ClearShadow();
  ui::decoration::Shadow* shadow();
  const ui::decoration::Shadow* shadow() const;

  // Configures the highlight border on `view`. Because the highlight border is
  // drawn inside `view`'s bounds, setting a highlight border places the
  // decoration layer in `LayerRegion::kAbove` (whereas a shadow-only decoration
  // is placed in `LayerRegion::kBelow`).
  void SetHighlightBorder(SkColor highlight_color,
                          SkColor border_color,
                          int thickness = 1);
  void ClearHighlightBorder();
  ui::decoration::HighlightBorder* highlight_border();
  const ui::decoration::HighlightBorder* highlight_border() const;

  // Update the corner radius of the view along with the decoration.
  void SetRoundedCornerRadius(int corner_radius);
  void SetRoundedCorners(const gfx::RoundedCornersF& radii);

  ui::Decoration* decoration() { return decoration_.get(); }
  const ui::Decoration* decoration() const { return decoration_.get(); }

 private:
  void MoveLayerToRegion(LayerRegion region);

  // ViewObserver:
  void OnViewLayerBoundsSet(View* view) override;
  void OnViewIsDeleting(View* view) override;

  raw_ptr<View> view_;
  std::unique_ptr<ui::Decoration> decoration_;
  raw_ptr<ui::decoration::ShadowWithHighlightBorder> source_;

  base::ScopedObservation<View, ViewObserver> view_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_DECORATION_H_
