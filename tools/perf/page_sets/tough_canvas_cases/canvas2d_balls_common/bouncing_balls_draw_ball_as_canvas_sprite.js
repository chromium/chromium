// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var spriteMap;
var ballRadius;
var spriteMapSize = 50;

function drawBallInit(diameter) {
  ballRadius = diameter / 2;
  var ballImage = document.getElementById('ballImage');
  spriteMap = document.createElement("canvas");
  spriteMap.setAttribute('width', ballDiameter*spriteMapSize + 2);
  spriteMap.setAttribute('height', ballDiameter + 2);
  ctx = spriteMap.getContext("2d");
  ctx.clearRect(0,0, ballDiameter*spriteMapSize + 2, ballDiameter + 2);
  ctx.translate(ballRadius + 1, ballRadius + 1);
  for (var i = 0; i < spriteMapSize; i++) {
    ctx.save();
    ctx.rotate(i * 2 * Math.PI / spriteMapSize );
    ctx.drawImage(ballImage, -ballRadius, -ballRadius, ballDiameter,
        ballDiameter);
    ctx.restore();
    ctx.translate(ballDiameter, 0);
  }
}

function safeMod(a, b) {
  var q = Math.floor(a / b);
  return a - q*b;
}

function drawBall(x, y, angle) {
  canvasContext.save();
  canvasContext.translate(x, y);
  var idx = safeMod(Math.floor(angle * spriteMapSize / (2 * Math.PI)),
      spriteMapSize);
  canvasContext.drawImage(spriteMap, idx*ballDiameter + 1, 1, ballDiameter,
      ballDiameter, -ballRadius, -ballRadius, ballDiameter, ballDiameter);
  canvasContext.restore();
}
