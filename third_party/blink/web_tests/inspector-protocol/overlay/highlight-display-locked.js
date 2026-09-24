// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startHTML(`
      <div id="container" style="content-visibility: hidden; contain-intrinsic-size: 10px;">
        <div id="child" style="width: 50px; height: 50px; background: blue">Text</div>
      </div>
    `,
                                 `Tests highlights for display locking.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpHighlight('container');
  await helper.dumpHighlight('child');
  testRunner.completeTest();
});
