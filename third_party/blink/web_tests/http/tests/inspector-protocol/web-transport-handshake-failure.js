(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Check the console message printed on a WebTransport handshake failure.`);
  const url = 'https://localhost';

  await dp.Log.enable();
  testRunner.log('Log Enabled');

  dp.Log.onEntryAdded(event => {
    const entry = event.params.entry;
    // Remove the error code, as it is platform-specific and can change.
    const text = entry.text.replace(/net::ERR_[A-Z_]+/, '[net error]');
    testRunner.log('Log.onEntryAdded');
    testRunner.log(`source: ${entry.source}`);
    testRunner.log(`level: ${entry.level}`);
    testRunner.log(`text: ${text}`);
    testRunner.completeTest();
  });

  session.evaluate(`new WebTransport('${url}');`);
  testRunner.log('Instantiate WebTransport.');
})
