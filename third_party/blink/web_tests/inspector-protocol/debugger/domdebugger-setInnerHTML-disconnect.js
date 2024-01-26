(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='divUnderTest'></div>
  `, `Tests disconnect inside pause on setInnerHTML breakpoint.`);

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
  await session.disconnect();
  testRunner.completeTest();
})
