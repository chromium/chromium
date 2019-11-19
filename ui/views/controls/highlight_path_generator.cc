// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/highlight_path_generator.h"

#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

HighlightPathGenerator::~HighlightPathGenerator() = default;

void HighlightPathGenerator::Install(
    View* host,
    std::unique_ptr<HighlightPathGenerator> generator) {
  host->SetProperty(kHighlightPathGeneratorKey, generator.release());
}

SkPath RectHighlightPathGenerator::GetHighlightPath(const View* view) {
  return SkPath().addRect(gfx::RectToSkRect(view->GetLocalBounds()));
}

void InstallRectHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<RectHighlightPathGenerator>());
}

SkPath CircleHighlightPathGenerator::GetHighlightPath(const View* view) {
  const SkRect rect = gfx::RectToSkRect(view->GetLocalBounds());
  const SkScalar radius = SkScalarHalf(std::min(rect.width(), rect.height()));

  return SkPath().addCircle(rect.centerX(), rect.centerY(), radius);
}

void InstallCircleHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<CircleHighlightPathGenerator>());
}

SkPath PillHighlightPathGenerator::GetHighlightPath(const View* view) {
  const SkRect rect = gfx::RectToSkRect(view->GetLocalBounds());
  const SkScalar radius = SkScalarHalf(std::min(rect.width(), rect.height()));

  return SkPath().addRoundRect(gfx::RectToSkRect(view->GetLocalBounds()),
                               radius, radius);
}

void InstallPillHighlightPathGenerator(View* view) {
  HighlightPathGenerator::Install(
      view, std::make_unique<PillHighlightPathGenerator>());
}

}  // namespace views
