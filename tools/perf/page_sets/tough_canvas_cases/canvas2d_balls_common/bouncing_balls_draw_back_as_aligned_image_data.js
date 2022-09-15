// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var backImageData;

function prepareBackground() {
  var backImage = document.getElementById('backImage');
  var backImageCanvas = document.createElement('canvas');
  backImageCanvas.width = canvasWidth;
  backImageCanvas.height = canvasHeight;
  var backContext = backImageCanvas.getContext('2d');
  backContext.drawImage(backImage, 0, 0, canvasWidth, canvasHeight);
  backImageData = backContext.getImageData(0, 0, canvasWidth, canvasHeight)
}

function drawBackground() {
  canvasContext.putImageData(backImageData, 0, 0);
}
