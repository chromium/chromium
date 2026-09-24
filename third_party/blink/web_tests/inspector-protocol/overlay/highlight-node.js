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
      #container {
          width: 400px;
          height: 400px;
          background-color: grey;
      }
      #inspectedElement {
          margin: 5px;
          border: solid 10px aqua;
          padding: 15px;
          width: 200px;
          height: 200px;
          background-color: blue;
          float: left;
      }
      #description {
          clear: both;
          height: 10px;
          font-size: 20px;
          font-family: Arial;
      }

      </style>
      <div id="inspectedElement"></div>
      <p id="description">foo<br />bar</p>
    `,
      `This test verifies the position and size of the highlight rectangles overlaid on an inspected node.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  const div = await helper.nodeWithId('inspectedElement');
  await nodeResolved(div, 'inspectedElement');
  await nodeResolved(div, 'inspectedElement with RGB format', 'rgb');
  await nodeResolved(div, 'inspectedElement with HSL format', 'hsl');
  await nodeResolved(div, 'inspectedElement with HWB format', 'hwb');

  let textNode = await helper.findNode(node => {
    return node.nodeType() === Node.TEXT_NODE && node.parentNode &&
        node.parentNode.nodeName() === 'P' &&
        node.parentNode.children()[0] === node;
  });
  await nodeResolvedApproximate(textNode, '\nFirst text node', 28, 25);

  textNode = await helper.findNode(node => {
    return node.nodeType() === Node.TEXT_NODE && node.parentNode &&
        node.parentNode.nodeName() === 'P' &&
        node.parentNode.children()[2] === node;
  });
  await nodeResolvedApproximate(textNode, '\nSecond text node', 30, 24);

  testRunner.completeTest();

  /**
   * @param {!Node} node
   * @param {string} name
   * @param {string=} colorFormat
   * @param {!Promise}
   */
  async function nodeResolved(node, name, colorFormat = 'hex') {
    const {result: {highlight: result}} =
        await dp.Overlay.getHighlightObjectForTest(
            {nodeId: node.id, colorFormat});
    testRunner.log(name + JSON.stringify(result, null, 2));
  }

  /**
   * @param {!Node} node
   * @param {string} name
   * @param {number} expectedWidth
   * @param {number} expectedHeight
   * @param {number=} tolerance
   * @param {!Promise}
   */
  async function nodeResolvedApproximate(node, name, expectedWidth,
                                         expectedHeight, tolerance = 3) {
    const {highlight: result} =
        (await dp.Overlay.getHighlightObjectForTest({nodeId: node.id})).result;

    if (result['paths']) {
      for (const path of result['paths']) {
        path['path'] = path['path'].map(value => {
          return typeof value === 'number' ? '<number>' : value;
        });
      }
    }

    if (result['elementInfo']) {
      const actualWidth = result['elementInfo']['nodeWidth'];
      const actualHeight = result['elementInfo']['nodeHeight'];
      const widthInTolerance =
          Math.abs(actualWidth - expectedWidth) < tolerance;
      const heightInTolerance =
          Math.abs(actualHeight - expectedHeight) < tolerance;
      result['elementInfo']['nodeWidth'] = `Width within ${tolerance}px from ${
          expectedWidth}? ${widthInTolerance}`;
      result['elementInfo']['nodeHeight'] = `Height within ${
          tolerance}px from ${expectedHeight}? ${heightInTolerance}`;
    }

    testRunner.log(name + JSON.stringify(result, null, 2));
  }
});
