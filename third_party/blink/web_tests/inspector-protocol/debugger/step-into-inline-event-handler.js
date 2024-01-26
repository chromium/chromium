(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <script>
    function testFunction() {
      var e = document.getElementById('div');
      debugger;
      e.click();
    }

    function shouldNotBeThisFunction() {
      return 239;
    }
    </script>
    <div id='div' onclick='shouldNotBeThisFunction()'></div>
  `, `Tests that Debugger.stepInto doesn't ignore inline event listeners.`);


  function dumpTopCallFrame(result) {
    var frame = result.params.callFrames[0];
    testRunner.log('functionName (should be onclick): ' + (frame.functionName.length ? frame.functionName : 'empty'));
  }

  await dp.Debugger.enable();
  var finished = dp.Runtime.evaluate({expression: 'testFunction()'});

  await dp.Debugger.oncePaused();
  dp.Debugger.stepInto();
  await dp.Debugger.oncePaused();
  dp.Debugger.stepInto();
  dumpTopCallFrame(await dp.Debugger.oncePaused());
  dp.Debugger.resume();

  await finished;
  testRunner.completeTest();
})
