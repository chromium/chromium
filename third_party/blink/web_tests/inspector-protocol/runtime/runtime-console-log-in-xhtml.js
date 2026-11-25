// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Runtime.consoleAPICalled from an inline script in an XHTML document reports accurate line and column numbers.');

  await dp.Runtime.enable();

  const messagePromise = dp.Runtime.onceConsoleAPICalled();
  await session.navigate('resources/console-log-in-xhtml.xhtml');
  const {params} = await messagePromise;

  const frame = params.stackTrace.callFrames[0];
  testRunner.log(
      `${params.args[0].value}:${frame.lineNumber}:${frame.columnNumber}`);
  testRunner.completeTest();
});
