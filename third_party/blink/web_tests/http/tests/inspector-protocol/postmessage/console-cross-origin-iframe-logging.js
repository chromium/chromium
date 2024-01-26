// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'http://example.test:8000/inspector-protocol/resources/empty.html',
      `Tests that postMessage origin mismatches are logged with source url and line number if both frames are in the same process.`);

  const onEntryAddedHandler = event => {
    testRunner.log(event.params.entry.stackTrace);
    testRunner.log(event.params.entry.text);
    testRunner.completeTest();
  };
  dp.Log.onEntryAdded(onEntryAddedHandler);
  await dp.Log.enable();

  await session.evaluateAsync(`
    const frame = document.createElement('iframe');
    frame.src =
    'http://other.origin.example.test:8000/inspector-protocol/resources/empty.html';
    document.body.appendChild(frame);
    window.myframe = frame;
    new Promise(f => frame.onload = f);
  `);

  testRunner.log(`Trying to access iframe's location`);
  const {result: result1} = await dp.Runtime.evaluate(
      {expression: 'window.myframe.contentWindow.location.host'});
  testRunner.log(result1.exceptionDetails.exception.className);

  testRunner.log(`Trying to reload iframe`);
  const {result: result2} = await dp.Runtime.evaluate(
      {expression: 'window.myframe.location.reload()'});
  testRunner.log(result2.exceptionDetails.exception.className);

  testRunner.log(`Trying to postMessage iframe`);
  await session.evaluate(`
    window.myframe.contentWindow.postMessage("fail",
    "http://example.test:8000");
  `);
})
