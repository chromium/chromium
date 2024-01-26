(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that multiple sessions cannot trace simultaneously.');
  var page = await testRunner.createPage();
  var session1 = await page.createSession();
  var session2 = await page.createSession();

  var session1DataCollected = false;
  session1.protocol.Tracing.onDataCollected(event => session1DataCollected = true);
  var session2DataCollected = false;
  session2.protocol.Tracing.onDataCollected(event => session2DataCollected = true);

  var session2TracingComplete = false;
  session2.protocol.Tracing.onTracingComplete(event => session2TracingComplete = true);

  testRunner.log('Starting tracing in session1');
  testRunner.log(await session1.protocol.Tracing.start());

  testRunner.log('Starting tracing in session2');
  testRunner.log(await session2.protocol.Tracing.start());

  var promise = session1.protocol.Tracing.onceTracingComplete();

  testRunner.log('Stopping tracing in session1');
  testRunner.log(await session1.protocol.Tracing.end());

  testRunner.log('Stopping tracing in session2');
  testRunner.log(await session2.protocol.Tracing.end());

  await promise;
  testRunner.log(`session1: dataCollected=${session1DataCollected} tracingComplete=${true}`);
  testRunner.log(`session2: dataCollected=${session2DataCollected} tracingComplete=${session2TracingComplete}`);

  testRunner.completeTest();
})
