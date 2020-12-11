/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

'use strict';

var startButton = document.getElementById('startButton');
var callButton = document.getElementById('callButton');
var hangupButton = document.getElementById('hangupButton');
callButton.disabled = true;
hangupButton.disabled = true;
startButton.onclick = start;
callButton.onclick = startTest;
hangupButton.onclick = hangup;

var startTime;
var localVideo = document.getElementById('localVideo');
var remoteVideo = document.getElementById('remoteVideo');

function trace(arg) {
  var now = (window.performance.now() / 1000).toFixed(3);
  console.log(now + ': ', arg);
}

function getSelectedVideoCodec() {
  var codec;
  if (document.getElementById('H264').checked) {
    codec = document.getElementById('H264').value;
  } else if (document.getElementById('VP8').checked) {
    codec = document.getElementById('VP8').value;
  } else {
    codec = document.getElementById('VP9').value;
  }
  return codec;
}

function Failure(message) {
  this.message = message;
  this.name = 'Failure';
}

localVideo.addEventListener('loadedmetadata', function() {
  trace('Local video videoWidth: ' + this.videoWidth +
    'px,  videoHeight: ' + this.videoHeight + 'px');
});

remoteVideo.addEventListener('loadedmetadata', function() {
  trace('Remote video videoWidth: ' + this.videoWidth +
    'px,  videoHeight: ' + this.videoHeight + 'px');
});

remoteVideo.onresize = function() {
  trace('Remote video size changed to ' +
    remoteVideo.videoWidth + 'x' + remoteVideo.videoHeight);
  // We'll use the first onsize callback as an indication that video has started
  // playing out.
  if (startTime) {
    var elapsedTime = window.performance.now() - startTime;
    trace('Setup time: ' + elapsedTime.toFixed(3) + 'ms');
    startTime = null;
  }
};

var localStream;
var pc1;
var pc2;
var offerOptions = {
  offerToReceiveAudio: 1,
  offerToReceiveVideo: 1
};

function getName(pc) {
  return (pc === pc1) ? 'pc1' : 'pc2';
}

function getOtherPc(pc) {
  return (pc === pc1) ? pc2 : pc1;
}

function gotStream(stream) {
  trace('Received local stream');
  localVideo.srcObject = stream;
  localStream = stream;
  callButton.disabled = false;
}

function start() {
  trace('Requesting local stream');
  startButton.disabled = true;
  navigator.mediaDevices.getUserMedia({
    audio: true,
    video: true
  })
    .then(gotStream)
    .catch(function(e) {
      alert('getUserMedia() error: ' + e.name);
    });
}

function call() {
  callButton.disabled = true;
  hangupButton.disabled = false;
  trace('Starting call');
  startTime = window.performance.now();
  var videoTracks = localStream.getVideoTracks();
  var audioTracks = localStream.getAudioTracks();
  if (videoTracks.length > 0) {
    trace('Using video device: ' + videoTracks[0].label);
  }
  if (audioTracks.length > 0) {
    trace('Using audio device: ' + audioTracks[0].label);
  }
  var servers = null;
  pc1 = new RTCPeerConnection(servers);
  trace('Created local peer connection object pc1');
  pc1.onicecandidate = function(e) {
    onIceCandidate(pc1, e);
  };
  pc2 = new RTCPeerConnection(servers);
  trace('Created remote peer connection object pc2');
  pc2.onicecandidate = function(e) {
    onIceCandidate(pc2, e);
  };
  pc1.oniceconnectionstatechange = function(e) {
    onIceStateChange(pc1, e);
  };
  pc2.oniceconnectionstatechange = function(e) {
    onIceStateChange(pc2, e);
  };
  pc2.ontrack = gotRemoteStream;

  localStream.getTracks().forEach(
    function(track) {
      pc1.addTrack(
        track,
        localStream
      );
    }
  );
  trace('Added local stream to pc1');

  trace('pc1 createOffer start');
  pc1.createOffer(
    offerOptions
  ).then(
    onCreateOfferSuccess,
    onCreateSessionDescriptionError
  );
}

function onCreateSessionDescriptionError(error) {
  trace('Failed to create session description: ' + error.toString());
}

/**
 * See |setSdpDefaultCodec|.
 */
function setSdpDefaultVideoCodec(sdp, codec, preferHwCodec) {
  return setSdpDefaultCodec(sdp, 'video', codec, preferHwCodec);
}

/**
 * Returns a modified version of |sdp| where the |codec| has been promoted to be
 * the default codec, i.e. the codec whose ID is first in the list of codecs on
 * the 'm=|type|' line, where |type| is 'audio' or 'video'. If |preferHwCodec|
 * is true, it will select the last codec with the given name, and if false, it
 * will select the first codec with the given name, because HW codecs are listed
 * after SW codecs in the SDP list.
 * @private
 */
function setSdpDefaultCodec(sdp, type, codec, preferHwCodec) {
  var sdpLines = splitSdpLines(sdp);

  // Find codec ID, e.g. 100 for 'VP8' if 'a=rtpmap:100 VP8/9000'.
  var codecId = findRtpmapId(sdpLines, codec, preferHwCodec);
  if (codecId === null) {
    throw new Failure('setSdpDefaultCodec',
      'Unknown ID for |codec| = \'' + codec + '\'.');
  }

  // Find 'm=|type|' line, e.g. 'm=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116'.
  var mLineNo = findLine(sdpLines, 'm=' + type);
  if (mLineNo === null) {
    throw new Failure('setSdpDefaultCodec',
      '\'m=' + type + '\' line missing from |sdp|.');
  }

  // Modify video line to use the desired codec as the default.
  sdpLines[mLineNo] = setMLineDefaultCodec(sdpLines[mLineNo], codecId);
  return mergeSdpLines(sdpLines);
}

/**
 * Searches through all |sdpLines| for the 'a=rtpmap:' line for the codec of
 * the specified name, returning its ID as an int if found, or null otherwise.
 * |codec| is the case-sensitive name of the codec. If |lastInstance|
 * is true, it will return the last such ID, and if false, it will return the
 * first such ID.
 * For example, if |sdpLines| contains 'a=rtpmap:100 VP8/9000' and |codec| is
 * 'VP8', this function returns 100.
 * @private
 */
function findRtpmapId(sdpLines, codec, lastInstance) {
  var lineNo = findRtpmapLine(sdpLines, codec, lastInstance);
  if (lineNo === null) {
    return null;
  }
  // Parse <id> from 'a=rtpmap:<id> <codec>/<rate>'.
  var id = sdpLines[lineNo].substring(9, sdpLines[lineNo].indexOf(' '));
  return parseInt(id);
}

/**
 * Finds a 'a=rtpmap:' line from |sdpLines| that contains |contains| and returns
 * its line index, or null if no such line was found. |contains| may be the
 * codec ID, codec name or bitrate. If |lastInstance| is true, it will return
 * the last such line index, and if false, it will return the first such line
 * index.
 * An 'a=rtpmap:' line looks like this: 'a=rtpmap:<id> <codec>/<rate>'.
 */
function findRtpmapLine(sdpLines, contains, lastInstance) {
  if (lastInstance === true) {
    for (var i = sdpLines.length - 1; i >= 0 ; i--) {
      if (isRtpmapLine(sdpLines[i], contains)) {
        return i;
      }
    }
  } else {
    for (i = 0; i < sdpLines.length; i++) {
      if (isRtpmapLine(sdpLines[i], contains)) {
        return i;
      }
    }
  }
  return null;
}

/**
 * Returns true if |sdpLine| contains |contains| and is of pattern
 * 'a=rtpmap:<id> <codec>/<rate>'.
 */
function isRtpmapLine(sdpLine, contains) {
  // Is 'a=rtpmap:' line containing |contains| string?
  if (sdpLine.startsWith('a=rtpmap:') &&
      sdpLine.indexOf(contains) !== -1) {
    // Expecting pattern 'a=rtpmap:<id> <codec>/<rate>'.
    var pattern = new RegExp('a=rtpmap:(\\d+) \\w+\\/\\d+');
    if (!sdpLine.match(pattern)) {
      throw new Failure('isRtpmapLine', 'Unexpected "a=rtpmap:" pattern.');
    }
    return true;
  }
  return false;
}

/**
 * Returns a modified version of |mLine| that has |codecId| first in the list of
 * codec IDs. For example, setMLineDefaultCodec(
 *     'm=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116 117 96', 107)
 * Returns:
 *     'm=video 9 UDP/TLS/RTP/SAVPF 107 100 101 116 117 96'
 * @private
 */
function setMLineDefaultCodec(mLine, codecId) {
  var elements = mLine.split(' ');

  // Copy first three elements, codec order starts on fourth.
  var newLine = elements.slice(0, 3);

  // Put target |codecId| first and copy the rest.
  newLine.push(codecId);
  for (var i = 3; i < elements.length; i++) {
    if (elements[i] !== codecId) {
      newLine.push(elements[i]);
    }
  }

  return newLine.join(' ');
}

/** @private */
function splitSdpLines(sdp) {
  return sdp.split('\r\n');
}

/** @private */
function mergeSdpLines(sdpLines) {
  return sdpLines.join('\r\n');
}

/** @private */
function findLine(lines, lineStartsWith, startingLine = 0) {
  for (var i = startingLine; i < lines.length; i++) {
    if (lines[i].startsWith(lineStartsWith)) {
      return i;
    }
  }
  return null;
}

function onCreateOfferSuccess(desc) {
  var videoCodec = getSelectedVideoCodec();
  desc.sdp = setSdpDefaultVideoCodec(desc.sdp, videoCodec, videoCodec);
  trace('Offer from pc1\n' + desc.sdp);
  trace('Ok-' + JSON.stringify(desc));
  trace('pc1 setLocalDescription start');
  pc1.setLocalDescription(desc).then(
    function() {
      onSetLocalSuccess(pc1);
    },
    onSetSessionDescriptionError
  );
  trace('pc2 setRemoteDescription start');
  pc2.setRemoteDescription(desc).then(
    function() {
      onSetRemoteSuccess(pc2);
    },
    onSetSessionDescriptionError
  );
  trace('pc2 createAnswer start');
  // Since the 'remote' side has no media stream we need
  // to pass in the right constraints in order for it to
  // accept the incoming offer of audio and video.
  pc2.createAnswer().then(
    onCreateAnswerSuccess,
    onCreateSessionDescriptionError
  );
}

function onSetLocalSuccess(pc) {
  trace(getName(pc) + ' setLocalDescription complete');
}

function onSetRemoteSuccess(pc) {
  trace(getName(pc) + ' setRemoteDescription complete');
}

function onSetSessionDescriptionError(error) {
  trace('Failed to set session description: ' + error.toString());
}

function startTest() {
  call();
  setInterval(() => {
    pc1.getStats((response) => {
      trace(response);
    });
  }, 10 * 1000);
}

function gotRemoteStream(e) {
  if (remoteVideo.srcObject !== e.streams[0]) {
    remoteVideo.srcObject = e.streams[0];
    trace('pc2 received remote stream');
  }
}

function onCreateAnswerSuccess(desc) {
  trace('Answer from pc2:\n' + desc.sdp);
  trace('pc2 setLocalDescription start');
  pc2.setLocalDescription(desc).then(
    function() {
      onSetLocalSuccess(pc2);
    },
    onSetSessionDescriptionError
  );
  trace('pc1 setRemoteDescription start');
  pc1.setRemoteDescription(desc).then(
    function() {
      onSetRemoteSuccess(pc1);
    },
    onSetSessionDescriptionError
  );
}

function onIceCandidate(pc, event) {
  getOtherPc(pc).addIceCandidate(event.candidate)
    .then(
      function() {
        onAddIceCandidateSuccess(pc);
      },
      function(err) {
        onAddIceCandidateError(pc, err);
      }
    );
  trace(getName(pc) + ' ICE candidate: \n' + (event.candidate ?
    event.candidate.candidate : '(null)'));
}

function onAddIceCandidateSuccess(pc) {
  trace(getName(pc) + ' addIceCandidate success');
}

function onAddIceCandidateError(pc, error) {
  trace(getName(pc) + ' failed to add ICE Candidate: ' + error.toString());
}

function onIceStateChange(pc, event) {
  if (pc) {
    trace(getName(pc) + ' ICE state: ' + pc.iceConnectionState);
    console.log('ICE state change event: ', event);
  }
}

function hangup() {
  trace('Ending call');
  pc1.close();
  pc2.close();
  pc1 = null;
  pc2 = null;
  hangupButton.disabled = true;
  callButton.disabled = false;
}
