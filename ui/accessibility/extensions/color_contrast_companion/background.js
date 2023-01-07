// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stream = null;
var ui = null;

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  console.log('Got message');
  console.log(request);
  if (request.close && ui) {
    chrome.windows.remove(ui.id);
  }
});

function capture() {
  let video = document.createElement('video');
  video.autoplay = true;
  document.body.appendChild(video);
  video.addEventListener('canplay', () => {
    if (video.videoWidth < 100) {
      // We probably need the permission again.
      getStream();
      return;
    }

    window.setTimeout(() => {
      let canvas = document.createElement('canvas');
      canvas.height = video.videoHeight;
      canvas.width = video.videoWidth;
      var context = canvas.getContext('2d');
      context.drawImage(video, 0, 0, canvas.width, canvas.height);
      var imageDataUrl = canvas.toDataURL();
      document.body.removeChild(video);

      // Close the stream so it stops using resources.
      let tracks = stream.getTracks();
      tracks.forEach(function(track) {
        track.stop();
      });
      stream = null;

      chrome.windows.create(
          {
            'url': chrome.runtime.getURL('ui.html'),
            'focused': true,
            'type': 'popup',
            'state': 'fullscreen'
          },
          (win) => {
            ui = win;
            var tab = win.tabs[0];
            window.setTimeout(() => {
              console.log('Sending message');
              chrome.tabs.sendMessage(tab.id, {'imageDataUrl': imageDataUrl});
            }, 250);
          });
    }, 250);
  });
  video.srcObject = stream;
}

function getStream() {
  chrome.desktopCapture.chooseDesktopMedia(['screen'], (streamId) => {
    let video = document.createElement('video');
    navigator.mediaDevices
        .getUserMedia({
          video: {
            mandatory: {
              chromeMediaSource: 'desktop',
              chromeMediaSourceId: streamId,
            }
          }
        })
        .then(returnedStream => {
          stream = returnedStream;
          capture();
        });
  });
}

chrome.browserAction.onClicked.addListener(() => {
  if (!stream) {
    getStream();
    return;
  }

  capture();
});

var alreadyShowedHelp = localStorage.getItem('help');
if (!alreadyShowedHelp) {
  localStorage.setItem('help', 'true');
  chrome.windows.create(
      {'url': chrome.runtime.getURL('help.html'), 'focused': true});
}
