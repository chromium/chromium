// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var backGradient;
function prepareBackground() {
  backGradient = canvasContext.createLinearGradient(0, 0, canvasWidth, 0);
  backGradient.addColorStop(0, "#FF4040");
  backGradient.addColorStop(1, "#40FF40");
}

function drawBackground() {
  canvasContext.save();
  canvasContext.fillStyle = backGradient;
  canvasContext.fillRect(0, 0, canvasWidth, canvasHeight);
  canvasContext.restore();
}
