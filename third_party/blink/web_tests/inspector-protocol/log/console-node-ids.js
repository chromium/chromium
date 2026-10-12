// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
        div { width: 100px; height: 100px; view-transition-name: duplicate; }
      </style>
      <div id="first"></div><div id="second"></div>`,
      'Console messages preserve related DOM nodes.');
  await dp.Log.enable();
  const entryPromise = dp.Log.onceEntryAdded(
      event => event.params.entry.text ===
          'Unexpected duplicate view-transition-name: duplicate');
  await session.evaluateAsync(`
      document.startViewTransition().ready.catch(() => {})`);
  const {params: {entry}} = await entryPromise;
  testRunner.log('Node count: ' + entry.args.length);
  for (const argument of entry.args) {
    const response = await dp.Runtime.callFunctionOn({
      objectId: argument.objectId,
      functionDeclaration: 'function() { return this.id; }',
      returnByValue: true,
    });
    testRunner.log(response.result.result.value);
  }
  testRunner.completeTest();
})
