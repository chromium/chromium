(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='divUnderTest'></div>
  `, `Tests pausing on setInnerHTML breakpoint.`);

  dp.Debugger.enable();
  dp.DOM.enable();
  dp.DOMDebugger.enable();
  dp.DOMDebugger.setInstrumentationBreakpoint({eventName: 'Element.setInnerHTML'});
  dp.Runtime.evaluate({expression: `





    (function modifyHTML() {
      document.getElementById('divUnderTest').innerHTML = 'innerHTML';
    })()
  ` });
  var messageObject = await dp.Debugger.oncePaused();
  var callFrame = messageObject.params.callFrames[0];
  testRunner.log('Paused on the innerHTML assignment: ' + callFrame.functionName + '@:' + callFrame.location.lineNumber);
  await dp.Debugger.resume();
  testRunner.completeTest();
})
