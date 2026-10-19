// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Debugger.setPauseOnExceptions does not pause on syntax errors when state is 'uncaught'.`);

  await dp.Debugger.enable();
  await dp.Runtime.enable();
  await dp.Debugger.setPauseOnExceptions({state: 'uncaught'});

  dp.Debugger.onPaused(() => {
    testRunner.log('FAIL: debugger paused on syntax error');
  });

  const exceptionPromise = dp.Runtime.onceExceptionThrown();
  const consolePromise = dp.Runtime.onceConsoleAPICalled();

  await session.evaluateAsync(url => new Promise(resolve => {
                                const iframe = document.createElement('iframe');
                                iframe.onload = resolve;
                                iframe.src = url;
                                document.body.appendChild(iframe);
                              }),
                              testRunner.url('resources/syntax-error.html'));

  const {exceptionDetails} = (await exceptionPromise).params;
  const consoleEvent = (await consolePromise).params;

  const shortUrl = url => (url ? url.split('/').pop() : '');
  testRunner.log(`${shortUrl(exceptionDetails.url)}:${
      exceptionDetails.lineNumber + 1} ${exceptionDetails.text} ${
      exceptionDetails.exception.description} (at ${
      shortUrl(exceptionDetails.url)}:${exceptionDetails.lineNumber + 1}:${
      exceptionDetails.columnNumber + 1})`);
  const topFrame = consoleEvent.stackTrace.callFrames[0];
  testRunner.log(`${shortUrl(topFrame.url)}:${topFrame.lineNumber + 1} ${
      consoleEvent.args.map(a => a.value).join(' ')}`);

  testRunner.completeTest();
});
