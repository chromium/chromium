// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startHTML(`
      <!DOCTYPE html>
      <style>

      .float {
          width: 8em;
          height: 8em;
          float: left;
          shape-margin: 2em;
          margin: 1em;
      }

      .circle {
          background-color:blue;
          shape-outside: circle(closest-side at center);
          -webkit-clip-path: circle(closest-side at center);
      }

      </style>
      <div class="float circle" id="circle"> </div>
    `,
                                 `Highlight CSS shapes outside scroll.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpHighlight('circle');
  // Scroll the inspected page and dump again (matching the two dumps in the
  // original layout test: initial dump at i=0 and post-scroll dump at i=1).
  await session.evaluate('window.scrollTo(0, 100)');
  await helper.dumpHighlight('circle');
  testRunner.completeTest();
});
