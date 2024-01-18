(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML('<canvas id="cvn"></div>', 'Tests instrumentation breakpoint on webgl error');

  dp.Debugger.enable();
  dp.DOMDebugger.enable();
  dp.DOMDebugger.setInstrumentationBreakpoint({eventName: 'webglErrorFired'});
  dp.Runtime.evaluate({expression: `
    var canvas = document.getElementById("cvn");
    var gl = canvas.getContext("experimental-webgl");
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    //# sourceURL=test.js`});
  const {params:{callFrames, data, reason}} = await dp.Debugger.oncePaused();
  testRunner.logCallFrames(callFrames);
  testRunner.log(data);
  testRunner.log(reason);
  testRunner.completeTest();
})
