// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function ImageLoaderUtil() {}

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
      }
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
      height: source.height
    },
    target: {
      x: 0,
      y: 0,
      width: targetDimensions.width,
      height: targetDimensions.height
    },
    canvas: {
      width: targetCanvasDimensions.width,
      height: targetCanvasDimensions.height
    }
  };
};

/**
 * Matrix converts AdobeRGB color space into sRGB color space.
 * @const {!Array<number>}
 */
ImageLoaderUtil.MATRIX_FROM_ADOBE_TO_STANDARD = [
  1.39836, -0.39836, 0.00000,
  0.00000,  1.00000, 0.00000,
  0.00000, -0.04293, 1.04293
];

/**
 * Converts the canvas of color space into sRGB. TODO(noel): the Chrome <canvas>
 * is color managed today. Is this code still needed?
 * @param {HTMLCanvasElement} target Target canvas.
 * @param {string} colorSpace Current color space.
 */
ImageLoaderUtil.convertColorSpace = function(target, colorSpace) {
  if (colorSpace === 'adobeRgb') {
    const matrix = ImageLoaderUtil.MATRIX_FROM_ADOBE_TO_STANDARD;
    const context =
        assertInstanceof(target.getContext('2d'), CanvasRenderingContext2D);
    const imageData = context.getImageData(0, 0, target.width, target.height);
    const data = imageData.data;
    for (let i = 0; i < data.length; i += 4) {
      // Scale to [0, 1].
      let adobeR = data[i] / 255;
      let adobeG = data[i + 1] / 255;
      let adobeB = data[i + 2] / 255;

      // Apply adobeRgb inverse gamma to convert to linear color.
      adobeR = adobeR <= 0.0556 ? adobeR / 32 : Math.pow(adobeR, 2.2);
      adobeG = adobeG <= 0.0556 ? adobeG / 32 : Math.pow(adobeG, 2.2);
      adobeB = adobeB <= 0.0556 ? adobeB / 32 : Math.pow(adobeB, 2.2);

      // Matrix convert linear adobeRgb color to linear sRgb color.
      let sR = matrix[0] * adobeR + matrix[1] * adobeG + matrix[2] * adobeB;
      let sG = matrix[3] * adobeR + matrix[4] * adobeG + matrix[5] * adobeB;
      let sB = matrix[6] * adobeR + matrix[7] * adobeG + matrix[8] * adobeB;

      // Convert linear color to sRgb gamma color.
      sR = sR <= 0.0031308 ? 12.92 * sR : 1.055 * Math.pow(sR, 1 / 2.4) - 0.055;
      sG = sG <= 0.0031308 ? 12.92 * sG : 1.055 * Math.pow(sG, 1 / 2.4) - 0.055;
      sB = sB <= 0.0031308 ? 12.92 * sB : 1.055 * Math.pow(sB, 1 / 2.4) - 0.055;

      // Scale to [0, 255].
      data[i] = Math.max(0, Math.min(255, sR * 255));
      data[i + 1] = Math.max(0, Math.min(255, sG * 255));
      data[i + 2] = Math.max(0, Math.min(255, sB * 255));
    }
    context.putImageData(imageData, 0, 0);
  }
};
