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


// |matrix| - Float32Array representing 4x4 matrix (column major)
// |point| - DOMPointReadOnly
const transformByMatrix = function(matrix, point){
  return new DOMPointReadOnly(
    matrix[0] * point.x + matrix[4] * point.y + matrix[8] * point.z + matrix[12] * point.w,
    matrix[1] * point.x + matrix[5] * point.y + matrix[9] * point.z + matrix[13] * point.w,
    matrix[2] * point.x + matrix[6] * point.y + matrix[10] * point.z + matrix[14] * point.w,
    matrix[3] * point.x + matrix[7] * point.y + matrix[11] * point.z + matrix[15] * point.w,
  );
};

// |lhs|, |rhs| - Float32Arrays representing 4x4 matrices (column major)
// returns Float32Array representing 4x4 matrix (column major)
const multiply = function(lhs, rhs) {
  let result = new Float32Array([
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
  ]);

  for(let col = 0; col < 4; ++col) {
    for(let row = 0; row < 4; ++row) {
      for(let k = 0; k < 4; ++k) {
        result[col * 4 + row] += rhs[col * 4 + k] * lhs[k * 4 + row];
      }
    }
  }

  return result;
};

// |point| - DOMPointReadOnly
const normalizeLength = function(point){
  let l = Math.sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
  return new DOMPointReadOnly(point.x / l, point.y / l, point.z / l, point.w);
};

// |lhs|, |rhs| - 3-element float arrays
// returns 3-element float array
const dot = function(lhs, rhs) {
  return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
};

// |lhs|, |rhs| - 3-element float arrays
// returns 3-element float array
const cross = function(lhs, rhs){
  return [
    lhs[1] * rhs[2] - lhs[2] * rhs[1],
    lhs[2] * rhs[0] - lhs[0] * rhs[2],
    lhs[0] * rhs[1] - lhs[1] * rhs[0],
  ];
};

// |matrix| - Float32Array representing 4x4 matrix (column major)
// |axis| - DOMPointReadOnly representing axis, must be normalized
// |angle| - float, angle in radians
const rotate = function(matrix, axis, angle) {
  const sin_angle = Math.sin(angle);
  const cos_angle = Math.cos(angle);
  const one_minus_cos_angle = 1 - cos_angle;

  const rotateMatrix = new Float32Array([
      // first column
      cos_angle + axis.x * axis.x * one_minus_cos_angle,
      axis.y * axis.x * one_minus_cos_angle + axis.z * sin_angle,
      axis.z * axis.x * one_minus_cos_angle - axis.y * sin_angle,
      0,
      // second column
      axis.x * axis.y * one_minus_cos_angle - axis.z * sin_angle,
      cos_angle + axis.y * axis.y * one_minus_cos_angle,
      axis.z * axis.y * one_minus_cos_angle + axis.x * sin_angle,
      0,
      // third column
      axis.x * axis.z * one_minus_cos_angle + axis.y * sin_angle,
      axis.y * axis.z * one_minus_cos_angle - axis.x * sin_angle,
      cos_angle + axis.z * axis.z * one_minus_cos_angle,
      0,
      // fourth column
      0,
      0,
      0,
      1
  ]);

  return multiply(matrix, rotateMatrix);
};

export class XRRay {
  constructor() {
    if (arguments.length > 0 && arguments[0] instanceof XRRigidTransform) {
      if(arguments.length != 1)
        throw new Error("Invalid number of arguments!");

      this.origin_ = transformByMatrix(
        arguments[0].matrix, new DOMPointReadOnly(0, 0, 0, 1));
      this.direction_ = normalizeLength(
        transformByMatrix(
          arguments[0].matrix, new DOMPointReadOnly(0, 0, -1, 0)));
      this.matrix_ = arguments[0].matrix;
    } else {
      if(arguments.length > 2)
        throw new Error("Too many arguments!");

      this.origin_ = (arguments.length > 0 && arguments[0] !== undefined)
        ? arguments[0]
        : new DOMPointReadOnly(0, 0, 0, 1);
      this.direction_ = (arguments.length > 1 && arguments[1] !== undefined)
        ? arguments[1]
        : new DOMPointReadOnly(0, 0, -1, 0);

      // Compute the matrix from origin & direction.
      this.matrix_ = new Float32Array([
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        this.origin_.x, this.origin_.y, this.origin_.z, 1
      ]);

      const initial_ray_direction = [0, 0, -1];
      const desired_ray_direction = [this.direction_.x, this.direction_.y, this.direction_.z];

      const cos_angle = dot(initial_ray_direction, desired_ray_direction);

      if (cos_angle > 0.9999) {
        // Vectors are co-linear or almost co-linear & face the same direction,
        // no rotation is needed.
      } else if (cos_angle < -0.9999) {
        // Vectors are co-linear or almost co-linear & face the opposite
        // direction, rotation by 180 degrees is needed & can be around any vector
        // perpendicular to (0,0,-1) so let's rotate by (1, 0, 0).
        const axis = new DOMPointReadOnly(1, 0, 0, 0);
        cos_angle = -1;

        this.matrix_ = rotate(this.matrix_, axis, Math.acos(cos_angle));
      } else {
        // Rotation needed - create it from axis-angle.
        const cross_initial_desired = cross(initial_ray_direction, desired_ray_direction);
        const axis = normalizeLength(new DOMPointReadOnly(
          cross_initial_desired[0], cross_initial_desired[1], cross_initial_desired[2], 0));

        this.matrix_ = rotate(this.matrix_, axis, Math.acos(cos_angle));
      }
    }

    if (!(this.origin_ instanceof DOMPointReadOnly)) {
      throw new Error('origin must be a DOMPointReadOnly');
    }
    if (!(this.direction_ instanceof DOMPointReadOnly)) {
      throw new Error('direction must be a DOMPointReadOnly');
    }
    if (!(this.matrix_ instanceof Float32Array)) {
      throw new Error('matrix must be a Float32Array');
    }
  }

  get origin() { return this.origin_; }
  get direction() { return this.direction_; }
  get matrix() { return this.matrix_; }
}
