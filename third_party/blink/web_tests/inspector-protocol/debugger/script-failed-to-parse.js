// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Debugger.scriptFailedToParse event is raised after compiling a script with syntax error.');

  await session.evaluate(`
    function addScript(url) {
      const head = document.getElementsByTagName('head')[0];
      const script = document.createElement('script');
      script.setAttribute('src', url);
      head.appendChild(script);
    }
  `);

  await dp.Debugger.enable();

  dp.Debugger.onScriptParsed(event => {
    if (event.params.url.includes('script-failed-to-parse.js')) {
      testRunner.log(
          'FAIL: Debugger.scriptParsed emitted for script with syntax error');
    }
  });

  const failedToParsePromise = dp.Debugger.onceScriptFailedToParse(
      event =>
          event.params.url.endsWith('resources/script-failed-to-parse.js'));
  await session.evaluate(url => addScript(url),
                         testRunner.url('resources/script-failed-to-parse.js'));

  await failedToParsePromise;
  testRunner.log('Event with script-failed-to-parse.js received');

  testRunner.completeTest();
});
