(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that injected script is discarded upon front-end close.');
  var page = await testRunner.createPage();

  testRunner.log('Opening session #1');
  var session = await page.createSession();
  var {result} = await session.protocol.Runtime.evaluate({expression: '({ handle : "handle" })' });
  var objectId = result.result.objectId;
  var properties = await session.protocol.Runtime.getProperties({objectId: objectId, ownProperties: false});
  testRunner.log('Can resolve object: ' + !properties.error);
  testRunner.log('Disconnecting session #1');
  await session.disconnect();

  testRunner.log('Opening session #2');
  session = await page.createSession();
  properties = await session.protocol.Runtime.getProperties({objectId: objectId, ownProperties: false});
  testRunner.log('Can resolve object: ' + !properties.error);
  testRunner.log('Error:');
  testRunner.log(properties.error);
  testRunner.completeTest();
})
