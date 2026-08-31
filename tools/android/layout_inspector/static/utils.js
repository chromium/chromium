// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Utilities ********/

/**
 * Ensures unique and consecutive numbering of integer enums.
 */
function checkEnum(enumObj) {
  const vals = Object.values(enumObj);
  if (vals.length !== new Set(vals).size) {
    throw new Error(`Enum has duplicate items: ${JSON.stringify(enumObj)}.`);
  }
  if (vals.length != Math.max(...vals) - Math.min(...vals) + 1) {
    throw new Error(`Enum has gaps:  ${JSON.stringify(enumObj)}.`);
  }
  return enumObj;
}

/**
 * Asynchronously loads an image Blob into an HTMLImageElement, managing the
 * temporary URL lifecycle.
 *
 * @param {!Blob} blob The raw image data blob.
 * @return {!Promise<!HTMLImageElement>} Rejects if image loading fails.
 */
async function convertImageBlobToImage(blob) {
  const blobUrl = URL.createObjectURL(blob);
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => {
      URL.revokeObjectURL(blobUrl);
      resolve(img);
    };
    img.onerror = (e) => {
      URL.revokeObjectURL(blobUrl);
      reject(new Error(e));
    };
    img.src = blobUrl;
  });
}
