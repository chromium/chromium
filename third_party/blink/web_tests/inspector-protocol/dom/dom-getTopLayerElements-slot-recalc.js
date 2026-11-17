// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `<div id="host"></div>`,
      'Tests that DOM.getTopLayerElements handles the top layer changing ' +
          'while it pushes nodes to the frontend.');

  await dp.DOM.enable();
  // Only push the document down to <body>, so that the shadow tree is
  // serialized by DOM.getTopLayerElements below.
  await dp.DOM.getDocument();

  // Both dialogs are modal, so each has a ::backdrop in the top layer. Changing
  // the slot attribute leaves #outer unslotted. The two commands are sent back
  // to back, so that the slot assignment is still dirty when
  // DOM.getTopLayerElements runs.
  dp.Runtime.evaluate({
    expression: `(() => {
      const host = document.getElementById('host');
      const shadowRoot = host.attachShadow({mode: 'open'});
      shadowRoot.innerHTML =
          '<dialog id="inner"></dialog><slot name="assigned"></slot>';
      const outer = document.createElement('dialog');
      outer.id = 'outer';
      outer.slot = 'assigned';
      host.appendChild(outer);
      outer.showModal();
      shadowRoot.getElementById('inner').showModal();
      // Force a style update so that the ::backdrop pseudo-elements exist.
      document.body.offsetTop;
      outer.slot = 'unassigned';
    })()`
  });
  // Serializing the <slot> recalculates the slot assignment, which removes the
  // ::backdrop of #outer from the top layer.
  const response = await dp.DOM.getTopLayerElements();

  testRunner.log('Top layer elements:');
  for (const nodeId of response.result.nodeIds) {
    const described = await dp.DOM.describeNode({nodeId});
    if (described.error) {
      testRunner.log(`FAIL: ${described.error.message}`);
      continue;
    }
    const node = described.result.node;
    const idIndex = node.attributes.indexOf('id');
    const id = idIndex >= 0 ? `#${node.attributes[idIndex + 1]}` : '';
    testRunner.log(`${node.nodeName}${id}`);
  }

  testRunner.completeTest();
})
