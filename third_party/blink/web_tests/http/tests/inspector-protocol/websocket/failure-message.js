(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Verifies that a helpful console message is emitted on websocket failure.`);

  await dp.Log.enable();
  dp.Log.onEntryAdded(entryAddedEvent => {
    const message = entryAddedEvent.params.entry.text;
    if (message.includes('WebSocket') && message.includes('failed')) {
      testRunner.log('WebSocket failure console message:\n' + message);
      testRunner.completeTest();
    }
    testRunner.log('logEntry: ' + JSON.stringify(logEntry, null, 2));
  });

  session.evaluate(`new WebSocket('ws://localhost:8000/')`);
})
