(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that multiple sessions receive log entries concurrently.');
  var page = await testRunner.createPage();
  var session1 = await page.createSession();
  var session2 = await page.createSession();

  var messages1 = [];
  session1.protocol.Log.onEntryAdded(event => messages1.push(event));

  var messages2 = [];
  session2.protocol.Log.onEntryAdded(event => messages2.push(event));

  function dumpMessages() {
    for (var message of messages1) {
      testRunner.log('From session1:');
      testRunner.log(message);
    }
    messages1 = [];
    for (var message of messages2) {
      testRunner.log('From session2:');
      testRunner.log(message);
    }
    messages2 = [];
  }

  const exampleDiscouragedApi = "document.write('Hello World!')";

  testRunner.log('Enabling logging in session1');
  session1.protocol.Log.enable();
  session1.protocol.Log.startViolationsReport({config: [{name: 'discouragedAPIUse', threshold: -1}]});
  testRunner.log('Triggering violation');
  await session1.evaluate(exampleDiscouragedApi);
  dumpMessages();

  testRunner.log('Enabling logging in session2');
  session2.protocol.Log.enable();
  session2.protocol.Log.startViolationsReport({config: [{name: 'discouragedAPIUse', threshold: -1}]});
  testRunner.log('Triggering violation');
  await session1.evaluate(exampleDiscouragedApi);
  dumpMessages();

  testRunner.log('Disabling logging in session1');
  session1.protocol.Log.disable();
  testRunner.log('Triggering violation');
  await session1.evaluate(exampleDiscouragedApi);
  dumpMessages();

  testRunner.log('Disabling logging in session2');
  session2.protocol.Log.disable();
  testRunner.log('Triggering violation');
  await session1.evaluate(exampleDiscouragedApi);
  dumpMessages();

  testRunner.completeTest();
})
