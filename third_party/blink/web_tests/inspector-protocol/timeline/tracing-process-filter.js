// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {session} = await testRunner.startBlank('Tests that tracing does not record unrelated processes.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing();
  await session.evaluateAsync(`return Promise.resolve(42)`);
  await testRunner.startURL('data:text/html;charset=utf-8;base64,PGh0bWw+PC9odG1sPg==', 'Another page');
  const events = await tracingHelper.stopTracing();

  const pids = new Set();
  for (const event of events)
    pids.add(event.pid);
  testRunner.log(`There should be just 3 processes (browser, GPU, and renderer): ${pids.size}`);

  testRunner.completeTest();
})
