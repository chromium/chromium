/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

/* jshint esversion: 6 */

'use strict';

var $ = document.getElementById.bind(document);

var testTable = $('test-table');
var nPeerConnectionsInput = $('num-peerconnections');
var startTestButton = $('start-test');

startTestButton.onclick = startTest;

function logError(err) {
  console.log(err);
}

function addNewVideoElement() {
  var newRow = testTable.insertRow(-1);
  var newCell = newRow.insertCell(-1);
  var video = document.createElement('video');
  video.autoplay = true;
  newCell.appendChild(video);
  return video;
}

function PeerConnection(id) {
  this.id = id;

  this.localConnection = null;
  this.remoteConnection = null;

  this.remoteView = addNewVideoElement();

  this.start = function() {
    var onGetUserMediaSuccess = this.onGetUserMediaSuccess.bind(this);
    navigator.mediaDevices.getUserMedia({
      audio: true,
      video: true
    })
      .then(onGetUserMediaSuccess)
      .catch(logError);
  };

  this.onGetUserMediaSuccess = function(stream) {
    // Create local peer connection.
    this.localConnection = new RTCPeerConnection({
      'encodedInsertableStreams': true,
    });
    this.localConnection.onicecandidate = (event) => {
      this.onIceCandidate(this.remoteConnection, event);
    };
    stream.getTracks().forEach(track => {
      const sender = this.localConnection.addTrack(track);
      const {readable, writable} = sender.createEncodedStreams();
      readable.pipeTo(writable);
    });

    // Create remote peer connection.
    this.remoteConnection = new RTCPeerConnection({
      'encodedInsertableStreams': true,
    });
    this.remoteConnection.onicecandidate = (event) => {
      this.onIceCandidate(this.localConnection, event);
    };
    this.remoteView.srcObject = new MediaStream();
    this.remoteConnection.ontrack = (e) => {
      this.remoteView.srcObject.addTrack(e.track);
      const {readable, writable} = e.receiver.createEncodedStreams();
      readable.pipeTo(writable);
    };

    // Initiate call.
    var onCreateOfferSuccess = this.onCreateOfferSuccess.bind(this);
    this.localConnection.createOffer({
      offerToReceiveAudio: 1,
      offerToReceiveVideo: 1
    })
      .then(onCreateOfferSuccess, logError);
  };

  this.onCreateOfferSuccess = function(desc) {
    this.localConnection.setLocalDescription(desc);
    this.remoteConnection.setRemoteDescription(desc);

    var onCreateAnswerSuccess = this.onCreateAnswerSuccess.bind(this);
    this.remoteConnection.createAnswer()
      .then(onCreateAnswerSuccess, logError);
  };

  this.onCreateAnswerSuccess = function(desc) {
    this.remoteConnection.setLocalDescription(desc);
    this.localConnection.setRemoteDescription(desc);
  };

  this.onIceCandidate = function(connection, event) {
    if (event.candidate) {
      connection.addIceCandidate(new RTCIceCandidate(event.candidate));
    }
  };
}

function startTest() {
  var nPeerConnections = nPeerConnectionsInput.value;
  for (var i = 0; i < nPeerConnections; ++i) {
    new PeerConnection(i).start();
  }
}
