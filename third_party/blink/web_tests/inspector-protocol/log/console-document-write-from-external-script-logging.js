// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that ignored document.write() called from an external asynchronously loaded script is reported via Log.entryAdded as a warning with an async stack trace.`);

  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  await dp.Log.enable();

  await session.evaluate(`
      function loadExternalScript(url)
      {
          var scriptElement = document.createElement("script");
          scriptElement.src = url;
          document.body.appendChild(scriptElement);
      }
      //# sourceURL=console-document-write-from-external-script-logging.js`);

  const entryPromise = dp.Log.onceEntryAdded();
  session.evaluate(`loadExternalScript('${
      testRunner.url(
          'resources/external-script-with-document-write.js')}')\n//# sourceURL=console-document-write-from-external-script-logging.js`);
  const {entry} = (await entryPromise).params;

  const shortUrl = url => (url ? url.split('/').pop() : '');
  testRunner.log(
      `${shortUrl(entry.url)}:${entry.lineNumber + 1} ${entry.text}`);
  for (let stack = entry.stackTrace; stack; stack = stack.parent) {
    if (stack.description) {
      testRunner.log(stack.description);
    }
    for (const frame of stack.callFrames) {
      testRunner.log(`${frame.functionName || '(anonymous)'} @ ${
          shortUrl(frame.url)}:${frame.lineNumber + 1}`);
    }
  }

  testRunner.completeTest();
});
