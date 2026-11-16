// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
      <div id="host"></div>
    `,
      'Tests that appending a slot into a shadow root does not crash with DevTools open.');

  await dp.DOM.enable();
  await dp.DOM.getDocument({depth: -1, pierce: true});

  await session.evaluate(() => {
    const shadowRoot =
        document.getElementById('host').attachShadow({mode: 'open'});
    shadowRoot.innerHTML = '<div><input type="text"></div>';
  });

  await dp.DOM.getDocument({depth: -1, pierce: true});

  await session.evaluate(() => {
    const shadowRoot = document.getElementById('host').shadowRoot;
    shadowRoot.appendChild(document.createElement('slot'));
  });

  testRunner.log('PASS');
  testRunner.completeTest();
})
