// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that Runtime.evaluate reports exceptionDetails without crashing when throwing undefined, number, string, object, or Error. Bug 59611.');

  // Runtime.enable enables uncaught-exception stack capturing on v8::Message,
  // and Debugger.enable attaches that MessageObject to rejected replMode
  // promises so primitive throws also carry stackTrace (matching Console).
  await dp.Runtime.enable();
  await dp.Debugger.enable();

  async function evaluateAndLogException(expression) {
    testRunner.log(expression);
    const {result: {exceptionDetails}} = await dp.Runtime.evaluate({
      expression,
      objectGroup: 'console',
      includeCommandLineAPI: true,
      generatePreview: true,
      replMode: true,
    });
    const {type, value, description, preview} = exceptionDetails.exception;
    testRunner.log({
      text: exceptionDetails.text,
      hasStackTrace: Boolean(exceptionDetails.stackTrace?.callFrames.length),
      exception: {
        type,
        ...(value !== undefined && {value}),
        ...(description !== undefined && {description}),
        ...(preview !== undefined && {preview}),
      },
    });
  }

  testRunner.runTestSuite([
    async function testThrowUndefined() {
      await evaluateAndLogException('throw undefined');
    },
    async function testThrowNumber() {
      await evaluateAndLogException('throw 1');
    },
    async function testThrowString() {
      await evaluateAndLogException('throw \'asdf\'');
    },
    async function testThrowObject() {
      await evaluateAndLogException('throw {a:42}');
    },
    async function testThrowError() {
      await evaluateAndLogException('throw new Error(\'asdf\')');
    },
  ]);
});
