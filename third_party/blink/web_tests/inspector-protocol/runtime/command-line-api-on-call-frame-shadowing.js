(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that command line API does not mask values of scope variables while evaluating on a call frame.');

  await dp.Debugger.enable();

  await session.evaluate(`
    window.inspect = "inspect";
    var clear = "clear";

    function testFunction() {
      var dir = "dir";
      debugger;
    }
  `);

  const evalPromise = session.evaluate('testFunction()');
  const {params: {callFrames: [{callFrameId}]}} =
      await dp.Debugger.oncePaused();

  const shadowedResult = await dp.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'dir + clear + inspect',
    includeCommandLineAPI: true,
  });
  testRunner.log('dir + clear + inspect:');
  testRunner.log(shadowedResult.result ?? shadowedResult);

  const commandLineApiResult = await dp.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'typeof $$',
    includeCommandLineAPI: true,
  });
  testRunner.log('typeof $$:');
  testRunner.log(commandLineApiResult.result ?? commandLineApiResult);

  await Promise.all([dp.Debugger.resume(), evalPromise]);
  testRunner.completeTest();
});
