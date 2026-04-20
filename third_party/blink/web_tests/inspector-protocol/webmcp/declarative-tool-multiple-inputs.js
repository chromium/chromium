(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that declarative WebMCP tool with multiple inputs generates the correct schema.');

  testRunner.log('Enabling WebMCP Domain...');
  await dp.WebMCP.enable();

  testRunner.log('Navigating to a new page...');
  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({enabled: true});

  let addedCount = 0;
  let removedCount = 0;
  const { promise: allEventsReceived, resolve } = Promise.withResolvers();

  function checkDone() {
    if (addedCount === 2 && removedCount === 1) {
      resolve();
    }
  }

  dp.WebMCP.onToolsAdded(event => {
    testRunner.log('Tools Added:');
    for (const tool of event.params.tools) {
      testRunner.log(`  Name: ${tool.name}`);
      testRunner.log(`  Description: ${tool.description}`);
      testRunner.log(`  Schema: ${JSON.stringify(tool.inputSchema, null, 2)}`);
    }
    addedCount++;
    checkDone();
  });

  dp.WebMCP.onToolsRemoved(event => {
    testRunner.log('Tools Removed:');
    for (const tool of event.params.tools) {
      testRunner.log(`  Name: ${tool.name}`);
    }
    removedCount++;
    checkDone();
  });

  dp.Page.navigate({url: testRunner.url('../resources/webmcp-declarative-multiple-inputs.html')});
  await dp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  await allEventsReceived;

  testRunner.completeTest();
});
