(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests event listener breakpoints.');

  function finishIfError(message) {
    if (message.result)
      return;
    testRunner.log('FAIL: ' + JSON.stringify(message));
    testRunner.completeTest();
  }

  finishIfError(await dp.Debugger.enable());
  testRunner.log('PASS: Debugger was enabled');

  var errorResponse = dp.DOMDebugger.setEventListenerBreakpoint({eventName: 'click'});
  testRunner.log('Error on attempt to set event listener breakpoint when DOM is disabled: ' + JSON.stringify(errorResponse.error));

  finishIfError(await dp.DOM.enable());
  testRunner.log('PASS: DOM was enabled');

  finishIfError(await dp.DOMDebugger.setEventListenerBreakpoint({eventName:'click'}));
  testRunner.log('PASS: Listener was set.');

  finishIfError(await dp.DOM.disable());
  testRunner.log('PASS: DOM agent was disabled successfully.');

  var message = await dp.DOM.disable();
  if (!message.error)
    testRunner.log(`FAIL: we expected an error but it wasn't happen.`);
  else
    testRunner.log('PASS: The second attempt to disable DOM agent failed as expected.');
  testRunner.completeTest();
})
