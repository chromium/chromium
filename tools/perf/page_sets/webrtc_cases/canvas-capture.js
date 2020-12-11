/*
*  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree.
*/

'use strict';

const DEFAULT_FRAME_RATE = 30;

var canvas = document.getElementById('canvas');
var context = canvas.getContext('2d');

var remoteVideo = document.getElementById('remoteVideo');
var startButton = document.getElementById('startButton');
startButton.onclick = start;

var pc1;
var pc2;
var stream;

function logError(err) {
  console.error(err);
}

// This function draws a red rectangle on the canvas using
// requestAnimationFrame().
function draw() {
  window.requestAnimationFrame(draw);
  context.rect(0, 0, canvas.clientWidth, canvas.clientHeight);
  var randomNumber = Math.random();
  var hue;
  if (randomNumber < 0.33) {
    hue = 'red';
  } else if (randomNumber < 0.66) {
    hue = 'green';
  } else {
    hue = 'blue';
  }
  context.fillStyle = hue;
  context.fill();
}

function start() {
  startButton.onclick = hangup;
  startButton.className = 'red';
  startButton.innerHTML = 'Stop test';
  draw();
  stream = canvas.captureStream(DEFAULT_FRAME_RATE);
  call();
}

function call() {
  var servers = null;
  pc1 = new RTCPeerConnection(servers);
  pc1.onicecandidate = (event) => {
    if (event.candidate) {
      pc2.addIceCandidate(event.candidate);
    }
  };

  pc2 = new RTCPeerConnection(servers);
  pc2.onicecandidate = (event) => {
    if (event.candidate) {
      pc1.addIceCandidate(event.candidate);
    }
  };
  pc2.onaddstream = (event) => {
    remoteVideo.srcObject = event.stream;
  };

  pc1.addStream(stream);
  pc1.createOffer({
    offerToReceiveAudio: 1,
    offerToReceiveVideo: 1
  }).then(onCreateOfferSuccess, logError);
}

function onCreateOfferSuccess(desc) {
  pc1.setLocalDescription(desc);
  pc2.setRemoteDescription(desc);
  pc2.createAnswer().then(onCreateAnswerSuccess, logError);
}

function onCreateAnswerSuccess(desc) {
  pc2.setLocalDescription(desc);
  pc1.setRemoteDescription(desc);
}

function hangup() {
  pc1.close();
  pc2.close();
  pc1 = null;
  pc2 = null;
  startButton.onclick = start;
  startButton.className = 'green';
  startButton.innerHTML = 'Start test';
}
