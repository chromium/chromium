// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/rect_conversions.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

namespace gfx {

namespace {

int ToFlooredIntIgnoringError(float f, float error) {
  int rounded = ToRoundedInt(f);
  return std::abs(rounded - f) < error ? rounded : ToFlooredInt(f);
}

int ToCeiledIntIgnoringError(float f, float error) {
  int rounded = ToRoundedInt(f);
  return std::abs(rounded - f) < error ? rounded : ToCeiledInt(f);
}

}  // anonymous namespace

Rect ToEnclosingRect(const RectF& r) {
  int left = ToFlooredInt(r.x());
  int right = r.width() ? ToCeiledInt(r.right()) : left;
  int top = ToFlooredInt(r.y());
  int bottom = r.height() ? ToCeiledInt(r.bottom()) : top;

  Rect result;
  result.SetByBounds(left, top, right, bottom);
  return result;
}

Rect ToEnclosingRectIgnoringError(const RectF& r, float error) {
  int left = ToFlooredIntIgnoringError(r.x(), error);
  int right = r.width() ? ToCeiledIntIgnoringError(r.right(), error) : left;
  int top = ToFlooredIntIgnoringError(r.y(), error);
  int bottom = r.height() ? ToCeiledIntIgnoringError(r.bottom(), error) : top;

  Rect result;
  result.SetByBounds(left, top, right, bottom);
  return result;
}

Rect ToEnclosedRect(const RectF& rect) {
  Rect result;
  result.SetByBounds(ToCeiledInt(rect.x()), ToCeiledInt(rect.y()),
                     ToFlooredInt(rect.right()), ToFlooredInt(rect.bottom()));
  return result;
}

Rect ToNearestRect(const RectF& rect) {
  float float_min_x = rect.x();
  float float_min_y = rect.y();
  float float_max_x = rect.right();
  float float_max_y = rect.bottom();

  int min_x = ToRoundedInt(float_min_x);
  int min_y = ToRoundedInt(float_min_y);
  int max_x = ToRoundedInt(float_max_x);
  int max_y = ToRoundedInt(float_max_y);

  // If these DCHECKs fail, you're using the wrong method, consider using
  // ToEnclosingRect or ToEnclosedRect instead.
  DCHECK(std::abs(min_x - float_min_x) < 0.01f);
  DCHECK(std::abs(min_y - float_min_y) < 0.01f);
  DCHECK(std::abs(max_x - float_max_x) < 0.01f);
  DCHECK(std::abs(max_y - float_max_y) < 0.01f);

  Rect result;
  result.SetByBounds(min_x, min_y, max_x, max_y);

  return result;
}

bool IsNearestRectWithinDistance(const gfx::RectF& rect, float distance) {
  float float_min_x = rect.x();
  float float_min_y = rect.y();
  float float_max_x = rect.right();
  float float_max_y = rect.bottom();

  int min_x = ToRoundedInt(float_min_x);
  int min_y = ToRoundedInt(float_min_y);
  int max_x = ToRoundedInt(float_max_x);
  int max_y = ToRoundedInt(float_max_y);

  return
      (std::abs(min_x - float_min_x) < distance) &&
      (std::abs(min_y - float_min_y) < distance) &&
      (std::abs(max_x - float_max_x) < distance) &&
      (std::abs(max_y - float_max_y) < distance);
}

Rect ToFlooredRectDeprecated(const RectF& rect) {
  return Rect(ToFlooredInt(rect.x()),
              ToFlooredInt(rect.y()),
              ToFlooredInt(rect.width()),
              ToFlooredInt(rect.height()));
}

}  // namespace gfx
