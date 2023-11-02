// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var __c = ""; // that's good enough for me.
var __td;
var __tf;
var __tl;
var __iterations;
var __cycle;
var __results = false;
var __page;
var __TIMEOUT = 15;
function __get_cookie(name) {
  var cookies = document.cookie.split("; ");
  for (var i = 0; i < cookies.length; ++i) {
    var t = cookies[i].split("=");
    if (t[0] == name && t[1])
      return t[1];
  }
  return "";
}
function __pages() {  // fetch lazily
  if (!("data" in this))
    this.data = __get_cookie("__pc_pages").split(",");
  return this.data;
}
function __get_timings() {
  if (sessionStorage != null &&
      sessionStorage.getItem("__pc_timings") != null) {
    return sessionStorage["__pc_timings"];
  } else {
    return __get_cookie("__pc_timings");
  }
}
function __set_timings(timings) {
  if (sessionStorage == null)
    document.cookie = "__pc_timings=" + timings + "; path=/";
  else
    sessionStorage["__pc_timings"]=timings;
}
function __ontimeout() {
  var doc;

  // Call GC twice to cleanup JS heap before starting a new test.
  if (window.gc) {
    window.gc();
    window.gc();
  }

  var timings = __tl;
  var oldTimings = __get_timings();
  if (oldTimings != "") {
    timings = oldTimings + "," + timings;
  }
  __set_timings(timings);

  var ts = (new Date()).getTime();
  var tlag = (ts - __te) - __TIMEOUT;
  if (tlag > 0)
    __tf = __tf + tlag;
  if (__cycle == (__pages().length * __iterations)) {
    document.cookie = "__pc_done=1; path=/";
    doc = "../../common/report.html";
    if (window.console) {
      console.log("Pages: [" + __get_cookie('__pc_pages') + "]");
      console.log("times: [" + __get_timings() + "]");
    }
  } else {
    doc = "../" + __pages()[__page] + "/index.html";
  }

  var url = doc + "?n=" + __iterations + "&i=" + __cycle + "&p=" + __page +
            "&ts=" + ts + "&td=" + __td + "&tf=" + __tf;
  document.location.href = url;
}
function __onload() {
  if (__results) {
    // Set a variable to indicate that the result report page is loaded.
    document.cookie = "__navigated_to_report=1; path=/";
    return;
  }
  var unused = document.body.offsetHeight;  // force layout

  var ts = 0, td = 0, te = (new Date()).getTime(), tf = 0;

  var s = document.location.search;
  if (s) {
    var params = s.substring(1).split('&');
    for (var i = 0; i < params.length; ++i) {
      var f = params[i].split('=');
      switch (f[0]) {
      case 'skip':
        // No calculation, just viewing
        return;
      case 'n':
        __iterations = f[1];
        break;
      case 'i':
        __cycle = (f[1] - 0) + 1;
        break;
      case 'p':
        __page = ((f[1] - 0) + 1) % __pages().length;
        break;
      case 'ts':
        ts = (f[1] - 0);
        break;
      case 'td':
        td = (f[1] - 0);
        break;
      case 'tf':
        tf = (f[1] - 0);
        break;
      }
    }
  }
  __tl = (te - ts);
  __td = td + __tl;
  __te = te;
  __tf = tf;  // record t-fudge

  setTimeout("__ontimeout()", __TIMEOUT);
}

if (window.attachEvent)
  window.attachEvent("onload", __onload);
else
  addEventListener("load", __onload, false);
