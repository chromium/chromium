// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank('Tests that tracing does not record unrelated processes.');

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  // Create another page, then disconnect to make sure it's not the part
  // of the trace via session tracking.
  const {session: otherSession} = await testRunner.startURL('data:text/html;charset=utf-8;base64,PGh0bWw+PC9odG1sPg==', 'Another page');
  await otherSession.disconnect();

  await tracingHelper.startTracing();
  await session.evaluateAsync(`new Promise(resolve => requestAnimationFrame(resolve))`);
  // Make sure GPU process pops up in trace as well.
  await dp.Page.captureScreenshot({format: 'png'});
  const events = await tracingHelper.stopTracing();

  const pids = new Set();
  for (const event of events)
    pids.add(event.pid);
  testRunner.log(`There should be just 3 processes (browser, GPU, and renderer): ${pids.size}`);

  testRunner.completeTest();
})
