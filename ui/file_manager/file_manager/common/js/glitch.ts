// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordEnum} from './metrics.js';

/**
 * Possible error metrics. We allow for errors intercepted by an error counter.
 * These are unhandled errors and promise rejections.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in synch with FileManagerGlitch defined in
 * //tools/metrics/histograms/enums.xml
 */
export enum GlitchType {
  UNKNOWN = 0,
  UNHANDLED_ERROR = 1,
  UNHANDLED_REJECTION = 2,
  // Do not use it to report all caught exceptions. Only those exceptions that
  // we catch to work around errors that should never occur.
  CAUGHT_EXCEPTION = 3,
}

/**
 * @param glitchType What type of glitch was it.
 */
export function reportGlitch(glitchType: GlitchType) {
  recordEnum(`Glitch`, glitchType, Object.values(GlitchType));
}
