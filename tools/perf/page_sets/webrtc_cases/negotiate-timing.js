/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

const startButton = document.getElementById('startButton');
const callButton = document.getElementById('callButton');
const renegotiateButton = document.getElementById('renegotiateButton');
const hangupButton = document.getElementById('hangupButton');
const log = document.getElementById('log');
const videoSectionsField = document.getElementById('videoSections');

callButton.disabled = true;
hangupButton.disabled = true;
renegotiateButton.disabled = true;
startButton.onclick = start;
callButton.onclick = call;
renegotiateButton.onclick = renegotiate;
hangupButton.onclick = hangup;

let startTime;
const localVideo = document.getElementById('localVideo');
const remoteVideo = document.getElementById('remoteVideo');

let audioTransceiver;
let audioImpairmentAtStart = 0;

let result;

localVideo.addEventListener('loadedmetadata', function() {
  console.log(`Local video videoWidth: ${this.videoWidth}px,  videoHeight: ${this.videoHeight}px`);
});

remoteVideo.addEventListener('loadedmetadata', function() {
  console.log(`Remote video videoWidth: ${this.videoWidth}px,  videoHeight: ${this.videoHeight}px`);
});

remoteVideo.onresize = () => {
  console.log(`Remote video size changed to ${remoteVideo.videoWidth}x${remoteVideo.videoHeight}`);
  console.warn('RESIZE', remoteVideo.videoWidth, remoteVideo.videoHeight);
  // We'll use the first onsize callback as an indication that video has started
  // playing out.
  if (startTime) {
    const elapsedTime = window.performance.now() - startTime;
    console.log(`Setup time: ${elapsedTime.toFixed(3)}ms`);
    startTime = null;
  }
};

let localStream;
let pc1;
let pc2;

function logToScreen(text) {
  log.append(document.createElement('br'));
  log.append(text);
}

function getName(pc) {
  return (pc === pc1) ? 'pc1' : 'pc2';
}

function getOtherPc(pc) {
  return (pc === pc1) ? pc2 : pc1;
}

async function start() {
  console.log('Requesting local stream');
  startButton.disabled = true;
  const stream = await navigator.mediaDevices
      .getUserMedia({
        audio: true,
        video: true
      });
  console.log('Received local stream');
  localVideo.srcObject = stream;
  localStream = stream;
  callButton.disabled = false;
}

async function runOfferAnswer() {
  const startTime = performance.now();
  const result = {};
  const offer = await pc1.createOffer();
  const markTime1 = performance.now();
  result.callerCreateOffer = markTime1 - startTime;
  await pc1.setLocalDescription(offer);
  const markTime2 = performance.now();
  result.callerSetLocalDescription = markTime2 - markTime1;
  await pc2.setRemoteDescription(offer);
  const markTime3 = performance.now();
  result.calleeSetRemoteDescription = markTime3 - markTime2;
  const answer = await pc2.createAnswer();
  const markTime4 = performance.now();
  result.calleeCreateAnswer = markTime4 - markTime3;
  await pc1.setRemoteDescription(answer);
  const markTime5 = performance.now();
  result.callerSetRemoteDescription = markTime5 - markTime4;
  await pc2.setLocalDescription(answer);
  const markTime6 = performance.now();
  result.calleeSetLocalDescription = markTime6 - markTime5;
  result.elapsedTime = markTime6 - startTime;
  return result;
}

async function call() {
  callButton.disabled = true;
  renegotiateButton.disabled = false;
  hangupButton.disabled = false;
  console.log('Starting call');
  startTime = window.performance.now();
  const audioTracks = localStream.getAudioTracks();
  if (audioTracks.length > 0) {
    console.log(`Using audio device: ${audioTracks[0].label}`);
  }
  const servers = null;
  pc1 = new RTCPeerConnection(servers);
  console.log('Created local peer connection object pc1');
  pc1.onicecandidate = e => onIceCandidate(pc1, e);
  pc2 = new RTCPeerConnection(servers);
  console.log('Created remote peer connection object pc2');
  pc2.onicecandidate = e => onIceCandidate(pc2, e);
  pc1.oniceconnectionstatechange = e => onIceStateChange(pc1, e);
  pc2.oniceconnectionstatechange = e => onIceStateChange(pc2, e);
  pc2.addEventListener('track', gotRemoteStream, {once: true});

  localStream.getTracks().forEach(track => pc1.addTrack(track, localStream));
  console.log('Added local stream to pc1');

  await runOfferAnswer();
  console.log('Initial negotiation complete');
}

function gotRemoteStream(e) {
  console.log('gotRemoteStream', e.track, e.streams[0]);
  if (e.streams[0]) {
    // reset srcObject to work around minor bugs in Chrome and Edge.
    remoteVideo.srcObject = null;
    remoteVideo.srcObject = e.streams[0];
  }
}

async function onIceCandidate(pc, event) {
  if (event.candidate) {
    console.log(`${getName(pc)} emitted ICE candidate for index ${event.candidate.sdpMLineIndex}:\n${event.candidate.candidate}`);
  } else {
    console.log(`$getName(pc)} ICE NULL candidate`);
  }
  await getOtherPc(pc).addIceCandidate(event.candidate);
  console.log(`${getName(pc)} addIceCandidate success`);
}

function onIceStateChange(pc, event) {
  if (pc) {
    console.log(`${getName(pc)} ICE state: ${pc.iceConnectionState}`);
    console.log('ICE state change event, state: ', pc.iceConnectionState);
  }
}

function adjustTransceiverCounts(pc, videoCount) {
  const currentVideoTransceivers = pc.getTransceivers().filter(tr => tr.receiver.track.kind == 'video');
  const currentVideoCount = currentVideoTransceivers.length;
  if (currentVideoCount < videoCount) {
    console.log('Adding ' + (videoCount - currentVideoCount) + ' transceivers');
    for (let i = currentVideoCount; i < videoCount; ++i) {
      pc.addTransceiver('video');
    }
  } else if (currentVideoCount > videoCount) {
    console.log('Stopping ' + (currentVideoCount - videoCount) + ' transceivers');
    for (let i = videoCount; i < currentVideoCount; ++i) {
      currentVideoTransceivers[i].stop();
    }
  } else {
    console.log(`No adjustment, video count is ${currentVideoCount}, target was ${videoCount}`);
  }
}

async function getAudioImpairment(audioTransceiver) {
  const stats = await audioTransceiver.receiver.getStats();
  let currentImpairment;
  stats.forEach(stat => {
    if (stat.type == 'inbound-rtp') {
      currentImpairment = stat.concealedSamples;
    }
  });
  console.log('Found impairment value ', currentImpairment);
  return currentImpairment;
}

async function baselineAudioImpairment(pc) {
  audioTransceiver = pc.getTransceivers().find(tr => tr.receiver.track.kind == 'audio');
  console.log('Found audio transceiver');
  audioImpairmentAtStart = await getAudioImpairment(audioTransceiver);
}

async function measureAudioImpairment(pc) {
  const startTime = performance.now();
  const audioImpairmentNow = await getAudioImpairment(audioTransceiver);
  console.log('Measurement took ' + (performance.now() - startTime) + ' msec');
  return audioImpairmentNow - audioImpairmentAtStart;
}


async function renegotiate() {
  renegotiateButton.disabled = true;
  adjustTransceiverCounts(pc1, parseInt(videoSectionsField.value));
  await baselineAudioImpairment(pc2);
  const previousVideoTransceiverCount = pc2.getTransceivers().filter(tr => tr.receiver.track.kind == 'video').length;
  result = await runOfferAnswer();
  console.log(`Renegotiate finished after ${result.elapsedTime} milliseconds`);
  const currentVideoTransceiverCount = pc2.getTransceivers().filter(tr => tr.receiver.track.kind == 'video').length;
  result.audioImpairment = await measureAudioImpairment(pc2);
  logToScreen(`Negotiation from ${previousVideoTransceiverCount} to ${currentVideoTransceiverCount} video transceivers took ${result.elapsedTime.toFixed(2)} milliseconds, audio impairment ${result.audioImpairment}`);
  console.log('Results: ', JSON.stringify(result, ' ', 2));
  renegotiateButton.disabled = false;
}

function hangup() {
  console.log('Ending call');
  pc1.close();
  pc2.close();
  pc1 = null;
  pc2 = null;

  console.log('Releasing camera');
  const videoTracks = localStream.getVideoTracks();
  videoTracks.forEach(videoTrack => {
    videoTrack.stop();
    localStream.removeTrack(videoTrack);
  });
  localVideo.srcObject = null;

  hangupButton.disabled = true;
  callButton.disabled = true;
  renegotiateButton.disabled = true;
  startButton.disabled = false;
}
