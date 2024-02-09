// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_HIGHLIGHT_PATH_GENERATOR_H_
#define UI_VIEWS_CONTROLS_HIGHLIGHT_PATH_GENERATOR_H_

#include <memory>
#include <optional>

#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/views_export.h"

namespace gfx {
class RRectF;
}

namespace views {

class View;

// HighlightPathGenerators are used to generate its highlight path. This
// highlight path is used to generate the View's focus ring and ink-drop
// effects.
class VIEWS_EXPORT HighlightPathGenerator {
 public:
  HighlightPathGenerator();
  explicit HighlightPathGenerator(const gfx::Insets& insets);
  virtual ~HighlightPathGenerator();

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  static void Install(View* host,
                      std::unique_ptr<HighlightPathGenerator> generator);
  static std::optional<gfx::RRectF> GetRoundRectForView(const View* view);

  // TODO(http://crbug.com/1056490): Deprecate |GetHighlightPath()| in favor of
  // |GetRoundRect()|.
  virtual SkPath GetHighlightPath(const View* view);

  // Optionally returns a gfx::RRectF which contains data for drawing a
  // highlight. Note that |rect| is in the coordinate system of the view.
  // TODO(http://crbug.com/1056490): Once |GetHighlightPath()| is deprecated,
  // make this a pure virtual function and make the return not optional.
  virtual std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect);
  std::optional<gfx::RRectF> GetRoundRect(const View* view);

  void set_use_contents_bounds(bool use_contents_bounds) {
    use_contents_bounds_ = use_contents_bounds;
  }

  void set_use_mirrored_rect(bool use_mirrored_rect) {
    use_mirrored_rect_ = use_mirrored_rect;
  }

 private:
  const gfx::Insets insets_;

  // When set uses the view's content bounds instead of its local bounds.
  // TODO(http://crbug.com/1056490): Investigate removing this and seeing if all
  // ink drops / focus rings should use the content bounds.
  bool use_contents_bounds_ = false;

  // When set uses the mirror rect in RTL. This should not be needed for focus
  // rings paths as they handle RTL themselves.
  // TODO(http://crbug.com/1056490): Investigate moving FocusRing RTL to this
  // class and removing this bool.
  bool use_mirrored_rect_ = false;
};

// Sets a highlight path that is empty. This is used for ink drops that want to
// rely on the size of their created ripples/highlights and not have any
// clipping applied to them.
class VIEWS_EXPORT EmptyHighlightPathGenerator : public HighlightPathGenerator {
 public:
  EmptyHighlightPathGenerator() = default;

  EmptyHighlightPathGenerator(const EmptyHighlightPathGenerator&) = delete;
  EmptyHighlightPathGenerator& operator=(const EmptyHighlightPathGenerator&) =
      delete;

  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override;
};

void VIEWS_EXPORT InstallEmptyHighlightPathGenerator(View* view);

// Sets a rectangular highlight path.
class VIEWS_EXPORT RectHighlightPathGenerator : public HighlightPathGenerator {
 public:
  RectHighlightPathGenerator() = default;

  RectHighlightPathGenerator(const RectHighlightPathGenerator&) = delete;
  RectHighlightPathGenerator& operator=(const RectHighlightPathGenerator&) =
      delete;

  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override;
};

void VIEWS_EXPORT InstallRectHighlightPathGenerator(View* view);

// Sets a centered circular highlight path.
class VIEWS_EXPORT CircleHighlightPathGenerator
    : public HighlightPathGenerator {
 public:
  explicit CircleHighlightPathGenerator(const gfx::Insets& insets);

  CircleHighlightPathGenerator(const CircleHighlightPathGenerator&) = delete;
  CircleHighlightPathGenerator& operator=(const CircleHighlightPathGenerator&) =
      delete;

  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override;
};

void VIEWS_EXPORT InstallCircleHighlightPathGenerator(View* view);
void VIEWS_EXPORT
InstallCircleHighlightPathGenerator(View* view, const gfx::Insets& insets);

// Sets a pill-shaped highlight path.
class VIEWS_EXPORT PillHighlightPathGenerator : public HighlightPathGenerator {
 public:
  PillHighlightPathGenerator() = default;

  PillHighlightPathGenerator(const PillHighlightPathGenerator&) = delete;
  PillHighlightPathGenerator& operator=(const PillHighlightPathGenerator&) =
      delete;

  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override;
};

void VIEWS_EXPORT InstallPillHighlightPathGenerator(View* view);

// Sets a centered fixed-size circular highlight path.
class VIEWS_EXPORT FixedSizeCircleHighlightPathGenerator
    : public HighlightPathGenerator {
 public:
  explicit FixedSizeCircleHighlightPathGenerator(int radius);

  FixedSizeCircleHighlightPathGenerator(
      const FixedSizeCircleHighlightPathGenerator&) = delete;
  FixedSizeCircleHighlightPathGenerator& operator=(
      const FixedSizeCircleHighlightPathGenerator&) = delete;

  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override;

 private:
  const int radius_;
};

void VIEWS_EXPORT InstallFixedSizeCircleHighlightPathGenerator(View* view,
                                                               int radius);

// Sets a rounded rectangle highlight path with optional insets.
class VIEWS_EXPORT RoundRectHighlightPathGenerator
    : public HighlightPathGenerator {
 public:
  RoundRectHighlightPathGenerator(const gfx::Insets& insets, int corner_radius);
  RoundRectHighlightPathGenerator(const gfx::Insets& insets,
                                  const gfx::RoundedCornersF& rounded_corners);

  RoundRectHighlightPathGenerator(const RoundRectHighlightPathGenerator&) =
      delete;
  RoundRectHighlightPathGenerator& operator=(
      const RoundRectHighlightPathGenerator&) = delete;

  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override;

 private:
  const gfx::RoundedCornersF rounded_corners_;
};

void VIEWS_EXPORT
InstallRoundRectHighlightPathGenerator(View* view,
                                       const gfx::Insets& insets,
                                       int corner_radius);

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_HIGHLIGHT_PATH_GENERATOR_H_
