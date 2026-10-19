// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Debugger.setPauseOnExceptions does not crash when pausing on uncaught exceptions thrown from C++ bindings.`);

  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  await dp.Runtime.enable();

  await session.evaluate(`
      var functions;

      function testFunction()
      {
          console.clear();
          // This used to be a racy crash. Test some sequence of functions.
          functions = [f2, f1, f2, f1, f2, f1, f2, f1];
          functions.push(function() {});
          functions.shift()();
      }

      function f1() {
          setTimeout(functions.shift(), 0);
          document.body.appendChild("<throw_exception>");
      }

      function f2() {
          setTimeout(functions.shift(), 0);
          new Range().compareBoundaryPoints(1, 2);
      }
//# sourceURL=rethrow-error-from-bindings-crash.js`);

  const expectedErrorsCount = 8;
  const exceptions = [];
  let resolveAllExceptions;
  const allExceptionsPromise =
      new Promise(resolve => resolveAllExceptions = resolve);

  dp.Runtime.onExceptionThrown(event => {
    exceptions.push(event.params.exceptionDetails);
    if (exceptions.length === expectedErrorsCount) {
      resolveAllExceptions();
    }
  });

  await dp.Debugger.setPauseOnExceptions({state: 'uncaught'});

  let pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate(
      `setTimeout(testFunction, 0)\n//# sourceURL=schedule.js`);
  testRunner.log('Set timer for test function.');

  for (let i = 0; i < expectedErrorsCount; ++i) {
    await pausedPromise;
    if (i + 1 < expectedErrorsCount) {
      pausedPromise = dp.Debugger.oncePaused();
    }
    await dp.Debugger.resume();
  }

  await allExceptionsPromise;
  const shortUrl = url => (url ? url.split('/').pop() : '');
  for (const details of exceptions) {
    testRunner.log(`${shortUrl(details.url)}:${details.lineNumber + 1} ${
        details.text} ${details.exception.description.split('\n')[0]}`);
    for (let stack = details.stackTrace; stack; stack = stack.parent) {
      if (stack.description) {
        testRunner.log(stack.description);
      }
      for (const frame of stack.callFrames) {
        testRunner.log(`${frame.functionName || '(anonymous)'} @ ${
            shortUrl(frame.url)}:${frame.lineNumber + 1}`);
      }
    }
  }

  await dp.Debugger.setPauseOnExceptions({state: 'none'});
  testRunner.completeTest();
});
