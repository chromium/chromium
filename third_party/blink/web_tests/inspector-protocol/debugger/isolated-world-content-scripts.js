// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} =
      await testRunner.startBlank('Tests that content scripts are reported.');

  const executionContexts = new Map();
  dp.Runtime.onExecutionContextCreated(({params: {context}}) => {
    executionContexts.set(context.id, context);
  });

  const parsedScripts = [];
  dp.Debugger.onScriptParsed(({params}) => {
    parsedScripts.push(params);
  });

  dp.Runtime.onConsoleAPICalled(({params}) => {
    const args = params.args.map(arg => arg.value ?? arg.description).join(' ');
    testRunner.log(`Unexpected console.${params.type}: ${args}`);
  });
  dp.Runtime.onExceptionThrown(({params: {exceptionDetails}}) => {
    const details = exceptionDetails.exception?.description || '';
    testRunner.log(`Unexpected exception: ${exceptionDetails.text} ${details}`);
  });

  await dp.Page.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const {result: {frameTree}} = await dp.Page.getResourceTree();
  const mainFrameId = frameTree.frame.id;

  // Evaluate a control script in the default (main) world so we also verify
  // main-world scripts report auxData.isDefault === true and are excluded from
  // content scripts.
  await dp.Runtime.evaluate({expression: '1\n//# sourceURL=main.js'});

  const {result: {executionContextId: world1ContextId}} =
      await dp.Page.createIsolatedWorld(
          {frameId: mainFrameId, worldName: 'world-239'});
  const {result: {executionContextId: world2ContextId}} =
      await dp.Page.createIsolatedWorld(
          {frameId: mainFrameId, worldName: 'world-42'});

  const eval1 = await dp.Runtime.evaluate({
    contextId: world1ContextId,
    expression: '42\n//# sourceURL=239.js',
  });
  const eval2 = await dp.Runtime.evaluate({
    contextId: world2ContextId,
    expression: '239\n//# sourceURL=42.js',
  });
  if (eval1.result?.result?.value !== 42 ||
      eval2.result?.result?.value !== 239) {
    testRunner.log(
        `FAIL: unexpected eval values: ${JSON.stringify([eval1, eval2])}`);
  }

  const mainScript = parsedScripts.find(script => script.url === 'main.js');
  if (!mainScript || mainScript.executionContextAuxData?.isDefault !== true ||
      mainScript.executionContextAuxData?.type !== 'default') {
    testRunner.log('FAIL: main-world script missing or marked non-default');
  } else if (mainScript.executionContextId === world1ContextId ||
             mainScript.executionContextId === world2ContextId) {
    testRunner.log('FAIL: main-world script used isolated world contextId');
  }

  for (const [label, contextId] of [['world1', world1ContextId],
                                    ['world2', world2ContextId],
  ]) {
    const ctx = executionContexts.get(contextId);
    if (!ctx || ctx.auxData?.isDefault !== false ||
        ctx.auxData?.type !== 'isolated' ||
        ctx.auxData?.frameId !== mainFrameId) {
      testRunner.log(
          `FAIL: ${
              label} Runtime.executionContextCreated unexpected auxData: ` +
          `${JSON.stringify(ctx?.auxData)}`);
    }
  }

  const contentScripts =
      parsedScripts.filter(script => script.executionContextAuxData &&
                               'isDefault' in script.executionContextAuxData &&
                               !script.executionContextAuxData.isDefault);

  if (contentScripts.length !== 2 ||
      contentScripts[0].executionContextId !== world1ContextId ||
      contentScripts[1].executionContextId !== world2ContextId) {
    testRunner.log('FAIL: isolated world script executionContextId mismatch');
  }

  testRunner.log('');
  testRunner.log('Content Scripts:');
  for (const [i, script] of contentScripts.entries()) {
    testRunner.log(`#${i + 1}:`);
    testRunner.log(`  sourceURL:${script.url}`);
    testRunner.log(`  endColumn:${script.endColumn}`);
    testRunner.log(`  isDefault:${script.executionContextAuxData.isDefault}` +
        ` type:${script.executionContextAuxData.type}`);
  }
  testRunner.completeTest();
});
