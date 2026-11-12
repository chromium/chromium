// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Runtime.getProperties returns built-in, DOM, and user-defined properties on the global window object.');

  await session.evaluate(`
    function doit() {
      console.dir(window);
    }
  `);

  const {result: {result: windowObject}} = await dp.Runtime.evaluate({
    expression: 'window',
    objectGroup: 'console',
    includeCommandLineAPI: false,
  });
  const {result: {result: properties}} = await dp.Runtime.getProperties({
    objectId: windowObject.objectId,
    ownProperties: true,
    generatePreview: false,
  });

  const golden = {
    window: 1,
    document: 1,
    eval: 1,
    console: 1,
    frames: 1,
    Array: 1,
    doit: 1,
  };

  const found = {};
  for (const property of properties) {
    if (golden[property.name]) {
      found[property.name] = 1;
    }
  }

  testRunner.log(found);
  testRunner.completeTest();
});
