// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
const CvdType = {
  PROTANOMALY: 'PROTANOMALY',
  DEUTERANOMALY: 'DEUTERANOMALY',
  TRITANOMALY: 'TRITANOMALY',
};

/** @typedef {!CvdType|Storage.INVALID_TYPE_PLACEHOLDER} */
let OptionalCvdType;

/** @enum {string} The Cvd Matrix can be pivot across different color axis:
 * DEFAULT uses the standard color rotation depending on CVD
 * while RED, GREEN, BLUE are used to override.
 * */
const CvdAxis = {
  DEFAULT: 'DEFAULT',
  RED: 'RED',
  GREEN: 'GREEN',
  BLUE: 'BLUE',
};

