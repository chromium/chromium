// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SEPARATOR_H_
#define UI_VIEWS_CONTROLS_SEPARATOR_H_

#include <optional>

#include "ui/color/color_id.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {

// The Separator class is a view that shows a line used to visually separate
// other views.
class VIEWS_EXPORT Separator : public View {
  METADATA_HEADER(Separator, View)

 public:
  // The separator's thickness in dip.
  static constexpr int kThickness = 1;

  // The separator's orientation, set to `kVertical` by default.
  enum class Orientation { kVertical, kHorizontal };

  Separator();

  Separator(const Separator&) = delete;
  Separator& operator=(const Separator&) = delete;

  ~Separator() override;

  ui::ColorId GetColorId() const;
  void SetColorId(ui::ColorId color_id);

  // Vertical or horizontal extension depending on the orientation. Set to
  // `kThickness` by default.
  int GetPreferredLength() const;
  void SetPreferredLength(int length);

  Orientation GetOrientation() const;
  void SetOrientation(Orientation orientation);

  // Overridden from View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  int preferred_length_ = kThickness;
  ui::ColorId color_id_ = ui::kColorSeparator;
  Orientation orientation_ = Orientation::kVertical;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Separator, View)
VIEW_BUILDER_PROPERTY(ui::ColorId, ColorId)
VIEW_BUILDER_PROPERTY(int, PreferredLength)
VIEW_BUILDER_PROPERTY(Separator::Orientation, Orientation)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Separator)

#endif  // UI_VIEWS_CONTROLS_SEPARATOR_H_
