// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TRIANGLE_F_H_
#define UI_GFX_GEOMETRY_TRIANGLE_F_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {

COMPONENT_EXPORT(GEOMETRY)
bool PointIsInTriangle(const PointF& point,
                       const PointF& r1,
                       const PointF& r2,
                       const PointF& r3);
}

#endif  // UI_GFX_GEOMETRY_TRIANGLE_F_H_