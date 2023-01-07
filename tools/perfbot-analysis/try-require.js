// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function tryrequire(name) {
  try {
    return require(name);
  } catch (x) {
    return undefined;
  }
}

module.exports = {
  tryrequire,
}
