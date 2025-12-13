(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Verify that Target.exposeDevToolsProtocol scoped to the default execution context.');
  await dp.Target.exposeDevToolsProtocol(
      {targetId: page._targetId, bindingName: 'cdp'});

  await dp.Runtime.enable();
  dp.Page.createIsolatedWorld({frameId: page._targetId, worldName: 'foo'});
  const contextFoo =
      (await dp.Runtime.onceExecutionContextCreated()).params.context.id;

  const isCdpExposedToFoo = await dp.Runtime.evaluate(
      {expression: 'window.cdp!==undefined', contextId: contextFoo});
  testRunner.log(`Protocol exposed to foo. Expected: false. Actual: ${
      isCdpExposedToFoo.result.result.value}.`);

  const isCdpExposedToDefault =
      await dp.Runtime.evaluate({expression: 'window.cdp!==undefined'});
  testRunner.log(
      `Protocol exposed to default execution context. Expected: true. Actual: ${
          isCdpExposedToDefault.result.result.value}.`);

  testRunner.completeTest();
})
