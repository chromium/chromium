// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that evaluate in console works even if window.console is substituted or deleted. Bug 53072');

  await dp.Runtime.enable();
  dp.Runtime.onExceptionThrown(event => {
    testRunner.log(`FAIL: unexpected exceptionThrown: ${
        event.params.exceptionDetails.text}`);
  });

  await session.evaluate(`
    function deleteConsole() {
      window.console = undefined;
    }

    function substituteConsole() {
      Object.defineProperty(window, "__commandLineAPI", {
        enumerable: false,
        configurable: false,
        get: function() {
          throw "Substituted";
        },
      });
    }
  `);

  async function evaluateInConsole(expression) {
    const response = await dp.Runtime.evaluate({
      expression,
      objectGroup: 'console',
      includeCommandLineAPI: true,
      generatePreview: true,
      replMode: true,
    });
    if (response.error) {
      testRunner.log(`FAIL: Runtime.evaluate error for "${expression}": ${
          response.error.message}`);
    }
    if (response.result.exceptionDetails) {
      testRunner.log(`FAIL: exceptionDetails for "${expression}": ${
          response.result.exceptionDetails.text}`);
    }
    return response.result.result;
  }

  await evaluateInConsole('deleteConsole()');
  if (await session.evaluate('typeof window.console') !== 'undefined') {
    testRunner.log('FAIL: window.console was not deleted');
  }

  const res1 = await evaluateInConsole('1');
  testRunner.log(res1.value);

  await evaluateInConsole('substituteConsole()');
  const trapResult = await session.evaluate(
      '(() => { try { return window.__commandLineAPI; } catch (e) { return e; } })()');
  if (trapResult !== 'Substituted') {
    testRunner.log(
        `FAIL: unexpected __commandLineAPI getter behavior: ${trapResult}`);
  }

  const res2 = await evaluateInConsole('2');
  testRunner.log(res2.value);

  testRunner.completeTest();
});
