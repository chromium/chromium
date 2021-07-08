// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/gfx/path_mac.h"

#include <ostream>

#import <Cocoa/Cocoa.h>

#include "base/cxx17_backports.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace {

// Convert a quadratic bezier curve to a cubic bezier curve. Based on the
// implementation of the private SkConvertQuadToCubic method inside Skia.
void ConvertQuadToCubicBezier(NSPoint quad[3], NSPoint cubic[4]) {
  // The resultant cubic will have the same endpoints.
  cubic[0] = quad[0];
  cubic[3] = quad[2];

  const double scale = 2.0 / 3.0;

  cubic[1].x = quad[0].x + scale * (quad[1].x - quad[0].x);
  cubic[1].y = quad[0].y + scale * (quad[1].y - quad[0].y);

  cubic[2].x = quad[2].x + scale * (quad[1].x - quad[2].x);
  cubic[2].y = quad[2].y + scale * (quad[1].y - quad[2].y);
}

}  // namespace

namespace gfx {

NSBezierPath* CreateNSBezierPathFromSkPath(const SkPath& path) {
  NSBezierPath* result = [NSBezierPath bezierPath];
  SkPath::RawIter iter(path);
  SkPoint sk_points[4] = {{0.0}};
  SkPath::Verb verb;
  NSPoint points[4];
  while ((verb = iter.next(sk_points)) != SkPath::kDone_Verb) {
    for (size_t i = 0; i < base::size(points); i++)
      points[i] = NSMakePoint(sk_points[i].x(), sk_points[i].y());

    switch (verb) {
      case SkPath::kMove_Verb: {
        [result moveToPoint:points[0]];
        break;
      }
      case SkPath::kLine_Verb: {
        DCHECK(NSEqualPoints([result currentPoint], points[0]));
        [result lineToPoint:points[1]];
        break;
      }
      case SkPath::kQuad_Verb: {
        DCHECK(NSEqualPoints([result currentPoint], points[0]));
        NSPoint quad[] = {points[0], points[1], points[2]};
        // NSBezierPath does not support quadratic bezier curves. Hence convert
        // to cubic bezier curve.
        ConvertQuadToCubicBezier(quad, points);
        [result curveToPoint:points[3]
               controlPoint1:points[1]
               controlPoint2:points[2]];
        break;
      }
      case SkPath::kConic_Verb: {
        DCHECK(NSEqualPoints([result currentPoint], points[0]));
        // Approximate with quads. Use two for now, increase if more precision
        // is needed.
        const size_t kSubdivisionLevels = 1;
        const size_t kQuadCount = 1 << kSubdivisionLevels;
        // The quads will share endpoints, so we need one more point than twice
        // the number of quads.
        const size_t kPointCount = 1 + 2 * kQuadCount;
        SkPoint quads[kPointCount];
        SkPath::ConvertConicToQuads(sk_points[0], sk_points[1], sk_points[2],
                                    iter.conicWeight(), quads,
                                    kSubdivisionLevels);
        NSPoint ns_quads[kPointCount];
        for (size_t i = 0; i < kPointCount; i++)
          ns_quads[i] = NSMakePoint(quads[i].x(), quads[i].y());

        for (size_t i = 0; i < kQuadCount; i++) {
          NSPoint quad[] = {ns_quads[2 * i], ns_quads[2 * i + 1],
                            ns_quads[2 * i + 2]};
          ConvertQuadToCubicBezier(quad, points);
          DCHECK(NSEqualPoints([result currentPoint], points[0]));
          [result curveToPoint:points[3]
                 controlPoint1:points[1]
                 controlPoint2:points[2]];
        }
        break;
      }
      case SkPath::kCubic_Verb: {
        DCHECK(NSEqualPoints([result currentPoint], points[0]));
        [result curveToPoint:points[3]
               controlPoint1:points[1]
               controlPoint2:points[2]];
        break;
      }
      case SkPath::kClose_Verb: {
        [result closePath];
        break;
      }
      default: { NOTREACHED(); }
    }
  }

  // Set up the fill type.
  switch (path.getFillType()) {
    case SkPathFillType::kWinding:
      [result setWindingRule:NSNonZeroWindingRule];
      break;
    case SkPathFillType::kEvenOdd:
      [result setWindingRule:NSEvenOddWindingRule];
      break;
    case SkPathFillType::kInverseWinding:
    case SkPathFillType::kInverseEvenOdd:
      NOTREACHED() << "NSBezierCurve does not support inverse fill types.";
      break;
  }

  return result;
}

}  // namespace gfx
