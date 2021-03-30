// Copyright 2018 The Immersive Web Community Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

let neg = function(vector) {
  return {x : -vector.x, y : -vector.y, z : -vector.z, w : vector.w};
}

let sub = function(lhs, rhs) {
  if(!((lhs.w == 1 && rhs.w == 1) || (lhs.w == 1 && rhs.w == 0) || (lhs.w == 0 && rhs.w == 0)))
    console.error("only point - point, point - line or line - line subtraction is allowed");
  return {x : lhs.x - rhs.x, y : lhs.y - rhs.y, z : lhs.z - rhs.z, w : lhs.w - rhs.w};
}

let add = function(lhs, rhs) {
  if(!((lhs.w == 0 && rhs.w == 1) || (lhs.w == 1 && rhs.w == 0)))
    console.error("only line + point or point + line addition is allowed");

  return {x : lhs.x + rhs.x, y : lhs.y + rhs.y, z : lhs.z + rhs.z, w : lhs.w + rhs.w};
}

let mul = function(vector, scalar) {
  return {x : vector.x * scalar, y : vector.y * scalar, z : vector.z * scalar, w : vector.w};
}

// |matrix| - Float32Array, |input| - point-like dict (must have x, y, z, w)
export function transform_point_by_matrix (matrix, input) {
  return {
    x : matrix[0] * input.x + matrix[4] * input.y + matrix[8] * input.z + matrix[12] * input.w,
    y : matrix[1] * input.x + matrix[5] * input.y + matrix[9] * input.z + matrix[13] * input.w,
    z : matrix[2] * input.x + matrix[6] * input.y + matrix[10] * input.z + matrix[14] * input.w,
    w : matrix[3] * input.x + matrix[7] * input.y + matrix[11] * input.z + matrix[15] * input.w,
  };
}

// |point| - point-like dict (must have x, y, z, w)
let normalize_perspective = function(point) {
  if(point.w == 0 || point.w == 1) return point;

  return {
    x : point.x / point.w,
    y : point.y / point.w,
    z : point.z / point.w,
    w : 1
  };
}

let dotProduct = function(lhs, rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

let crossProduct = function(lhs, rhs) {
  return {
    x : lhs.y * rhs.z - lhs.z * rhs.y,
    y : lhs.z * rhs.x - lhs.x * rhs.z,
    z : lhs.x * rhs.y - lhs.y * rhs.x,
    w : 0
  }
}

let length = function(vector) {
  return Math.sqrt(dotProduct(vector, vector));
}

let normalize = function(vector) {
  const l = length(vector);
  return mul(vector, 1.0/l);
}

let calculateHitMatrix = function(ray_vector, plane_normal, point) {
  // projection of ray_vector onto a plane
  const ray_vector_projection = sub(ray_vector, mul(plane_normal, dotProduct(ray_vector, plane_normal)));

  // new coordinate system axes
  const y = plane_normal;
  const z = normalize(neg(ray_vector_projection));
  const x = normalize(crossProduct(y, z));

  let hitMatrix = new Float32Array(16);

  hitMatrix[0] = x.x;
  hitMatrix[1] = x.y;
  hitMatrix[2] = x.z;
  hitMatrix[3] = 0;

  hitMatrix[4] = y.x;
  hitMatrix[5] = y.y;
  hitMatrix[6] = y.z;
  hitMatrix[7] = 0;

  hitMatrix[8] = z.x;
  hitMatrix[9] = z.y;
  hitMatrix[10] = z.z;
  hitMatrix[11] = 0;

  hitMatrix[12] = point.x;
  hitMatrix[13] = point.y;
  hitMatrix[14] = point.z;
  hitMatrix[15] = 1;

  return hitMatrix;
}

// Single plane hit test - doesn't take into account the plane's polygon
// |frame| - XRFrame, |ray| - XRRay, |plane| - XRPlane, |frameOfReference| - XRSpace
// Returns null if the hit test did not hit the |plane|.
// If the |ray| lies on the |plane|, there are infinite intersection points &
// we will return an object containing the plane only.
// If the |ray| intersects with the |plane|, we will return an object containing
// the distance along the plane as `distance`, the |plane| & |ray| as `plane` and `ray`,
// the intersection point (in |frameOfReference| coordinates as `point`,
// and relative to plane pose as `point_on_plane`),
// hit test pose in |frameOfReference| as `hitMatrix`,
// and |plane|'s pose in |frameOfReference| as `pose_matrix`.
function hitTestPlane(frame, ray, plane, frameOfReference) {
  const plane_pose = frame.getPose(plane.planeSpace, frameOfReference);
  if(!plane_pose) {
    return null;
  }

  const plane_normal = transform_point_by_matrix(
    plane_pose.transform.matrix, {x : 0, y : 1.0, z : 0, w : 0});
  const plane_center = normalize_perspective(
      transform_point_by_matrix(
        plane_pose.transform.matrix, {x : 0, y : 0, z : 0, w : 1.0}));

  const ray_origin = ray.origin;
  const ray_vector = ray.direction;

  const numerator = dotProduct( sub(plane_center, ray_origin), plane_normal);
  const denominator = dotProduct(ray_vector, plane_normal);

  if(denominator < 0.0001 && denominator > -0.0001) {
    // parallel planes
    if(numerator < 0.0001 && numerator > -0.0001) {
      // contained in the plane
      console.debug("Ray contained in the plane", plane);
      return { plane : plane };
    } else {
      // no hit
      console.debug("No hit", plane);
      return null;
    }
  } else {
    // single point of intersection
    const d =  numerator / denominator;
    if(d < 0) {
      // no hit - plane-line intersection exists but not for half-line
      console.debug("No hit", d, plane);
      return null;
    } else {
      const point = add(ray_origin, mul(ray_vector, d));  // hit test point coordinates in frameOfReference

      let point_on_plane = transform_point_by_matrix(plane_pose.transform.inverse.matrix, point); // hit test point coodinates relative to plane pose
      console.assert(Math.abs(point_on_plane.y) < 0.0001, "Incorrect Y coordinate of mapped point");

      let hitMatrix = calculateHitMatrix(ray_vector, plane_normal, point);

      return {
        distance : d,
        plane : plane,
        ray : ray,
        point : point,
        point_on_plane : point_on_plane,
        hitMatrix : hitMatrix,
        pose_matrix : plane_pose.transform.matrix
      };
    }
  }

  console.error("Should never reach here");
  return null;
}

// multiple planes hit test
// |frame| - XRFRame, |ray| - XRRay, |frameOfReference| - XRSpace
export function hitTest(frame, ray, frameOfReference) {
  const planes = frame.detectedPlanes;

  let hit_test_results = [];
  planes.forEach(plane => {
    let result = hitTestPlane(frame, ray, plane, frameOfReference);
    if(result) {
      // throw away results with no intersection with plane
      hit_test_results.push(result);
    }
  });

  // throw away all strange results (ray lies on plane)
  let hit_test_results_with_points = hit_test_results.filter(
    maybe_plane => typeof maybe_plane.point != "undefined");

  // sort results by distance
  hit_test_results_with_points.sort((l, r) => l.distance - r.distance);

  // throw away the ones that don't fall within polygon bounds (except the bottommost plane)
  // convert hittest results to something that the caller expects

  return hit_test_results_with_points;
}

function simplifyPolygon(polygon) {
  let result = [];

  let previous_point = polygon[polygon.length - 1];
  for(let i = 0; i < polygon.length; ++i) {
    const current_point = polygon[i];

    const segment = sub(current_point, previous_point);
    if(length(segment) < 0.001) {
      continue;
    }

    result.push(current_point);
    previous_point = current_point;
  }

  return result;
}

export function extendPolygon(polygon) {
  return polygon.map(vertex => {
    let center_to_vertex_normal = normalize(vertex);
    center_to_vertex_normal.w = 0;
    const addition = mul(center_to_vertex_normal, 0.2);
    return add(vertex, addition);
  });
}

// 2d "cross product" of 3d points lying on a 2d plane with Y = 0
let crossProduct2d = function(lhs, rhs) {
  return lhs.x * rhs.z - lhs.z * rhs.x;
}

// Filters hit test results to keep only the planes for which the used ray falls
// within their polygon. Optionally, we can keep the last horizontal plane that
// was hit.
export function filterHitTestResults(hitTestResults,
                                     keep_last_plane = false,
                                     simplify_planes = false,
                                     use_enlarged_polygon = false) {
  console.assert(!(simplify_planes && use_enlarged_polygon), "Wait that's illegal.")

  let result = hitTestResults.filter(hitTestResult => {

    let polygon = simplify_planes ? simplifyPolygon(hitTestResult.plane.polygon)
                                  : hitTestResult.plane.polygon;

    polygon = use_enlarged_polygon ? hitTestResult.plane.extended_polygon : polygon;

    const hit_test_point = hitTestResult.point_on_plane;

    // Check if the point is on the same side from all the segments:
    // - if yes, then it's in the polygon
    // - if no, then it's outside of the polygon
    // This works only for convex polygons.

    let side = 0; // unknown, 1 = right, 2 = left
    let previous_point = polygon[polygon.length - 1];
    for(let i = 0; i < polygon.length; ++i) {
      const current_point = polygon[i];

      const line_segment = sub(current_point, previous_point);
      const segment_direction = normalize(line_segment);

      const turn_segment = sub(hit_test_point, current_point);
      const turn_direction = normalize(turn_segment);

      const cosine_ray_segment = crossProduct2d(segment_direction, turn_direction);
      if(side == 0) {
        if(cosine_ray_segment > 0) {
          side = 1;
        } else {
          side = 2;
        }
      } else {
        if(cosine_ray_segment > 0 && side == 2) return false;
        if(cosine_ray_segment < 0 && side == 1) return false;
      }

      previous_point = current_point;
    }

    return true;
  });

  if(keep_last_plane && hitTestResults.length > 0) {
    const last_horizontal_plane_result = hitTestResults.slice().reverse().find(
      element => {
        return element.plane.orientation == "Horizontal";
      });

    if(last_horizontal_plane_result
      && result.findIndex(element => element === last_horizontal_plane_result) == -1) {
      result.push(last_horizontal_plane_result);
    }
  }

  return result;
}
