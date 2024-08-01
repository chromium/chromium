// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/transform_operation.h"

#include <algorithm>
#include <limits>
#include <numbers>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "base/numerics/ranges.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/transform_operations.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace {
const SkScalar kAngleEpsilon = 1e-4f;
}

namespace gfx {

bool TransformOperation::IsIdentity() const {
  if (type == TRANSFORM_OPERATION_ROTATE) {
    // We can't use matrix.IsIdentity() because rotate(n*360) is not identity,
    // but the matrix is identity.
    return rotate.angle == 0 ||
           // Rotation with zero length axis is treated as identity.
           (rotate.axis.x == 0 && rotate.axis.y == 0 && rotate.axis.z == 0);
  }
  return matrix.IsIdentity();
}

static bool IsOperationIdentity(const TransformOperation* operation) {
  return !operation || operation->IsIdentity();
}

static bool ShareSameAxis(const TransformOperation* from,
                          bool is_identity_from,
                          const TransformOperation* to,
                          bool is_identity_to,
                          SkScalar* axis_x,
                          SkScalar* axis_y,
                          SkScalar* axis_z,
                          SkScalar* angle_from) {
  DCHECK_EQ(is_identity_from, IsOperationIdentity(from));
  DCHECK_EQ(is_identity_to, IsOperationIdentity(to));
  DCHECK(!is_identity_from || !is_identity_to);

  if (is_identity_from && !is_identity_to) {
    *axis_x = to->rotate.axis.x;
    *axis_y = to->rotate.axis.y;
    *axis_z = to->rotate.axis.z;
    *angle_from = 0;
    return true;
  }

  if (!is_identity_from && is_identity_to) {
    *axis_x = from->rotate.axis.x;
    *axis_y = from->rotate.axis.y;
    *axis_z = from->rotate.axis.z;
    *angle_from = from->rotate.angle;
    return true;
  }

  SkScalar length_2 = from->rotate.axis.x * from->rotate.axis.x +
                      from->rotate.axis.y * from->rotate.axis.y +
                      from->rotate.axis.z * from->rotate.axis.z;
  SkScalar other_length_2 = to->rotate.axis.x * to->rotate.axis.x +
                            to->rotate.axis.y * to->rotate.axis.y +
                            to->rotate.axis.z * to->rotate.axis.z;

  if (length_2 <= kAngleEpsilon || other_length_2 <= kAngleEpsilon)
    return false;

  SkScalar dot = to->rotate.axis.x * from->rotate.axis.x +
                 to->rotate.axis.y * from->rotate.axis.y +
                 to->rotate.axis.z * from->rotate.axis.z;
  SkScalar error =
      SkScalarAbs(SK_Scalar1 - (dot * dot) / (length_2 * other_length_2));
  bool result = error < kAngleEpsilon;
  if (result) {
    *axis_x = to->rotate.axis.x;
    *axis_y = to->rotate.axis.y;
    *axis_z = to->rotate.axis.z;
    // If the axes are pointing in opposite directions, we need to reverse
    // the angle.
    *angle_from = dot > 0 ? from->rotate.angle : -from->rotate.angle;
  }
  return result;
}

static SkScalar BlendSkScalars(SkScalar from, SkScalar to, SkScalar progress) {
  return from * (1 - progress) + to * progress;
}

void TransformOperation::Bake() {
  matrix.MakeIdentity();
  switch (type) {
    case TransformOperation::TRANSFORM_OPERATION_TRANSLATE:
      matrix.Translate3d(translate.x, translate.y, translate.z);
      break;
    case TransformOperation::TRANSFORM_OPERATION_ROTATE:
      matrix.RotateAbout(
          gfx::Vector3dF(rotate.axis.x, rotate.axis.y, rotate.axis.z),
          rotate.angle);
      break;
    case TransformOperation::TRANSFORM_OPERATION_SCALE:
      matrix.Scale3d(scale.x, scale.y, scale.z);
      break;
    case TransformOperation::TRANSFORM_OPERATION_SKEWX:
    case TransformOperation::TRANSFORM_OPERATION_SKEWY:
    case TransformOperation::TRANSFORM_OPERATION_SKEW:
      matrix.Skew(skew.x, skew.y);
      break;
    case TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE: {
      Transform m;
      m.set_rc(3, 2, perspective_m43);
      matrix.PreConcat(m);
      break;
    }
    case TransformOperation::TRANSFORM_OPERATION_MATRIX:
    case TransformOperation::TRANSFORM_OPERATION_IDENTITY:
      break;
  }
}

bool TransformOperation::ApproximatelyEqual(const TransformOperation& other,
                                            SkScalar tolerance) const {
  DCHECK_LE(0, tolerance);
  if (type != other.type)
    return false;
  switch (type) {
    case TransformOperation::TRANSFORM_OPERATION_TRANSLATE:
      return base::IsApproximatelyEqual(translate.x, other.translate.x,
                                        tolerance) &&
             base::IsApproximatelyEqual(translate.y, other.translate.y,
                                        tolerance) &&
             base::IsApproximatelyEqual(translate.z, other.translate.z,
                                        tolerance);
    case TransformOperation::TRANSFORM_OPERATION_ROTATE:
      return base::IsApproximatelyEqual(rotate.axis.x, other.rotate.axis.x,
                                        tolerance) &&
             base::IsApproximatelyEqual(rotate.axis.y, other.rotate.axis.y,
                                        tolerance) &&
             base::IsApproximatelyEqual(rotate.axis.z, other.rotate.axis.z,
                                        tolerance) &&
             base::IsApproximatelyEqual(rotate.angle, other.rotate.angle,
                                        tolerance);
    case TransformOperation::TRANSFORM_OPERATION_SCALE:
      return base::IsApproximatelyEqual(scale.x, other.scale.x, tolerance) &&
             base::IsApproximatelyEqual(scale.y, other.scale.y, tolerance) &&
             base::IsApproximatelyEqual(scale.z, other.scale.z, tolerance);
    case TransformOperation::TRANSFORM_OPERATION_SKEWX:
    case TransformOperation::TRANSFORM_OPERATION_SKEWY:
    case TransformOperation::TRANSFORM_OPERATION_SKEW:
      return base::IsApproximatelyEqual(skew.x, other.skew.x, tolerance) &&
             base::IsApproximatelyEqual(skew.y, other.skew.y, tolerance);
    case TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE:
      return base::IsApproximatelyEqual(perspective_m43, other.perspective_m43,
                                        tolerance);
    case TransformOperation::TRANSFORM_OPERATION_MATRIX:
      return matrix.ApproximatelyEqual(other.matrix, tolerance);
    case TransformOperation::TRANSFORM_OPERATION_IDENTITY:
      return other.matrix.IsIdentity();
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool TransformOperation::BlendTransformOperations(
    const TransformOperation* from,
    const TransformOperation* to,
    SkScalar progress,
    TransformOperation* result) {
  bool is_identity_from = IsOperationIdentity(from);
  bool is_identity_to = IsOperationIdentity(to);
  if (is_identity_from && is_identity_to)
    return true;

  TransformOperation::Type interpolation_type =
      TransformOperation::TRANSFORM_OPERATION_IDENTITY;
  if (is_identity_to)
    interpolation_type = from->type;
  else
    interpolation_type = to->type;
  result->type = interpolation_type;

  switch (interpolation_type) {
    case TransformOperation::TRANSFORM_OPERATION_TRANSLATE: {
      SkScalar from_x = is_identity_from ? 0 : from->translate.x;
      SkScalar from_y = is_identity_from ? 0 : from->translate.y;
      SkScalar from_z = is_identity_from ? 0 : from->translate.z;
      SkScalar to_x = is_identity_to ? 0 : to->translate.x;
      SkScalar to_y = is_identity_to ? 0 : to->translate.y;
      SkScalar to_z = is_identity_to ? 0 : to->translate.z;
      result->translate.x = BlendSkScalars(from_x, to_x, progress),
      result->translate.y = BlendSkScalars(from_y, to_y, progress),
      result->translate.z = BlendSkScalars(from_z, to_z, progress),
      result->Bake();
      break;
    }
    case TransformOperation::TRANSFORM_OPERATION_ROTATE: {
      SkScalar axis_x = 0;
      SkScalar axis_y = 0;
      SkScalar axis_z = 1;
      SkScalar from_angle = 0;
      SkScalar to_angle = is_identity_to ? 0 : to->rotate.angle;
      if (ShareSameAxis(from, is_identity_from, to, is_identity_to, &axis_x,
                        &axis_y, &axis_z, &from_angle)) {
        result->rotate.axis.x = axis_x;
        result->rotate.axis.y = axis_y;
        result->rotate.axis.z = axis_z;
        result->rotate.angle = BlendSkScalars(from_angle, to_angle, progress);
        result->Bake();
      } else {
        if (!is_identity_to)
          result->matrix = to->matrix;
        gfx::Transform from_matrix;
        if (!is_identity_from)
          from_matrix = from->matrix;
        if (!result->matrix.Blend(from_matrix, progress))
          return false;
      }
      break;
    }
    case TransformOperation::TRANSFORM_OPERATION_SCALE: {
      SkScalar from_x = is_identity_from ? 1 : from->scale.x;
      SkScalar from_y = is_identity_from ? 1 : from->scale.y;
      SkScalar from_z = is_identity_from ? 1 : from->scale.z;
      SkScalar to_x = is_identity_to ? 1 : to->scale.x;
      SkScalar to_y = is_identity_to ? 1 : to->scale.y;
      SkScalar to_z = is_identity_to ? 1 : to->scale.z;
      result->scale.x = BlendSkScalars(from_x, to_x, progress);
      result->scale.y = BlendSkScalars(from_y, to_y, progress);
      result->scale.z = BlendSkScalars(from_z, to_z, progress);
      result->Bake();
      break;
    }
    case TransformOperation::TRANSFORM_OPERATION_SKEWX:
    case TransformOperation::TRANSFORM_OPERATION_SKEWY:
    case TransformOperation::TRANSFORM_OPERATION_SKEW: {
      SkScalar from_x = is_identity_from ? 0 : from->skew.x;
      SkScalar from_y = is_identity_from ? 0 : from->skew.y;
      SkScalar to_x = is_identity_to ? 0 : to->skew.x;
      SkScalar to_y = is_identity_to ? 0 : to->skew.y;
      result->skew.x = BlendSkScalars(from_x, to_x, progress);
      result->skew.y = BlendSkScalars(from_y, to_y, progress);
      result->Bake();
      break;
    }
    case TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE: {
      SkScalar from_perspective_m43;
      if (is_identity_from) {
        from_perspective_m43 = 0.f;
      } else {
        DCHECK_LE(from->perspective_m43, 0.0f);
        DCHECK_GE(from->perspective_m43, -1.0f);
        from_perspective_m43 = from->perspective_m43;
      }
      SkScalar to_perspective_m43;
      if (is_identity_to) {
        to_perspective_m43 = 0.f;
      } else {
        DCHECK_LE(to->perspective_m43, 0.0f);
        DCHECK_GE(to->perspective_m43, -1.0f);
        to_perspective_m43 = to->perspective_m43;
      }

      result->perspective_m43 = std::clamp(
          BlendSkScalars(from_perspective_m43, to_perspective_m43, progress),
          -1.0f, 0.0f);
      result->Bake();
      break;
    }
    case TransformOperation::TRANSFORM_OPERATION_MATRIX: {
      if (!is_identity_to)
        result->matrix = to->matrix;
      gfx::Transform from_matrix;
      if (!is_identity_from)
        from_matrix = from->matrix;
      if (!result->matrix.Blend(from_matrix, progress))
        return false;
      break;
    }
    case TransformOperation::TRANSFORM_OPERATION_IDENTITY:
      // Do nothing.
      break;
  }

  return true;
}

// If p = (px, py) is a point in the plane being rotated about (0, 0, nz), this
// function computes the angles we would have to rotate from p to get to
// (length(p), 0), (-length(p), 0), (0, length(p)), (0, -length(p)). If nz is
// negative, these angles will need to be reversed.
static void FindCandidatesInPlane(float px,
                                  float py,
                                  float nz,
                                  double* candidates,
                                  int* num_candidates) {
  double phi = atan2(px, py);
  *num_candidates = 4;
  candidates[0] = phi;
  for (int i = 1; i < *num_candidates; ++i)
    candidates[i] = candidates[i - 1] + std::numbers::pi / 2;
  if (nz < 0.f) {
    for (int i = 0; i < *num_candidates; ++i)
      candidates[i] *= -1.f;
  }
}

static void BoundingBoxForArc(const gfx::Point3F& point,
                              const TransformOperation* from,
                              const TransformOperation* to,
                              SkScalar min_progress,
                              SkScalar max_progress,
                              gfx::BoxF* box) {
  const TransformOperation* exemplar = from ? from : to;
  gfx::Vector3dF axis(exemplar->rotate.axis.x, exemplar->rotate.axis.y,
                      exemplar->rotate.axis.z);

  const bool x_is_zero = axis.x() == 0.f;
  const bool y_is_zero = axis.y() == 0.f;
  const bool z_is_zero = axis.z() == 0.f;

  // We will have at most 6 angles to test (excluding from->angle and
  // to->angle).
  static const int kMaxNumCandidates = 6;
  double candidates[kMaxNumCandidates];
  int num_candidates = kMaxNumCandidates;

  if (x_is_zero && y_is_zero && z_is_zero)
    return;

  SkScalar from_angle = from ? from->rotate.angle : 0.f;
  SkScalar to_angle = to ? to->rotate.angle : 0.f;

  // If the axes of rotation are pointing in opposite directions, we need to
  // flip one of the angles. Note, if both |from| and |to| exist, then axis will
  // correspond to |from|.
  if (from && to) {
    gfx::Vector3dF other_axis(to->rotate.axis.x, to->rotate.axis.y,
                              to->rotate.axis.z);
    if (gfx::DotProduct(axis, other_axis) < 0.f)
      to_angle *= -1.f;
  }

  float min_degrees =
      SkScalarToFloat(BlendSkScalars(from_angle, to_angle, min_progress));
  float max_degrees =
      SkScalarToFloat(BlendSkScalars(from_angle, to_angle, max_progress));
  if (max_degrees < min_degrees)
    std::swap(min_degrees, max_degrees);

  gfx::Transform from_transform;
  from_transform.RotateAbout(axis, min_degrees);
  gfx::Transform to_transform;
  to_transform.RotateAbout(axis, max_degrees);

  *box = gfx::BoxF();

  gfx::Point3F point_rotated_from = from_transform.MapPoint(point);
  gfx::Point3F point_rotated_to = to_transform.MapPoint(point);

  box->set_origin(point_rotated_from);
  box->ExpandTo(point_rotated_to);

  if (x_is_zero && y_is_zero) {
    FindCandidatesInPlane(point.x(), point.y(), axis.z(), candidates,
                          &num_candidates);
  } else if (x_is_zero && z_is_zero) {
    FindCandidatesInPlane(point.z(), point.x(), axis.y(), candidates,
                          &num_candidates);
  } else if (y_is_zero && z_is_zero) {
    FindCandidatesInPlane(point.y(), point.z(), axis.x(), candidates,
                          &num_candidates);
  } else {
    gfx::Vector3dF normal = axis;
    normal.InvScale(normal.Length());

    // First, find center of rotation.
    gfx::Point3F origin;
    gfx::Vector3dF to_point = point - origin;
    gfx::Point3F center =
        origin + gfx::ScaleVector3d(normal, gfx::DotProduct(to_point, normal));

    // Now we need to find two vectors in the plane of rotation. One pointing
    // towards point and another, perpendicular vector in the plane.
    gfx::Vector3dF v1 = point - center;
    float v1_length = v1.Length();
    if (v1_length == 0.f)
      return;

    v1.InvScale(v1_length);
    gfx::Vector3dF v2 = gfx::CrossProduct(normal, v1);
    // v1 is the basis vector in the direction of the point.
    // i.e. with a rotation of 0, v1 is our +x vector.
    // v2 is a perpenticular basis vector of our plane (+y).

    // Take the parametric equation of a circle.
    // x = r*cos(t); y = r*sin(t);
    // We can treat that as a circle on the plane v1xv2.
    // From that we get the parametric equations for a circle on the
    // plane in 3d space of:
    // x(t) = r*cos(t)*v1.x + r*sin(t)*v2.x + cx
    // y(t) = r*cos(t)*v1.y + r*sin(t)*v2.y + cy
    // z(t) = r*cos(t)*v1.z + r*sin(t)*v2.z + cz
    // Taking the derivative of (x, y, z) and solving for 0 gives us our
    // maximum/minimum x, y, z values.
    // x'(t) = r*cos(t)*v2.x - r*sin(t)*v1.x = 0
    // tan(t) = v2.x/v1.x
    // t = atan2(v2.x, v1.x) + n*pi;
    candidates[0] = atan2(v2.x(), v1.x());
    candidates[1] = candidates[0] + std::numbers::pi;
    candidates[2] = atan2(v2.y(), v1.y());
    candidates[3] = candidates[2] + std::numbers::pi;
    candidates[4] = atan2(v2.z(), v1.z());
    candidates[5] = candidates[4] + std::numbers::pi;
  }

  double min_radians = base::DegToRad(min_degrees);
  double max_radians = base::DegToRad(max_degrees);

  for (int i = 0; i < num_candidates; ++i) {
    double radians = candidates[i];
    while (radians < min_radians)
      radians += 2.0 * std::numbers::pi;
    while (radians > max_radians)
      radians -= 2.0 * std::numbers::pi;
    if (radians < min_radians)
      continue;

    gfx::Transform rotation;
    rotation.RotateAbout(axis, base::RadToDeg(radians));
    gfx::Point3F rotated = rotation.MapPoint(point);

    box->ExpandTo(rotated);
  }
}

bool TransformOperation::BlendedBoundsForBox(const gfx::BoxF& box,
                                             const TransformOperation* from,
                                             const TransformOperation* to,
                                             SkScalar min_progress,
                                             SkScalar max_progress,
                                             gfx::BoxF* bounds) {
  bool is_identity_from = IsOperationIdentity(from);
  bool is_identity_to = IsOperationIdentity(to);
  if (is_identity_from && is_identity_to) {
    *bounds = box;
    return true;
  }

  TransformOperation::Type interpolation_type =
      TransformOperation::TRANSFORM_OPERATION_IDENTITY;
  if (is_identity_to)
    interpolation_type = from->type;
  else
    interpolation_type = to->type;

  switch (interpolation_type) {
    case TransformOperation::TRANSFORM_OPERATION_IDENTITY:
      *bounds = box;
      return true;
    case TransformOperation::TRANSFORM_OPERATION_TRANSLATE:
    case TransformOperation::TRANSFORM_OPERATION_SKEWX:
    case TransformOperation::TRANSFORM_OPERATION_SKEWY:
    case TransformOperation::TRANSFORM_OPERATION_SKEW:
    case TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE:
    case TransformOperation::TRANSFORM_OPERATION_SCALE: {
      TransformOperation from_operation;
      TransformOperation to_operation;
      if (!BlendTransformOperations(from, to, min_progress, &from_operation) ||
          !BlendTransformOperations(from, to, max_progress, &to_operation))
        return false;

      *bounds = from_operation.matrix.MapBox(box);

      BoxF to_box = to_operation.matrix.MapBox(box);
      bounds->ExpandTo(to_box);

      return true;
    }
    case TransformOperation::TRANSFORM_OPERATION_ROTATE: {
      SkScalar axis_x = 0;
      SkScalar axis_y = 0;
      SkScalar axis_z = 1;
      SkScalar from_angle = 0;
      if (!ShareSameAxis(from, is_identity_from, to, is_identity_to, &axis_x,
                         &axis_y, &axis_z, &from_angle)) {
        return false;
      }

      bool first_point = true;
      for (int i = 0; i < 8; ++i) {
        gfx::Point3F corner = box.origin();
        corner += gfx::Vector3dF(i & 1 ? box.width() : 0.f,
                                 i & 2 ? box.height() : 0.f,
                                 i & 4 ? box.depth() : 0.f);
        gfx::BoxF box_for_arc;
        BoundingBoxForArc(corner, from, to, min_progress, max_progress,
                          &box_for_arc);
        if (first_point)
          *bounds = box_for_arc;
        else
          bounds->Union(box_for_arc);
        first_point = false;
      }
      return true;
    }
    case TransformOperation::TRANSFORM_OPERATION_MATRIX:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace gfx
