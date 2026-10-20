// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_SHADOW_WITH_HIGHLIGHT_BORDER_H_
#define UI_DECORATION_SHADOW_WITH_HIGHLIGHT_BORDER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "ui/decoration/decoration_source.h"
#include "ui/decoration/decoration_util.h"
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ui::decoration {

// DecorationSource that draws an optional drop shadow and an optional highlight
// border around the content it frames on a single NinePatchLayer.
//
// Owns and delegates property management to optional Shadow and HighlightBorder
// instances, compositing both effects around the shared content edge in a
// single cached ninebox image.
class COMPONENT_EXPORT(UI_DECORATION) ShadowWithHighlightBorder
    : public DecorationSource {
 public:
  DECLARE_SAFE_CAST_TARGET()

  explicit ShadowWithHighlightBorder(
      std::unique_ptr<Shadow> shadow = nullptr,
      std::unique_ptr<HighlightBorder> highlight_border = nullptr);

  ShadowWithHighlightBorder(const ShadowWithHighlightBorder&) = delete;
  ShadowWithHighlightBorder& operator=(const ShadowWithHighlightBorder&) =
      delete;

  ~ShadowWithHighlightBorder() override;

  void SetShadow(std::unique_ptr<Shadow> shadow);
  Shadow* shadow() { return shadow_.get(); }
  const Shadow* shadow() const { return shadow_.get(); }

  void SetHighlightBorder(std::unique_ptr<HighlightBorder> highlight_border);
  HighlightBorder* highlight_border() { return highlight_border_.get(); }
  const HighlightBorder* highlight_border() const {
    return highlight_border_.get();
  }

  // Computes the combined size-adjusted shadow and highlight border spec for
  // `content_bounds` and `rounded_corners`. Returns std::nullopt when neither
  // effect has anything to draw.
  std::optional<ShadowWithHighlightBorderValue> GetSpec(
      const gfx::Rect& content_bounds,
      const gfx::RoundedCornersF& rounded_corners) const;

  // DecorationSource:
  std::optional<Details> GetDetails(
      const gfx::Rect& content_bounds,
      const gfx::RoundedCornersF& rounded_corners) override;

 private:
  std::unique_ptr<Shadow> shadow_;
  std::unique_ptr<HighlightBorder> highlight_border_;
};

}  // namespace ui::decoration

#endif  // UI_DECORATION_SHADOW_WITH_HIGHLIGHT_BORDER_H_
