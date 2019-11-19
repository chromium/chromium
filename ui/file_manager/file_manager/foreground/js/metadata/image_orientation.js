// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * rotate90: clockwise degrees / 90.
 *
 * @typedef {{scaleX: number, scaleY: number, rotate90: number}}
 */
let ImageTransformParam;

/**
 * Class representing image orientation.
 * @final
 */
class ImageOrientation {
  /**
   * The constructor takes 2x2 matrix value that cancels the image orientation:
   * |a, c|
   * |b, d|
   * @param {number} a
   * @param {number} b
   * @param {number} c
   * @param {number} d
   */
  constructor(a, b, c, d) {
    /** @public @const {number} */
    this.a = a;

    /** @public @const {number} */
    this.b = b;

    /** @public @const {number} */
    this.c = c;

    /** @public @const {number} */
    this.d = d;
  }

  /**
   * @param {number} orientation 1-based orientation number defined by EXIF.
   * @return {!ImageOrientation}
   */
  static fromExifOrientation(orientation) {
    switch (~~orientation) {
      case 1:
        return new ImageOrientation(1, 0, 0, 1);
      case 2:
        return new ImageOrientation(-1, 0, 0, 1);
      case 3:
        return new ImageOrientation(-1, 0, 0, -1);
      case 4:
        return new ImageOrientation(1, 0, 0, -1);
      case 5:
        return new ImageOrientation(0, 1, 1, 0);
      case 6:
        return new ImageOrientation(0, 1, -1, 0);
      case 7:
        return new ImageOrientation(0, -1, -1, 0);
      case 8:
        return new ImageOrientation(0, -1, 1, 0);
      default:
        console.error('Invalid orientation number.');
        return new ImageOrientation(1, 0, 0, 1);
    }
  }

  /**
   * @param {number} rotation90 Clockwise degrees / 90.
   * @return {!ImageOrientation}
   */
  static fromClockwiseRotation(rotation90) {
    switch (~~(rotation90 % 4)) {
      case 0:
        return new ImageOrientation(1, 0, 0, 1);
      case 1:
      case -3:
        return new ImageOrientation(0, 1, -1, 0);
      case 2:
      case -2:
        return new ImageOrientation(-1, 0, 0, -1);
      case 3:
      case -1:
        return new ImageOrientation(0, -1, 1, 0);
      default:
        console.error('Invalid orientation number.');
        return new ImageOrientation(1, 0, 0, 1);
    }
  }

  /**
   * Builds a transformation matrix from the image transform parameters.
   * @param {!ImageTransformParam} transform
   * @return {!ImageOrientation}
   */
  static fromRotationAndScale(transform) {
    const scaleX = transform.scaleX;
    const scaleY = transform.scaleY;
    const rotate90 = transform.rotate90;

    const orientation = ImageOrientation.fromClockwiseRotation(rotate90);

    // Flip X and Y.
    // In the Files app., CSS transformations are applied like
    // "transform: rotate(90deg) scaleX(-1)".
    // Since the image is scaled based on the X,Y axes pinned to the original,
    // it is equivalent to scale first and then rotate.
    // |a c| |s_x 0 | |x|   |a*s_x c*s_y| |x|
    // |b d| | 0 s_y| |y| = |b*s_x d*s_y| |y|
    return new ImageOrientation(
        orientation.a * scaleX, orientation.b * scaleX, orientation.c * scaleY,
        orientation.d * scaleY);
  }

  /**
   * Obtains the image size after cancelling its orientation.
   * @param {number} imageWidth
   * @param {number} imageHeight
   * @return {{width:number, height:number}}
   */
  getSizeAfterCancelling(imageWidth, imageHeight) {
    const projectedX = this.a * imageWidth + this.c * imageHeight;
    const projectedY = this.b * imageWidth + this.d * imageHeight;
    return {
      width: Math.abs(projectedX),
      height: Math.abs(projectedY),
    };
  }

  /**
   * Applies the transformation that cancels the image orientation to the given
   * context.
   * @param {!CanvasRenderingContext2D} context
   * @param {number} imageWidth
   * @param {number} imageHeight
   */
  cancelImageOrientation(context, imageWidth, imageHeight) {
    // Calculate where to project the point of (imageWidth, imageHeight).
    const projectedX = this.a * imageWidth + this.c * imageHeight;
    const projectedY = this.b * imageWidth + this.d * imageHeight;

    // If the projected point coordinates are negative, add offset to cancel it.
    const offsetX = projectedX < 0 ? -projectedX : 0;
    const offsetY = projectedY < 0 ? -projectedY : 0;

    // Apply the transform.
    context.setTransform(this.a, this.b, this.c, this.d, offsetX, offsetY);
  }

  /**
   * Checks if the orientation represents identity transformation or not.
   * @return {boolean}
   */
  isIdentity() {
    return this.a === 1 && this.b === 0 && this.c === 0 && this.d === 1;
  }
}
