// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <script src="${testRunner.url('../../resources/ahem.js')}"></script>
    <style>
    body {
      margin: 0;
      font: 10px/10px Ahem;
    }
    #container {
      margin: 50px 60px 70px 80px;
      width: 300px;
      height: 300px;
      writing-mode: vertical-rl;
    }
    #child {
      padding: 50px 60px 70px 80px;
      width: 100px;
      height: 100px;
    }
    </style>
    <div id="container">
      <div id="child">
        <span id="span">ABCDEFG</span>
      </div>
    </div>
  `,
                                                   `\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await session.evaluateAsync('');
  function dumpHighlight(id) {
    return helper.dumpHighlight(id);
  }
  await dumpHighlight('container');
  await dumpHighlight('child');
  await dumpHighlight('span');

  let textNode = await helper.findNode(node => node.nodeValue() == 'ABCDEFG');
  let {highlight: result} = (await dp.Overlay.getHighlightObjectForTest({
                              nodeId: textNode.id
                            })).result;
  testRunner.log('TEXT' + JSON.stringify(result, null, 2));
  testRunner.completeTest();
});
