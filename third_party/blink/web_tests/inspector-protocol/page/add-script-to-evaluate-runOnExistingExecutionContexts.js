(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(`Tests that Page.addScriptToEvaluateOnNewDocument can run for existing contexts.`);

  const bt = testRunner.browserP().Target;
  await bt.setDiscoverTargets({discover: true});
  await bt.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  bt.createTarget({
    url: 'about:blank',
    newWindow: false
  });

  const attachedEvent = await bt.onceAttachedToTarget();
  const frameId = attachedEvent.params.targetInfo.targetId;
  const worldName = 'testWorld';

  const {protocol: dp} = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
  dp.Runtime.enable();
  dp.Page.enable();
  const worldPromise = dp.Page.createIsolatedWorld({frameId, worldName})
  dp.Page.addScriptToEvaluateOnNewDocument({ source: 'window.foo = 41;', runImmediately: true });
  dp.Page.addScriptToEvaluateOnNewDocument({ source: 'window.bar = 42;' });
  dp.Page.addScriptToEvaluateOnNewDocument({ source: 'window.foo = 43;', worldName, runImmediately: true });
  dp.Page.addScriptToEvaluateOnNewDocument({ source: 'window.bar = 44;', worldName });
  await dp.Runtime.runIfWaitingForDebugger();

  const world = await worldPromise;

  async function logWindowVariable(varName, contextId) {
    testRunner.log(`${varName} in the ${contextId ? 'isolated' : 'main'} world:`);
    testRunner.log(await dp.Runtime.evaluate({
      contextId: contextId,
      expression: `window.${varName}`,
    }));
  }
  await logWindowVariable('foo');
  await logWindowVariable('bar');
  await logWindowVariable('foo', world.result.executionContextId);
  await logWindowVariable('bar', world.result.executionContextId);
  testRunner.completeTest();
})
