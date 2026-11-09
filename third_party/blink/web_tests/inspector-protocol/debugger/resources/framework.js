// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A framework for testing.

var Framework = {};

Framework.safeRun = function(callback, onSuccess, onException,
                             breakOnUncaught) {
  try {
    callback();
    if (onSuccess)
      Framework.safeRun(onSuccess, undefined, onException, breakOnUncaught);
  } catch (e) {
    if (onException)
      Framework.safeRun(
          onException, undefined,
          breakOnUncaught ? Framework.breakInFramework : undefined);
    else if (breakOnUncaught)
      Framework.breakInFramework();
  }
};

Framework.throwFrameworkException = function(msg) {
  throw Error('FrameworkException' + (msg ? ': ' + msg : ''));
};

Framework.breakInFramework = function() {
  debugger;
};

Framework.empty = function() {};

Framework.schedule = function(callback, delay) {
  setTimeout(callback, delay || 0);
};

Framework.willSchedule = function(callback, delay) {
  return function Framework_willSchedule() {
    return Framework.schedule(callback, delay);
  };
};

Framework.doSomeAsyncChainCalls = function(callback) {
  var func1 = Framework.willSchedule(function Framework_inner1() {
    if (callback)
      callback();
  });
  var func2 = Framework.willSchedule(function Framework_inner2() {
    if (window.callbackFromFramework)
      window.callbackFromFramework(func1);
    else
      func1();
  });
  Framework.schedule(func2);
};

Framework.throwInNative = function() {
  var wrongJson = '})';
  window['dummy'] = JSON.parse(wrongJson);
};

Framework.throwInNativeAndCatch = function() {
  try {
    Framework.throwInNative();
  } catch (e) {
  }
};

Framework.throwFrameworkExceptionAndCatch = function() {
  try {
    Framework.throwFrameworkException();
  } catch (e) {
  }
};
