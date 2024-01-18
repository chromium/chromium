(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests function location in CallFrame.');

  dp.Debugger.enable();
  dp.Runtime.evaluate({expression: `




function testFunction()
{
    var a = 2;
    debugger;
}
setTimeout(testFunction, 0);
  `});

  var messageObject = await dp.Debugger.oncePaused();
  testRunner.log(`Paused on 'debugger;'`);
  var topFrame = messageObject.params.callFrames[0];
  topFrame.location.scriptId = '42';
  topFrame.functionLocation.scriptId = '42';
  testRunner.log('Top frame location: ' + JSON.stringify(topFrame.location));
  testRunner.log('Top frame functionLocation: ' + JSON.stringify(topFrame.functionLocation));
  testRunner.completeTest();
})
