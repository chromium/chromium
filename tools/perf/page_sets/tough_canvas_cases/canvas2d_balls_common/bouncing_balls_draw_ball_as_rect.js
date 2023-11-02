// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ballRadius;

function drawBallInit(diameter) {
  ballRadius = diameter / 2;
}

function drawBall(x, y, angle) {
  canvasContext.save();
  canvasContext.fillStyle = 'blue';
  canvasContext.translate(x, y);
  canvasContext.rotate(angle);
  canvasContext.fillRect(-ballRadius, -ballRadius, ballDiameter, ballDiameter);
  canvasContext.restore();
}
