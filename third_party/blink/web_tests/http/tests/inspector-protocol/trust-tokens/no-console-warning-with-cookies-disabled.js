(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Check that no console error is printed on a Trust Tokens failure due to the subsystem being unavailable with third party cookies disabled.`);
  await dp.Log.enable();
  testRunner.log('Log Enabled');

  let numEntries = 0;
  dp.Log.onEntryAdded(event => {
    ++numEntries;

    const entry = event.params.entry;
    testRunner.log('Log.onEntryAdded');
    testRunner.log(`source: ${entry.source}`);
    testRunner.log(`level: ${entry.level}`);
    testRunner.log(`text: ${entry.text}`);
  });

  await session.evaluate(`testRunner.setBlockThirdPartyCookies(true);`);

  const obtainedError = await session.evaluateAsync(`
      fetch('/issue', {trustToken:{type:'token-request'}}).catch((e)=>e.toString());
        `);
  testRunner.log(`Trust Tokens operation concluded with expected failure, throwing exception "${obtainedError}".`);

  if (numEntries > 0) {
    testRunner.fail(`Shouldn't observe any errors in the console, but saw ${numEntries} many.`);
  }

  testRunner.completeTest();
})
