// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ImageTransformParam {
  scaleX: number;
  scaleY: number;

  /** Clockwise degrees / 90. */
  rotate90: number;
}

/**
 * Class representing image orientation.
 */
export class ImageOrientation {
  /**
   * The constructor takes 2x2 matrix value that cancels the image orientation:
   * |a, c|
   * |b, d|
   */
  constructor(
      public readonly a: number, public readonly b: number,
      public readonly c: number, public readonly d: number) {}

  /**
   * @param orientation 1-based orientation number defined by EXIF.
   */
  static fromExifOrientation(orientation: number): ImageOrientation {
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
        console.error(`Invalid orientation number: ${orientation}`);
        return new ImageOrientation(1, 0, 0, 1);
    }
  }

  /**
   * @param rotation90 Clockwise degrees / 90.
   */
  static fromClockwiseRotation(rotation90: number): ImageOrientation {
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
        console.error(`Invalid rotation number: ${rotation90}`);
        return new ImageOrientation(1, 0, 0, 1);
    }
  }

  /** Builds a transformation matrix from the image transform parameters. */
  static fromRotationAndScale(transform: ImageTransformParam):
      ImageOrientation {
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

  /** Obtains the image size after cancelling its orientation. */
  getSizeAfterCancelling(imageWidth: number, imageHeight: number):
      {width: number, height: number} {
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
   */
  cancelImageOrientation(
      context: CanvasRenderingContext2D, imageWidth: number,
      imageHeight: number) {
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
   */
  isIdentity(): boolean {
    return this.a === 1 && this.b === 0 && this.c === 0 && this.d === 1;
  }
}

export function isImageTransformParam(
    orientation: ImageTransformParam|ImageOrientation|
    undefined): orientation is ImageTransformParam {
  return !!orientation && 'scaleX' in orientation && 'scaleY' in orientation;
}

export function isImageOrientation(orientation: ImageTransformParam|
                                   ImageOrientation|
                                   undefined): orientation is ImageOrientation {
  return !!orientation && 'a' in orientation && 'b' in orientation &&
      'c' in orientation && 'd' in orientation;
}
