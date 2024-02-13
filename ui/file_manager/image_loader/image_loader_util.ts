// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {isImageOrientation} from './image_orientation.js';
import type {LoadImageRequest} from './load_image_request.js';

/**
 * Checks if the options on the request contain any image processing.
 *
 * @param width Source width.
 * @param height Source height.
 * @param request The request, containing resizing options.
 * @return True if yes, false if not.
 */
export function shouldProcess(
    width: number, height: number, request: LoadImageRequest): boolean {
  const targetDimensions = resizeDimensions(width, height, request);

  // Dimensions has to be adjusted.
  if (targetDimensions.width !== width || targetDimensions.height !== height) {
    return true;
  }

  // Orientation has to be adjusted.
  if (isImageOrientation(request.orientation) &&
      !request.orientation.isIdentity()) {
    return true;
  }

  // No changes required.
  return false;
}

/**
 * Calculates dimensions taking into account resize options, such as:
 * - scale: for scaling,
 * - maxWidth, maxHeight: for maximum dimensions,
 * - width, height: for exact requested size.
 * Returns the target size as hash array with width, height properties.
 *
 * @param width Source width.
 * @param height Source height.
 * @param request The request, containing resizing options.
 * @return Dimensions.
 */
export function resizeDimensions(
    width: number, height: number,
    request: LoadImageRequest): {width: number, height: number} {
  const scale = request.scale || 1;
  assert(isImageOrientation(request.orientation));
  const targetDimensions =
      request.orientation.getSizeAfterCancelling(width * scale, height * scale);
  let targetWidth = targetDimensions.width;
  let targetHeight = targetDimensions.height;

  if (request.maxWidth && targetWidth > request.maxWidth) {
    const scale = request.maxWidth / targetWidth;
    targetWidth *= scale;
    targetHeight *= scale;
  }

  if (request.maxHeight && targetHeight > request.maxHeight) {
    const scale = request.maxHeight / targetHeight;
    targetWidth *= scale;
    targetHeight *= scale;
  }

  if (request.width) {
    targetWidth = request.width;
  }

  if (request.height) {
    targetHeight = request.height;
  }

  targetWidth = Math.round(targetWidth);
  targetHeight = Math.round(targetHeight);

  return {width: targetWidth, height: targetHeight};
}

/**
 * Performs resizing and cropping of the source image into the target canvas.
 *
 * @param source Source image or canvas.
 * @param target Target canvas.
 * @param request The request, containing resizing options.
 */
export function resizeAndCrop(
    source: HTMLCanvasElement|HTMLImageElement, target: HTMLCanvasElement,
    request: LoadImageRequest) {
  // Calculates copy parameters.
  const copyParameters = calculateCopyParameters(source, request);
  target.width = copyParameters.canvas.width;
  target.height = copyParameters.canvas.height;

  // Apply.
  const targetContext = target.getContext('2d')!;
  targetContext.save();
  assert(isImageOrientation(request.orientation));
  request.orientation.cancelImageOrientation(
      targetContext, copyParameters.target.width, copyParameters.target.height);
  targetContext.drawImage(
      source, copyParameters.source.x, copyParameters.source.y,
      copyParameters.source.width, copyParameters.source.height,
      copyParameters.target.x, copyParameters.target.y,
      copyParameters.target.width, copyParameters.target.height);
  targetContext.restore();
}

export interface CopyParameters {
  source: {x: number, y: number, width: number, height: number};
  target: {x: number, y: number, width: number, height: number};
  canvas: {width: number, height: number};
}

/**
 * Calculates copy parameters.
 *
 * @param source Source image or canvas.
 * @param request The request, containing resizing options.
 * @return Calculated copy parameters.
 */
export function calculateCopyParameters(
    source: HTMLCanvasElement|HTMLImageElement,
    request: LoadImageRequest): CopyParameters {
  if (request.crop) {
    // When an image is cropped, target should be a fixed size square.
    assert(request.width);
    assert(request.height);
    assert(request.width === request.height);

    // The length of shorter edge becomes dimension of cropped area in the
    // source.
    const cropSourceDimension = Math.min(source.width, source.height);

    return {
      source: {
        x: Math.floor((source.width / 2) - (cropSourceDimension / 2)),
        y: Math.floor((source.height / 2) - (cropSourceDimension / 2)),
        width: cropSourceDimension,
        height: cropSourceDimension,
      },
      target: {
        x: 0,
        y: 0,
        width: request.width!,
        height: request.height!,
      },
      canvas: {
        width: request.width!,
        height: request.height!,
      },
    };
  }

  // Target dimension is calculated in the rotated(transformed) coordinate.
  const targetCanvasDimensions =
      resizeDimensions(source.width, source.height, request);

  assert(isImageOrientation(request.orientation));
  const targetDimensions = request.orientation.getSizeAfterCancelling(
      targetCanvasDimensions.width, targetCanvasDimensions.height);

  return {
    source: {
      x: 0,
      y: 0,
      width: source.width,
      height: source.height,
    },
    target: {
      x: 0,
      y: 0,
      width: targetDimensions.width,
      height: targetDimensions.height,
    },
    canvas: {
      width: targetCanvasDimensions.width,
      height: targetCanvasDimensions.height,
    },
  };
}
