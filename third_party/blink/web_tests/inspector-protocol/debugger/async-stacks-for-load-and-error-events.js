(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests async stacks for load and error events');
  let debuggerId = (await dp.Debugger.enable()).result.debuggerId;
  let debuggers = new Map([[debuggerId, dp.Debugger]]);
  dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  testRunner.log('Test load event..');
  await dp.Runtime.evaluate({
    expression: `
    let script = document.createElement('script');
    script.src = '${testRunner.url('resources/simple.js')}';
    script.addEventListener('load', onScriptLoad);
    document.body.appendChild(script);

    function onScriptLoad() {
      debugger;
    }
    //# sourceURL=test.js
  `
  });

  let {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
      await dp.Debugger.oncePaused();
  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      debuggerId);
  dp.Debugger.resume();

  testRunner.log('Test error event..');
  await dp.Runtime.evaluate({
    expression: `
    script = document.createElement('script');
    script.src = '${testRunner.url('resources/should-be-no-script.js')}';
    script.addEventListener('error', onScriptError);
    document.body.appendChild(script);

    function onScriptError() {
      debugger;
    }
    //# sourceURL=test.js
  `
  });

  ({params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
       await dp.Debugger.oncePaused());
  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      debuggerId);
  dp.Debugger.resume();

  testRunner.completeTest();
})
