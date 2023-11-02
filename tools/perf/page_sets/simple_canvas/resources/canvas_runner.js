// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
    var MEASURE_DRAW_TIMES = 50;
    var MAX_MEASURE_DRAW_TIMES = 1000;
    var MAX_MEASURE_TIME_PER_FRAME = 1000; // 1 sec
    var currentTest = null;
    var isTestDone = false;

    var CanvasRunner = {};

    CanvasRunner.startPlayingAndWaitForVideo = function (video, callback) {
      var gotPlaying = false;
      var gotTimeUpdate = false;

      var maybeCallCallback = function() {
        if (gotPlaying && gotTimeUpdate && callback) {
          callback(video);
          callback = undefined;
          video.removeEventListener('playing', playingListener, true);
          video.removeEventListener('timeupdate', timeupdateListener, true);
        }
      };

      var playingListener = function() {
        gotPlaying = true;
        maybeCallCallback();
      };

      var timeupdateListener = function() {
        // Checking to make sure the current time has advanced beyond
        // the start time seems to be a reliable heuristic that the
        // video element has data that can be consumed.
        if (video.currentTime > 0.0) {
          gotTimeUpdate = true;
          maybeCallCallback();
        }
      };

      video.addEventListener('playing', playingListener, true);
      video.addEventListener('timeupdate', timeupdateListener, true);
      video.loop = true;
      video.play();
    }

    CanvasRunner.gc = function () {
      if (window.GCController)
        window.GCController.collectAll();
      else {
        function gcRec(n) {
          if (n < 1)
            return {};
          var temp = {i: "ab" + i + (i / 100000)};
          temp += "foo";
          gcRec(n-1);
        }
        for (var i = 0; i < 1000; i++)
          gcRec(10);
      }
    };
    window.CanvasRunner = CanvasRunner;
  })();
