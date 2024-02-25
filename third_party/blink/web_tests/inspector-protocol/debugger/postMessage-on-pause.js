(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that worker can be paused in postMessage.');

  dp.Debugger.enable();
  dp.Runtime.evaluate({expression: `
    var messageDispatched = false;
    window.addEventListener('message', event => {
      messageDispatched = true;
      debugger;
    }, true);

    (function testFunction() {
      window.postMessage('test', '*');
      debugger;
    })()
  `});

  await dp.Debugger.oncePaused();
  testRunner.log(`Paused on 'debugger;'`);

  var messageObject = await dp.Runtime.evaluate({expression: 'messageDispatched' });
  var r = messageObject.result.result;
  if (r.type === 'boolean' && r.value === false)
    testRunner.log('PASS: message has not been dispatched yet.');
  else
    testRunner.log('FAIL: unexpected response ' + JSON.stringify(messageObject, null, 2));

  messageObject = await dp.Runtime.evaluate({expression: 'messageDispatched' });
  r = messageObject.result.result;
  if (r.type === 'boolean' && r.value === false)
    testRunner.log('PASS: message has not been dispatched yet.');
  else
    testRunner.log('FAIL: unexpected response ' + JSON.stringify(messageObject, null, 2));

  await dp.Debugger.resume();
  testRunner.log('Resumed, now waiting for pause in the event listener...');

  await dp.Debugger.oncePaused();
  testRunner.log('PASS: pasued in the event listener.');
  await dp.Debugger.resume();
  testRunner.completeTest();
})
