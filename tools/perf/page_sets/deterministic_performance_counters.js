// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Monkey patch performance to get deterministic results.
(function () {
  // Web page replay patches Data.now().
  var start_time = Date.now();
  var now_counter = 0;
  function now() {
     return now_counter++;
  }
  function timing() {
    // These values are arbitrary.
    // They are obtained by sampling "performance.now()" on a live web site.
    return {
      "navigationStart": start_time,
      "unloadEventStart": 0,
      "unloadEventEnd": 0,
      "redirectStart": 0,
      "redirectEnd": 0,
      "fetchStart": start_time + 94,
      "domainLookupStart": start_time + 94,
      "domainLookupEnd": start_time + 94,
      "connectStart": start_time + 94,
      "connectEnd": start_time + 94,
      "secureConnectionStart":0,
      "requestStart": start_time + 98,
      "responseStart": start_time + 546,
      "responseEnd": start_time + 1511,
      "domLoading": start_time + 562,
      "domInteractive": start_time + 1647,
      "domContentLoadedEventStart": start_time + 1647,
      "domContentLoadedEventEnd": start_time + 1654,
      "domComplete": start_time + 2009,
      "loadEventStart": start_time + 2009,
      "loadEventEnd": start_time + 2009
    };
  }
  Object.defineProperty(window, "performance", {
    get : function() {
            return { 'now' : now, 'timing' : timing() };
          }
  });
})();
