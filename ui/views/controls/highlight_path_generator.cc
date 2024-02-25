// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/highlight_path_generator.h"

#include <algorithm>
#include <utility>

#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

HighlightPathGenerator::HighlightPathGenerator()
    : HighlightPathGenerator(gfx::Insets()) {}

HighlightPathGenerator::HighlightPathGenerator(const gfx::Insets& insets)
    : insets_(insets) {}

HighlightPathGenerator::~HighlightPathGenerator() = default;

// static
void HighlightPathGenerator::Install(
    View* host,
    std::unique_ptr<HighlightPathGenerator> generator) {
  host->SetProperty(kHighlightPathGeneratorKey, std::move(generator));
}

// static
std::optional<gfx::RRectF> HighlightPathGenerator::GetRoundRectForView(
    const View* view) {
  HighlightPathGenerator* path_generator =
      view->GetProperty(kHighlightPathGeneratorKey);
  return path_generator ? path_generator->GetRoundRect(view) : std::nullopt;
}

SkPath HighlightPathGenerator::GetHighlightPath(const View* view) {
  // A rounded rectangle must be supplied if using this default implementation.
  std::optional<gfx::RRectF> round_rect = GetRoundRect(view);
  DCHECK(round_rect);
  return SkPath().addRRect(SkRRect{*round_rect});
}

std::optional<gfx::RRectF> HighlightPathGenerator::GetRoundRect(
    const gfx::RectF& rect) {
  return std::nullopt;
}

std::optional<gfx::RRectF> HighlightPathGenerator::GetRoundRect(
    const View* view) {
  gfx::Rect bounds =
      use_contents_bounds_ ? view->GetContentsBounds() : view->GetLocalBounds();
  bounds.Inset(insets_);
  if (use_mirrored_rect_)
    bounds = view->GetMirroredRect(bounds);
  return GetRoundRect(gfx::RectF(bounds));
}

std::optional<gfx::RRectF> EmptyHighlightPathGenerator::GetRoundRect(
    const gfx::RectF& rect) {
  return gfx::RRectF();
}

void InstallEmptyHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<EmptyHighlightPathGenerator>());
}

std::optional<gfx::RRectF> RectHighlightPathGenerator::GetRoundRect(
    const gfx::RectF& rect) {
  return gfx::RRectF(rect);
}

void InstallRectHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<RectHighlightPathGenerator>());
}

CircleHighlightPathGenerator::CircleHighlightPathGenerator(
    const gfx::Insets& insets)
    : HighlightPathGenerator(insets) {}

std::optional<gfx::RRectF> CircleHighlightPathGenerator::GetRoundRect(
    const gfx::RectF& rect) {
  gfx::RectF bounds = rect;
  const float corner_radius = std::min(bounds.width(), bounds.height()) / 2.f;
  bounds.ClampToCenteredSize(
      gfx::SizeF(corner_radius * 2.f, corner_radius * 2.f));
  return gfx::RRectF(bounds, corner_radius);
}

void InstallCircleHighlightPathGenerator(View* view) {
  InstallCircleHighlightPathGenerator(view, gfx::Insets());
}

void InstallCircleHighlightPathGenerator(View* view,
                                         const gfx::Insets& insets) {
  HighlightPathGenerator::Install(
      view, std::make_unique<CircleHighlightPathGenerator>(insets));
}

std::optional<gfx::RRectF> PillHighlightPathGenerator::GetRoundRect(
    const gfx::RectF& rect) {
  gfx::RectF bounds = rect;
  const float corner_radius = std::min(bounds.width(), bounds.height()) / 2.f;
  return gfx::RRectF(bounds, corner_radius);
}

void InstallPillHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<PillHighlightPathGenerator>());
}

FixedSizeCircleHighlightPathGenerator::FixedSizeCircleHighlightPathGenerator(
    int radius)
    : radius_(radius) {}

std::optional<gfx::RRectF> FixedSizeCircleHighlightPathGenerator::GetRoundRect(
    const gfx::RectF& rect) {
  gfx::RectF bounds = rect;
  bounds.ClampToCenteredSize(gfx::SizeF(radius_ * 2, radius_ * 2));
  return gfx::RRectF(bounds, radius_);
}

void InstallFixedSizeCircleHighlightPathGenerator(View* view, int radius) {
  HighlightPathGenerator::Install(
      view, std::make_unique<FixedSizeCircleHighlightPathGenerator>(radius));
}

RoundRectHighlightPathGenerator::RoundRectHighlightPathGenerator(
    const gfx::Insets& insets,
    int corner_radius)
    : RoundRectHighlightPathGenerator(insets,
                                      gfx::RoundedCornersF(corner_radius)) {}

RoundRectHighlightPathGenerator::RoundRectHighlightPathGenerator(
    const gfx::Insets& insets,
    const gfx::RoundedCornersF& rounded_corners)
    : HighlightPathGenerator(insets), rounded_corners_(rounded_corners) {}

std::optional<gfx::RRectF> RoundRectHighlightPathGenerator::GetRoundRect(
    const gfx::RectF& rect) {
  return gfx::RRectF(rect, rounded_corners_);
}

void InstallRoundRectHighlightPathGenerator(View* view,
                                            const gfx::Insets& insets,
                                            int corner_radius) {
  HighlightPathGenerator::Install(
      view,
      std::make_unique<RoundRectHighlightPathGenerator>(insets, corner_radius));
}

}  // namespace views
