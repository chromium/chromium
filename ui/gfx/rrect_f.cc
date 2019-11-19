// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rrect_f.h"

#include <iomanip>
#include <iostream>

#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace gfx {

// Sets all x radii to x_rad, and all y radii to y_rad. If one of x_rad or
// y_rad are zero, sets ALL radii to zero.
RRectF::RRectF(float x,
               float y,
               float width,
               float height,
               float x_rad,
               float y_rad)
    : skrrect_(SkRRect::MakeRectXY(SkRect::MakeXYWH(x, y, width, height),
                                   x_rad,
                                   y_rad)) {
  if (IsEmpty()) {
    // Make sure that empty rects are created fully empty, not with some
    // non-zero dimensions.
    skrrect_ = SkRRect::MakeEmpty();
  }
}

// Directly sets all four corners.
RRectF::RRectF(float x,
               float y,
               float width,
               float height,
               float upper_left_x,
               float upper_left_y,
               float upper_right_x,
               float upper_right_y,
               float lower_right_x,
               float lower_right_y,
               float lower_left_x,
               float lower_left_y) {
  SkVector radii[4] = {
      {upper_left_x, upper_left_y},
      {upper_right_x, upper_right_y},
      {lower_right_x, lower_right_y},
      {lower_left_x, lower_left_y},
  };
  skrrect_.setRectRadii(SkRect::MakeXYWH(x, y, width, height), radii);
  if (IsEmpty()) {
    // Make sure that empty rects are created fully empty, not with some
    // non-zero dimensions.
    skrrect_ = SkRRect::MakeEmpty();
  }
}

gfx::Vector2dF RRectF::GetSimpleRadii() const {
  DCHECK(GetType() <= Type::kOval);
  SkPoint result = skrrect_.getSimpleRadii();
  return gfx::Vector2dF(result.x(), result.y());
}

float RRectF::GetSimpleRadius() const {
  DCHECK(GetType() <= Type::kSingle);
  SkPoint result = skrrect_.getSimpleRadii();
  return result.x();
}

RRectF::Type RRectF::GetType() const {
  SkPoint rad;
  switch (skrrect_.getType()) {
    case SkRRect::kEmpty_Type:
      return Type::kEmpty;
    case SkRRect::kRect_Type:
      return Type::kRect;
    case SkRRect::kSimple_Type:
      rad = skrrect_.getSimpleRadii();
      if (rad.x() == rad.y()) {
        return Type::kSingle;
      }
      return Type::kSimple;
    case SkRRect::kOval_Type:
      return Type::kOval;
    case SkRRect::kNinePatch_Type:
    case SkRRect::kComplex_Type:
    default:
      return Type::kComplex;
  }
}

gfx::Vector2dF RRectF::GetCornerRadii(Corner corner) const {
  SkPoint result = skrrect_.radii(SkRRect::Corner(corner));
  return gfx::Vector2dF(result.x(), result.y());
}

void RRectF::GetAllRadii(SkVector radii[4]) const {
  // Unfortunately, the only way to get all radii is one at a time.
  radii[SkRRect::kUpperLeft_Corner] =
      skrrect_.radii(SkRRect::kUpperLeft_Corner);
  radii[SkRRect::kUpperRight_Corner] =
      skrrect_.radii(SkRRect::kUpperRight_Corner);
  radii[SkRRect::kLowerRight_Corner] =
      skrrect_.radii(SkRRect::kLowerRight_Corner);
  radii[SkRRect::kLowerLeft_Corner] =
      skrrect_.radii(SkRRect::kLowerLeft_Corner);
}

void RRectF::SetCornerRadii(Corner corner, float x_rad, float y_rad) {
  // Unfortunately, the only way to set this is to create a new SkRRect.
  SkVector radii[4];
  GetAllRadii(radii);
  radii[SkRRect::Corner(corner)] = SkPoint::Make(x_rad, y_rad);
  skrrect_.setRectRadii(skrrect_.rect(), radii);
}

void RRectF::Scale(float x_scale, float y_scale) {
  if (IsEmpty()) {
    // SkRRect doesn't support scaling of empty rects.
    return;
  }
  if (!x_scale || !y_scale) {
    // SkRRect doesn't support scaling TO an empty rect.
    skrrect_ = SkRRect::MakeEmpty();
    return;
  }
  SkMatrix scale = SkMatrix::MakeScale(x_scale, y_scale);
  SkRRect result;
  bool success = skrrect_.transform(scale, &result);
  DCHECK(success);
  skrrect_ = result;
}

void RRectF::Offset(float horizontal, float vertical) {
  skrrect_.offset(horizontal, vertical);
}

const RRectF& RRectF::operator+=(const gfx::Vector2dF& offset) {
  Offset(offset.x(), offset.y());
  return *this;
}

const RRectF& RRectF::operator-=(const gfx::Vector2dF& offset) {
  Offset(-offset.x(), -offset.y());
  return *this;
}

std::string RRectF::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(3);
  ss << rect().origin().x() << "," << rect().origin().y() << " "
     << rect().size().width() << "x" << rect().size().height();
  Type type = this->GetType();
  if (type <= Type::kRect) {
    ss << ", rectangular";
  } else if (type <= Type::kSingle) {
    ss << ", radius " << GetSimpleRadius();
  } else if (type <= Type::kSimple) {
    gfx::Vector2dF radii = GetSimpleRadii();
    ss << ", x_rad " << radii.x() << ", y_rad " << radii.y();
  } else {
    ss << ",";
    const Corner corners[] = {Corner::kUpperLeft, Corner::kUpperRight,
                              Corner::kLowerRight, Corner::kLowerLeft};
    for (const auto& c : corners) {
      auto this_corner = GetCornerRadii(c);
      ss << " [" << this_corner.x() << " " << this_corner.y() << "]";
    }
  }
  return ss.str();
}

namespace {
inline bool AboveTol(float val1, float val2, float tolerance) {
  return (std::abs(val1 - val2) > tolerance);
}
}  // namespace

bool RRectF::ApproximatelyEqual(const RRectF& rect, float tolerance) const {
  if (AboveTol(skrrect_.rect().x(), rect.skrrect_.rect().x(), tolerance) ||
      AboveTol(skrrect_.rect().y(), rect.skrrect_.rect().y(), tolerance) ||
      AboveTol(skrrect_.width(), rect.skrrect_.width(), tolerance) ||
      AboveTol(skrrect_.height(), rect.skrrect_.height(), tolerance))
    return false;
  for (int i = 0; i < 4; i++) {
    SkVector r1 = skrrect_.radii(SkRRect::Corner(i));
    SkVector r2 = rect.skrrect_.radii(SkRRect::Corner(i));
    if (std::abs(r1.x() - r2.x()) > tolerance ||
        std::abs(r1.y() - r2.y()) > tolerance) {
      return false;
    }
  }
  return true;
}

}  // namespace gfx
