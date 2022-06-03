// Copyright 2012 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/r2rect.h"

#include <iosfwd>

#include "s2/r1interval.h"
#include "s2/r2.h"

R2Rect R2Rect::FromCenterSize(R2Point const& center, R2Point const& size) {
  return R2Rect(
      R1Interval(center.x() - 0.5 * size.x(), center.x() + 0.5 * size.x()),
      R1Interval(center.y() - 0.5 * size.y(), center.y() + 0.5 * size.y()));
}

R2Rect R2Rect::FromPointPair(R2Point const& p1, R2Point const& p2) {
  return R2Rect(R1Interval::FromPointPair(p1.x(), p2.x()),
                R1Interval::FromPointPair(p1.y(), p2.y()));
}

bool R2Rect::Contains(R2Rect const& other) const {
  return x().Contains(other.x()) && y().Contains(other.y());
}

bool R2Rect::InteriorContains(R2Rect const& other) const {
  return x().InteriorContains(other.x()) && y().InteriorContains(other.y());
}

bool R2Rect::Intersects(R2Rect const& other) const {
  return x().Intersects(other.x()) && y().Intersects(other.y());
}

bool R2Rect::InteriorIntersects(R2Rect const& other) const {
  return x().InteriorIntersects(other.x()) && y().InteriorIntersects(other.y());
}

void R2Rect::AddPoint(R2Point const& p) {
  bounds_[0].AddPoint(p[0]);
  bounds_[1].AddPoint(p[1]);
}

void R2Rect::AddRect(R2Rect const& other) {
  bounds_[0].AddInterval(other[0]);
  bounds_[1].AddInterval(other[1]);
}

R2Point R2Rect::Project(R2Point const& p) const {
  return R2Point(x().Project(p.x()), y().Project(p.y()));
}

R2Rect R2Rect::Expanded(R2Point const& margin) const {
  R1Interval xx = x().Expanded(margin.x());
  R1Interval yy = y().Expanded(margin.y());
  if (xx.is_empty() || yy.is_empty())
    return Empty();
  return R2Rect(xx, yy);
}

R2Rect R2Rect::Union(R2Rect const& other) const {
  return R2Rect(x().Union(other.x()), y().Union(other.y()));
}

R2Rect R2Rect::Intersection(R2Rect const& other) const {
  R1Interval xx = x().Intersection(other.x());
  R1Interval yy = y().Intersection(other.y());
  if (xx.is_empty() || yy.is_empty())
    return Empty();
  return R2Rect(xx, yy);
}

bool R2Rect::ApproxEquals(R2Rect const& other, double max_error) const {
  return (x().ApproxEquals(other.x(), max_error) &&
          y().ApproxEquals(other.y(), max_error));
}

std::ostream& operator<<(std::ostream& os, R2Rect const& r) {
  return os << "[Lo" << r.lo() << ", Hi" << r.hi() << "]";
}
