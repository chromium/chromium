/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* exported startTest */

'use strict';

const $ = document.getElementById.bind(document);

function logError(err) {
  console.error(err);
}

/**
 * FeedTable stores all video elements.
 */
class FeedTable {
  constructor() {
    this.numCols = 5;
    this.col = 0;
    this.testTable = document.getElementById('test-table');
    this.row = this.testTable.insertRow(-1);
  }

  addNewCell(elementType) {
    if (this.col === this.numCols) {
      this.row = this.testTable.insertRow(-1);
      this.col = 0;
    }
    var newCell = this.row.insertCell(-1);
    var element = document.createElement(elementType);
    element.autoplay = false;
    newCell.appendChild(element);
    this.col++;
    return element;
  }
}

/**
 * A simple loopback connection;
 * - localConnection is fed video from local camera
 * - localConnection is linked to remoteConnection
 * - remoteConnection is displayed in the given videoElement
 */
class PeerConnection {
  /**
   * @param {Object} element - An 'audio' or 'video' element.
   * @param {Object} constraints - The constraints for the peer connection.
   */
  constructor(element, constraints) {
    this.localConnection = null;
    this.remoteConnection = null;
    this.remoteView = element;
    this.constraints = constraints;
  }

  start() {
    return navigator.mediaDevices
      .getUserMedia(this.constraints)
      .then((stream) => {
        this.onGetUserMediaSuccess(stream);
      });
  }

  onGetUserMediaSuccess(stream) {
    this.localConnection = new RTCPeerConnection(null);
    this.localConnection.onicecandidate = (event) => {
      this.onIceCandidate(this.remoteConnection, event);
    };
    this.localConnection.addStream(stream);

    this.remoteConnection = new RTCPeerConnection(null);
    this.remoteConnection.onicecandidate = (event) => {
      this.onIceCandidate(this.localConnection, event);
    };
    this.remoteConnection.onaddstream = (e) => {
      this.remoteView.srcObject = e.stream;
    };

    this.localConnection
      .createOffer({offerToReceiveAudio: 1, offerToReceiveVideo: 1})
      .then((offerDesc) => {
        this.onCreateOfferSuccess(offerDesc);
      }, logError);
  }

  onCreateOfferSuccess(offerDesc) {
    this.localConnection.setLocalDescription(offerDesc);
    this.remoteConnection.setRemoteDescription(offerDesc);

    this.remoteConnection.createAnswer().then(
      (answerDesc) => {
        this.onCreateAnswerSuccess(answerDesc);
      }, logError);
  }

  onCreateAnswerSuccess(answerDesc) {
    this.remoteConnection.setLocalDescription(answerDesc);
    this.localConnection.setRemoteDescription(answerDesc);
  }

  onIceCandidate(connection, event) {
    if (event.candidate) {
      connection.addIceCandidate(new RTCIceCandidate(event.candidate));
    }
  }
}

class TestRunner {
  constructor(runtimeSeconds, pausePlayIterationDelayMillis) {
    this.runtimeSeconds = runtimeSeconds;
    this.pausePlayIterationDelayMillis = pausePlayIterationDelayMillis;
    this.elements = [];
    this.peerConnections = [];
    this.feedTable = new FeedTable();
    this.iteration = 0;
    this.startTime = null;
    this.lastIterationTime = null;
  }

  addPeerConnection(elementType) {
    const element = this.feedTable.addNewCell(elementType);
    const constraints = {audio: true};
    if (elementType === 'video') {
      constraints.video = {
        width: {exact: 300}
      };
    } else if (elementType === 'audio') {
      constraints.video = false;
    } else {
      throw new Error('elementType must be one of "audio" or "video"');
    }
    this.elements.push(element);
    this.peerConnections.push(new PeerConnection(element, constraints));
  }

  startTest() {
    this.startTime = Date.now();
    let promises = testRunner.peerConnections.map((conn) => conn.start());
    Promise.all(promises)
      .then(() => {
        this.startTime = Date.now();
        this.pauseAndPlayLoop();
      })
      .catch((e) => {
        throw e;
      });
  }

  pauseAndPlayLoop() {
    this.iteration++;
    this.elements.forEach((feed) => {
      if (Math.random() >= 0.5) {
        feed.play();
      } else {
        feed.pause();
      }
    });
    const status = this.getStatus();
    this.lastIterationTime = Date.now();
    $('status').textContent = status;
    if (status !== 'ok-done') {
      setTimeout(
        () => {
          this.pauseAndPlayLoop();
        }, this.pausePlayIterationDelayMillis);
    } else { // We're done. Pause all feeds.
      this.elements.forEach((feed) => {
        feed.pause();
      });
    }
  }

  getStatus() {
    if (this.iteration === 0) {
      return 'not-started';
    }
    const timeSpent = Date.now() - this.startTime;
    if (timeSpent >= this.runtimeSeconds * 1000) {
      return 'ok-done';
    }
    return `running, iteration: ${this.iteration}`;
  }

  getResults() {
    const runTimeMillis = this.lastIterationTime - this.startTime;
    return {'runTimeSeconds': runTimeMillis / 1000};
  }
}

let testRunner;

function startTest(
  runtimeSeconds, numPeerConnections, pausePlayIterationDelayMillis,
  elementType) {
  testRunner = new TestRunner(
    runtimeSeconds, pausePlayIterationDelayMillis);
  for (let i = 0; i < numPeerConnections; i++) {
    testRunner.addPeerConnection(elementType);
  }
  testRunner.startTest();
}
