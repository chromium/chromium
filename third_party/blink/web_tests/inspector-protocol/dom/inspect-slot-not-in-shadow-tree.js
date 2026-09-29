// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
      <p id="description"></p>

      <slot id="test1"><span>test</span></slot>
    `,
      'Tests that slots that are not in a shadow tree can be inspected.');

  await dp.DOM.enable();
  await dp.Runtime.enable();
  const inspectRequested = dp.Runtime.onceInspectRequested();
  await dp.Runtime.evaluate(
      {expression: 'inspect(test1)', includeCommandLineAPI: true});
  const {params: {object}} = await inspectRequested;
  const {result: {node}} =
      await dp.DOM.describeNode({objectId: object.objectId});
  if (node.localName === 'slot') {
    testRunner.log('Inspect successful');
  }
  testRunner.completeTest();
})
