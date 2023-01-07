// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var backImageCanvas;

function prepareBackground() {
  var backImage = document.getElementById('backImage');
  backImageCanvas = document.createElement('canvas');
  backImageCanvas.width = backImage.naturalWidth;
  backImageCanvas.height = backImage.naturalHeight;
  var backContext = backImageCanvas.getContext('2d');
  backContext.drawImage(backImage, 0, 0);
}

function drawBackground() {
  canvasContext.drawImage(backImageCanvas, 0, 0, canvasWidth, canvasHeight);
}
