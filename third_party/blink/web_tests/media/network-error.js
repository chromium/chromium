// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Webm Opus init segment. Must be defined here since XHR can't be used to load
// local files; even in the layout test harness.
var OPUS_INIT_SEGMENT = "GkXfowEAAAAAAAAfQoaBAUL3gQFC8oEEQvOBCEKChHdlYm1Ch4EEQoWBAhhTgGcBAAAAAAAH6xFNm3RALE27i1OrhBVJqWZTrIHlTbuMU6uEFlSua1OsggEhTbuMU6uEElTDZ1OsggGP7AEAAAAAAACqAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAVSalmAQAAAAAAADAq17GDD0JATYCMTGF2ZjU4LjkuMTAwV0GMTGF2ZjU4LjkuMTAwRImIQGmAAAAAAAAWVK5rAQAAAAAAAGKuAQAAAAAAAFnXgQFzxYEBnIEAIrWcg3VuZIaGQV9PUFVTVqqDJiWgVruEBMS0AIOBAuEBAAAAAAAAEZ+BAbWIQOdwAAAAAABiZIEgY6KTT3B1c0hlYWQBAXgAgLsAAAAAABJUw2cBAAAAAAAAvHNzAQAAAAAAAC1jwAEAAAAAAAAAZ8gBAAAAAAAAGUWjh0VOQ09ERVJEh4xMYXZmNTguOS4xMDBzcwEAAAAAAAA3Y8ABAAAAAAAABGPFgQFnyAEAAAAAAAAfRaOHRU5DT0RFUkSHkkxhdmM1OC4xMi4xMDIgb3B1c3NzAQAAAAAAADpjwAEAAAAAAAAEY8WBAWfIAQAAAAAAACJFo4hEVVJBVElPTkSHlDAwOjAwOjAwLjIwNDAwMDAwMAAAH0O2dQEAAAAAAAWI54EAo/0=";

// Uses Media Source Extensions to generate a Network Error using EndOfStream.
function generateNetworkError(element) {
  var mediaSource = new MediaSource();
  mediaSource.addEventListener('sourceopen', function() {
    sourceBuffer = mediaSource.addSourceBuffer('audio/webm; codecs="opus"');
    sourceBuffer.appendBuffer(
        Uint8Array.from(window.atob(OPUS_INIT_SEGMENT), c => c.charCodeAt(0)));
  });

  element.addEventListener('loadedmetadata', function() {
    mediaSource.endOfStream("network");
  });

  element.src = window.URL.createObjectURL(mediaSource);
}
