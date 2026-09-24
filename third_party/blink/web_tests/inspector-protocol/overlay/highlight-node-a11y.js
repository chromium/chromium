// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
        <!DOCTYPE html>
        <button id="button">click</button>
        <input id="input"></input>
        <div id="div" tabindex="0" role="button" aria-label="click">Save</div>
      `,
      `This test verifies a11y attributes for a node.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function dumpHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumpHighlight('button');
  await dumpHighlight('input');
  await dumpHighlight('div');

  testRunner.completeTest();
});
