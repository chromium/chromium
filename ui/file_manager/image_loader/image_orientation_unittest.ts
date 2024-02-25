// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {ImageOrientation} from './image_orientation.js';

export function testGetSizeAfterCancelling() {
  // Crockwise 90 degrees image orientation.
  const orientation = new ImageOrientation(0, 1, 1, 0);

  // After cancelling orientation, the width and the height are swapped.
  const size = orientation.getSizeAfterCancelling(100, 200);
  assertEquals(200, size.width);
  assertEquals(100, size.height);
}

export function testCancelImageOrientation() {
  // Crockwise 90 degrees image orientation.
  const orientation = new ImageOrientation(0, 1, 1, 0);

  const canvas = document.createElement('canvas');
  canvas.width = 2;
  canvas.height = 1;

  const context = canvas.getContext('2d')!;
  const imageData = context.createImageData(2, 1);
  imageData.data[0] = 255;  // R
  imageData.data[1] = 0;    // G
  imageData.data[2] = 0;    // B
  imageData.data[3] = 100;  // A
  imageData.data[4] = 0;    // R
  imageData.data[5] = 0;    // G
  imageData.data[6] = 0;    // B
  imageData.data[7] = 100;  // A
  context.putImageData(imageData, 0, 0);

  const destinationCanvas = document.createElement('canvas');
  destinationCanvas.width = 1;
  destinationCanvas.height = 2;
  const destinationContext = destinationCanvas.getContext('2d')!;
  orientation.cancelImageOrientation(destinationContext, 2, 1);
  destinationContext.drawImage(canvas, 0, 0);
  const destinationImageData = destinationContext.getImageData(0, 0, 1, 2);
  assertDeepEquals(
      new Uint8ClampedArray([255, 0, 0, 100, 0, 0, 0, 100]),
      destinationImageData.data);
}

function assertImageOrientationEquals(
    expected: ImageOrientation, actual: ImageOrientation, message: string) {
  assertEquals(expected.a, actual.a, message);
  assertEquals(expected.b, actual.b, message);
  assertEquals(expected.c, actual.c, message);
  assertEquals(expected.d, actual.d, message);
}

export function testFromRotationAndScale() {
  const rotate270 = {scaleX: 1, scaleY: 1, rotate90: -1};
  const rotate90 = {scaleX: 1, scaleY: 1, rotate90: 1};
  const flipX = {scaleX: -1, scaleY: 1, rotate90: 0};
  const flipY = {scaleX: 1, scaleY: -1, rotate90: 0};
  const flipBoth = {scaleX: -1, scaleY: -1, rotate90: 0};
  const rotate180 = {scaleX: 1, scaleY: 1, rotate90: 2};
  const flipXAndRotate90 = {scaleX: -1, scaleY: 1, rotate90: 1};
  const flipYAndRotate90 = {scaleX: 1, scaleY: -1, rotate90: 1};
  const rotate1080 = {scaleX: 1, scaleY: 1, rotate90: 12};
  const flipBothAndRotate180 = {scaleX: -1, scaleY: -1, rotate90: 2};
  /*
   The image coordinate system is aligned to the screen. (Y+ pointing down)
   O----> e_x                 ^
   |           rotate 270 CW  | e'_x = (0, -1)' = (a, b)'
   |             =====>       |
   V e_y                      O----> e'_y = (1, 0)' = (c, d)'
  */
  assertImageOrientationEquals(
      new ImageOrientation(0, -1, 1, 0),
      ImageOrientation.fromRotationAndScale(rotate270), 'rotate270');
  assertImageOrientationEquals(
      new ImageOrientation(0, 1, -1, 0),
      ImageOrientation.fromRotationAndScale(rotate90), 'rotate90');
  assertImageOrientationEquals(
      new ImageOrientation(-1, 0, 0, 1),
      ImageOrientation.fromRotationAndScale(flipX), 'flipX');
  assertImageOrientationEquals(
      new ImageOrientation(1, 0, 0, -1),
      ImageOrientation.fromRotationAndScale(flipY), 'flipY');
  assertImageOrientationEquals(
      new ImageOrientation(-1, 0, 0, -1),
      ImageOrientation.fromRotationAndScale(flipBoth), 'flipBoth');
  assertImageOrientationEquals(
      new ImageOrientation(-1, 0, 0, -1),
      ImageOrientation.fromRotationAndScale(rotate180), 'rotate180');
  assertImageOrientationEquals(
      new ImageOrientation(0, -1, -1, 0),
      ImageOrientation.fromRotationAndScale(flipXAndRotate90),
      'flipXAndRotate90');
  assertImageOrientationEquals(
      new ImageOrientation(0, 1, 1, 0),
      ImageOrientation.fromRotationAndScale(flipYAndRotate90),
      'flipYAndRotate90');
  assertTrue(
      ImageOrientation.fromRotationAndScale(flipBothAndRotate180).isIdentity(),
      'flipBothAndRotate180');
  assertTrue(
      ImageOrientation.fromRotationAndScale(rotate1080).isIdentity(),
      'rotate1080');
}
