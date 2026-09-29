// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <div id="testDiv" style="color: green">testDiv</div>
    `,
      'Tests that inspector doesn\'t force styles recalc on operations with inline element styles that result in no changes.');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper({log: () => {}}, session);

  await dp.DOM.enable();
  await dp.CSS.enable();

  await tracingHelper.invokeAsyncWithTracing(function performActions() {
    var testDiv = document.querySelector('#testDiv');
    for (var i = 0; i < 20; ++i)
      testDiv.style.visibility = '';
  });

  const updateLayoutTreeEvents =
      tracingHelper.findEvents('UpdateLayoutTree', 'X');
  testRunner.log(`Found ${
      updateLayoutTreeEvents.length} UpdateLayoutTree events (expecting 0).`);
  testRunner.completeTest();
});
