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
 * Creates an HTML element from a custom Emmet-like DSL string.
 *
 * Syntax Rules:
 * 1. Tag, ID, and Class name: specified in standard CSS selector syntax
 *    (e.g., 'div#main-id.class1.class2'). Defaults to 'div' if tag is omitted.
 * 2. Attributes: Prefixed by '@' and assigned via '=' (e.g., '@width=100').
 * 3. Text Content: Separated from the selector portion by a Tab ('\t')
 *    character (e.g., 'span.title\tHello World').
 *
 * Example:
 *   makeElt('button#btn-load.action-button@disabled=true\tLoad Screen')
 *
 * @param {string} input The formatted DSL string.
 * @return {!HTMLElement} The newly created element.
 */
function makeElt(input) {
  // 1. Separate Text from Metadata using Tab
  const [meta, text] = input.split('\t');

  // 2. Extract Tag, ID, and Classes
  const tag = meta.match(/^[a-z0-9]+/i)?.[0] || 'div';
  const id = meta.match(/#([a-z0-9_-]+)/i)?.[1];
  const classes = meta.match(/\.([a-z0-9_-]+)/gi)?.map(c => c.slice(1));

  const el = document.createElement(tag);

  // 3. Assign ID and Classes
  if (id) el.id = id;
  if (classes) el.className = classes.join(' ');

  // 4. Parse Attributes (anything starting with @)
  const attrMatches = meta.matchAll(/@([a-z0-9_-]+)=([^@#.\s\t]+)/gi);
  for (const match of attrMatches) {
    el.setAttribute(match[1], match[2].replace(/["']/g, ''));
  }

  // 5. Assign Text
  if (text) el.textContent = text;

  return el;
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
