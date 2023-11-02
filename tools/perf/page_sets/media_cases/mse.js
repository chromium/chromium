// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file runs a series of Media Source Entensions (MSE) operations on a
// video tag to set up a media file for playback. The test takes several URL
// parameters described in the loadTestParams() function.

(() => {
  // Map from media content to MIME type. All test content must be added to this
  // map. (Feel free to extend it for your test case!)
  const MEDIA_MIMES = {
    'aac_audio.mp4': 'audio/mp4; codecs="mp4a.40.2"',
    'h264_video.mp4': 'video/mp4; codecs="avc1.640028"',
    'tulip0.av1.mp4': 'video/mp4; codecs="av01.0.05M.08"',
    'tulip2.vp9.webm': 'video/webm; codecs="opus,vp9"',
  };
  const testParams = {}

  function test() {
    loadTestParams();
    if (testParams.waitForPageLoaded) {
      document.body.onload = () => {
        runTest();
      }
    } else {
      runTest();
    }
  }

  function loadTestParams() {
    var queryParameters = parseQueryParameters();
    // waitForPageLoaded determines whether to wait for body.onload event or
    // to start right away.
    testParams.waitForPageLoaded =
        (queryParameters['waitForPageLoaded'] === 'true');
    // startOffset is used to start the media at an offset instead of at the
    // beginning of the file.
    testParams.startOffset = parseInt(queryParameters['startOffset'] || '0');
    // appendSize determines how large a chunk of the media file to append.
    testParams.appendSize = parseInt(queryParameters['appendSize'] || '128000');
    // media argument lists the media files to play.
    testParams.media = queryParameters['media'];
    if (!testParams.media)
      throw Error('media parameter must be defined to provide test content');
    if (!Array.isArray(testParams.media))
      testParams.media = [testParams.media];
  }

  function parseQueryParameters() {
    var params = {};
    var r = /([^&=]+)=([^&]*)/g;
    var match;
    while (match = r.exec(window.location.search.substring(1))) {
      key = decodeURIComponent(match[1])
      value = decodeURIComponent(match[2]);
      if (value.includes(',')) {
        value = value.split(',');
      }
      params[key] = value;
    }
    return params;
  }

  function runTest() {
    let appenders = [];
    let mediaElement = document.getElementById('video_id');
    let mediaSource = new window.MediaSource();
    window.__mediaSource = mediaSource;

    // Pass the test if currentTime of the media increases since that means that
    // the file has started playing.
    // This code can be modified in the future for full playback tests.
    mediaElement.addEventListener('timeupdate', () => {
      window.clearTimeout(timeout);
      PassTest('Test completed after timeupdate event was received.')
    }, {once: true});

    // Also pass the test if ended occurs; since we're appending small chunks
    // there are cases where 'timeupdate' may not necessarily fire.
    mediaElement.addEventListener('ended', () => {
      window.clearTimeout(timeout);
      if (mediaElement.currentTime > 0)
        PassTest('Test completed after ended event was received.')
      else
        FailTest('Test failed because ended occured before currentTime > 0.')
    }, {once: true});

    // Fail the test if we time out.
    var timeout = setTimeout(function() {
      FailTest('Test timed out waiting for a timeupdate or ended event.');
    }, 10000);

    mediaSource.addEventListener('sourceopen', (open_event) => {
      let mediaSource = open_event.target;
      for (let i = 0; i < appenders.length; ++i) {
        appenders[i].onSourceOpen(mediaSource);
      }

      // Append each segment and wait for the append to complete.
      let num_complete_appends = 0;
      for (let i = 0; i < appenders.length; ++i) {
        appenders[i].attemptAppend(() => {
          num_complete_appends++;
          if (num_complete_appends === testParams.media.length) {
            mediaSource.endOfStream();
            mediaElement.play();
          }
        });
      }
    });

    // Do not attach MediaSource object until all the buffer appenders have
    // received the data from the network that they'll append. This removes
    // the factor of network overhead from the attachment timing.
    let number_of_appenders_with_data = 0;
    for (const media_file of testParams.media) {
      appender = new BufferAppender(media_file, MEDIA_MIMES[media_file]);
      appender.requestMediaBytes(() => {
        number_of_appenders_with_data++;
        if (number_of_appenders_with_data === testParams.media.length) {
          // This attaches the mediaSource object to the mediaElement. Once this
          // operation has completed internally, the mediaSource object
          // readyState will transition from closed to open, and the sourceopen
          // event will fire.
          mediaElement.src = URL.createObjectURL(mediaSource);
        }
      });
      appenders.push(appender);
    }
  }

  class BufferAppender {
    constructor(media_file, mimetype) {
      this.media_file = media_file;
      this.mimetype = mimetype;
      this.xhr = new XMLHttpRequest();
      this.sourceBuffer = null;
    }
    requestMediaBytes(callback) {
      this.xhr.addEventListener('loadend', callback, {once: true});
      this.xhr.open('GET', this.media_file);
      this.xhr.setRequestHeader(
          'Range', 'bytes=' + testParams.startOffset + '-' +
          (testParams.startOffset + testParams.appendSize - 1));
      this.xhr.responseType = 'arraybuffer';
      this.xhr.send();
    }
    onSourceOpen(mediaSource) {
      if (this.sourceBuffer)
        return;
      this.sourceBuffer = mediaSource.addSourceBuffer(this.mimetype);
    }
    attemptAppend(callback) {
      if (!this.xhr.response || !this.sourceBuffer)
        return;
      this.sourceBuffer.addEventListener('updateend', callback, {once: true});
      this.sourceBuffer.appendBuffer(this.xhr.response);
      this.xhr = null;
    }
  } // End BufferAppender

  function PassTest(message) {
    console.log('Test passed: ' + message);
    window.__testDone = true;
  }

  function FailTest(error_message) {
    console.error('Test failed: ' + error_message);
    window.__testFailed = true;
    window.__testError = error_message;
    window.__testDone = true;
  }

  window.onerror = (messageOrEvent, source, lineno, colno, error) => {
    // Fail the test if there are any errors. See crbug/777489.
    // Note that error.stack already contains error.message.
    FailTest(error.stack);
  }

  window.__test = test;
  // These are outputs to be consumed by media Telemetry test driver code.
  window.__testDone = false;
  window.__testFailed = false;
  window.__testError = '';
})();
