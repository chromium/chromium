// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that injected script isn\'t reported to frontend.\n');

  await dp.Page.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const {result: {frameTree}} = await dp.Page.getResourceTree();

  const parsedUrls = [];
  let isolatedContextId;
  dp.Debugger.onScriptParsed(({params}) => {
    parsedUrls.push(params.url);
    if (params.url === 'foo.js' &&
        params.executionContextAuxData?.isDefault !== true) {
      testRunner.fail('Expected foo.js to parse in default execution context.');
    }
    if (params.url === '239.js' &&
        (params.executionContextId !== isolatedContextId ||
         params.executionContextAuxData?.isDefault !== false)) {
      testRunner.fail(
          'Expected 239.js to parse in isolated execution context.');
    }
    testRunner.log(params.url);
  });

  ({result: {executionContextId: isolatedContextId}} =
       await dp.Page.createIsolatedWorld({
         frameId: frameTree.frame.id,
         worldName: '239',
       }));

  await dp.Runtime.evaluate({
    expression: 'void 0;\n//# sourceURL=foo.js',
    objectGroup: 'console',
  });
  await dp.Runtime.evaluate({
    contextId: isolatedContextId,
    expression: 'console.log(42);\n//# sourceURL=239.js',
    objectGroup: 'console',
  });

  if (parsedUrls.length !== 2) {
    testRunner.fail(`Expected 2 scriptParsed events, got ${
        parsedUrls.length}: ${parsedUrls.join(', ')}`);
  }
  testRunner.completeTest();
});
