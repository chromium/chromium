// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Minimal externs file provided for places in the code that
 * still use JavaScript instead of TypeScript.
 * @externs
 */

/**
 * @constructor
 * @extends {HTMLElement}
 */
function LottieRenderer() {}

LottieRenderer.prototype.play = function() {};
LottieRenderer.prototype.pause = function() {};
LottieRenderer.prototype.refreshAnimationColors = function() {};
