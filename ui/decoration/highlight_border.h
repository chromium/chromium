// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_HIGHLIGHT_BORDER_H_
#define UI_DECORATION_HIGHLIGHT_BORDER_H_

#include <optional>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/decoration/decoration_source.h"
#include "ui/decoration/highlight_border_value.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ui::decoration {

// DecorationSource that frames content with a highlight border: an outer
// `border_color` ring hugging the content from the outside and an inner
// `highlight_color` ring just inside the content edge (see
// HighlightBorderValue).
class COMPONENT_EXPORT(UI_DECORATION) HighlightBorder
    : public DecorationSource {
 public:
  DECLARE_SAFE_CAST_TARGET()

  HighlightBorder(SkColor highlight_color,
                  SkColor border_color,
                  int thickness = 1);

  HighlightBorder(const HighlightBorder&) = delete;
  HighlightBorder& operator=(const HighlightBorder&) = delete;

  ~HighlightBorder() override;

  void SetColors(SkColor highlight_color, SkColor border_color);
  SkColor highlight_color() const { return highlight_color_; }
  SkColor border_color() const { return border_color_; }

  // Content too small to hold rings this thick gets thinner ones instead (the
  // way Shadow lowers its elevation), so this is the thickness drawn whenever
  // the content allows it.
  void SetThickness(int thickness);
  int thickness() const { return thickness_; }

  // DecorationSource:
  std::optional<Details> GetDetails(
      const gfx::Rect& content_bounds,
      const gfx::RoundedCornersF& rounded_corners) override;

 private:
  // Color of the inner ring.
  SkColor highlight_color_;
  // Color of the outer ring.
  SkColor border_color_;
  int thickness_;
};

}  // namespace ui::decoration

#endif  // UI_DECORATION_HIGHLIGHT_BORDER_H_
