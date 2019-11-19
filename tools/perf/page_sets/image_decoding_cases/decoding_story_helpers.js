// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Given |array| of base64 data or image paths, this function successively
 * replaces the source of <img id="img"> with elements
 * {0, 1, ..., |numSquares| - 1} for |numSquares| from the initial call.
 * This happens every 450 ms.
 */
function drawColorfulSquares(numSquares, array) {
  if (numSquares > 0) {
    setTimeout(function() {
      var image_width = 1024;
      var image_height = 1024;
      document.getElementById('img').src = array[numSquares - 1];
      drawColorfulSquares(numSquares - 1, array);
    }, 450);
  }
}
