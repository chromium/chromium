// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function prepareBackground() {
}

function drawBackground() {
  canvasContext.save();
  canvasContext.fillStyle = "#FFFFFF";
  canvasContext.fillRect(0, 0, canvasWidth, canvasHeight);
  canvasContext.restore();
}
