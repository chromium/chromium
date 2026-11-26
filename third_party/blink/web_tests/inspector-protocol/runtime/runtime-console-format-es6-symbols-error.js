// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Runtime.consoleAPICalled formats ES6 Symbol descriptions without invoking an overridden Symbol.prototype.toString.');

  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(({params}) => {
    const arg = params.args[0];
    const frame = params.stackTrace.callFrames[0];
    testRunner.log(
        `${frame.url}:${frame.lineNumber + 1} ${arg.type} ${arg.description}`);
  });
  dp.Runtime.onExceptionThrown(({params}) => {
    testRunner.log(`Unexpected exception: ${params.exceptionDetails.text}`);
  });

  await session.evaluate(`
    function logSymbolToConsoleWithError() {
      Symbol.prototype.toString = function(arg) { throw new Error(); };
      var smb = Symbol();
      console.log(smb);
    }
    //# sourceURL=runtime-console-format-es6-symbols-error.js
  `);

  await session.evaluate('logSymbolToConsoleWithError()');
  testRunner.completeTest();
});
