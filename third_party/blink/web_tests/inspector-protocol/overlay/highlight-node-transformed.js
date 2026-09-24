// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>

      body {
          margin: 0;
      }

      iframe {
          position: absolute;
          left: 83px;
          top: 53px;
          width: 200px;
          height: 200px;
      }

      </style>
      <iframe id="transform-iframe" src="${
          testRunner.url(
              './resources/highlight-node-transformed-iframe.html')}"></iframe>
    `,
      `\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpHighlight('div');
  testRunner.completeTest();
});
