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
