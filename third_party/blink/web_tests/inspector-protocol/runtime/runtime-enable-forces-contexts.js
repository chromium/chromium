(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that Runtime.enable forces execution context creation.`);

  var count = 0;
  dp.Runtime.onExecutionContextCreated(event => ++count);

  await dp.Page.enable();
  dp.Page.navigate({url: 'data:text/html,<body>page<iframe></iframe></body>'});
  await dp.Page.onceLoadEventFired();
  testRunner.log('Navigated to page without script');

  testRunner.log('Sending Runtime.enable');
  await session.protocol.Runtime.enable();
  testRunner.log(`Got execution contexts: ${count}`);

  count = 0;
  dp.Page.navigate({url: 'data:text/html,<body>page<iframe src="data:text/html,text"></iframe></body>'});
  await dp.Page.onceLoadEventFired();
  testRunner.log('Navigated to page without script (with Runtime enabled)');
  testRunner.log(`Got execution contexts: ${count}`);

  testRunner.completeTest();
})
