// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that uncaught exceptions in an iframe are logged via Runtime.exceptionThrown (Bug 47250).');

  await dp.Runtime.enable();
  const {result: {debuggerId}} = await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  const debuggers = new Map([[debuggerId, dp.Debugger]]);

  const exceptions = [];
  let resolveAllExceptions;
  const allExceptionsPromise = new Promise(resolve => {
    resolveAllExceptions = resolve;
  });

  dp.Runtime.onExceptionThrown(event => {
    exceptions.push(event.params.exceptionDetails);
    if (exceptions.length === 3) {
      resolveAllExceptions();
    }
  });

  await session.evaluate(`
    const iframe = document.createElement('iframe');
    iframe.src = ${
      JSON.stringify(testRunner.url('resources/uncaught-in-iframe.html'))};
    document.body.appendChild(iframe);
  `);

  await allExceptionsPromise;

  for (const details of exceptions) {
    testRunner.log(`${details.text} (${details.url}:${details.lineNumber}:${
        details.columnNumber})`);
    testRunner.log(details.exception.description);
    await testRunner.logStackTrace(debuggers, details.stackTrace, debuggerId);
  }

  testRunner.completeTest();
})
