// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// @ts-nocheck

import {assert} from 'chrome://resources/ash/common/assert.js';

import {LoadImageRequest} from './load_image_request.js';

export function ImageLoaderUtil() {}

/**
 * Checks if the options on the request contain any image processing.
 *
 * @param {number} width Source width.
 * @param {number} height Source height.
 * @param {!LoadImageRequest} request The request, containing resizing options.
 * @return {boolean} True if yes, false if not.
 */
ImageLoaderUtil.shouldProcess = function(width, height, request) {
  const targetDimensions =
      ImageLoaderUtil.resizeDimensions(width, height, request);

  // Dimensions has to be adjusted.
  if (targetDimensions.width != width || targetDimensions.height != height) {
    return true;
  }

  // Orientation has to be adjusted.
  if (!request.orientation.isIdentity()) {
    return true;
  }

  // No changes required.
  return false;
};

/**
 * Calculates dimensions taking into account resize options, such as:
 * - scale: for scaling,
 * - maxWidth, maxHeight: for maximum dimensions,
 * - width, height: for exact requested size.
 * Returns the target size as hash array with width, height properties.
 *
 * @param {number} width Source width.
 * @param {number} height Source height.
 * @param {!LoadImageRequest} request The request, containing resizing options.
 * @return {!{width: number, height:number}} Dimensions.
 */
ImageLoaderUtil.resizeDimensions = function(width, height, request) {
  const scale = request.scale || 1;
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
};

/**
 * Performs resizing and cropping of the source image into the target canvas.
 *
 * @param {HTMLCanvasElement|Image} source Source image or canvas.
 * @param {HTMLCanvasElement} target Target canvas.
 * @param {!LoadImageRequest} request The request, containing resizing options.
 */
ImageLoaderUtil.resizeAndCrop = function(source, target, request) {
  // Calculates copy parameters.
  const copyParameters =
      ImageLoaderUtil.calculateCopyParameters(source, request);
  target.width = copyParameters.canvas.width;
  target.height = copyParameters.canvas.height;

  // Apply.
  const targetContext =
      /** @type {CanvasRenderingContext2D} */ (target.getContext('2d'));
  targetContext.save();
  request.orientation.cancelImageOrientation(
      targetContext, copyParameters.target.width, copyParameters.target.height);
  targetContext.drawImage(
      source,
      copyParameters.source.x,
      copyParameters.source.y,
      copyParameters.source.width,
      copyParameters.source.height,
      copyParameters.target.x,
      copyParameters.target.y,
      copyParameters.target.width,
      copyParameters.target.height);
  targetContext.restore();
};

/**
 * @typedef {{
 *   source: {x:number, y:number, width:number, height:number},
 *   target: {x:number, y:number, width:number, height:number},
 *   canvas: {width:number, height:number}
 * }}
 */
ImageLoaderUtil.CopyParameters;

/**
 * Calculates copy parameters.
 *
 * @param {HTMLCanvasElement|Image} source Source image or canvas.
 * @param {!LoadImageRequest} request The request, containing resizing options.
 * @return {!ImageLoaderUtil.CopyParameters} Calculated copy parameters.
 */
ImageLoaderUtil.calculateCopyParameters = function(source, request) {
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
        width: request.width,
        height: request.height,
      },
      canvas: {
        width: request.width,
        height: request.height,
      },
    };
  }

  // Target dimension is calculated in the rotated(transformed) coordinate.
  const targetCanvasDimensions =
      ImageLoaderUtil.resizeDimensions(source.width, source.height, request);

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
};
