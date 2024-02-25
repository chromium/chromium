(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  const {_, session, dp} = await testRunner.startBlank(
      `Tests basic function of the WebAudio domain.`);

  let result, event, contextId;

  result = await dp.WebAudio.enable();
  testRunner.log(`Enabled successfully: ${!result.error}`);

  session.evaluateAsync('const context = new AudioContext();');
  event = await dp.WebAudio.onceContextCreated();
  contextId = event.params.context.contextId;
  testRunner.log(
    `context state after creation: ${event.params.context.contextState}`);
  testRunner.log(Object.keys(event.params.context).sort());

  // The context will automatically transition to 'running' after creation.
  // That's when |ContextChanged| event is fired.
  event = await dp.WebAudio.onceContextChanged();
  testRunner.log(
    `context state after suspension: ${event.params.context.contextState}`);
  testRunner.log(Object.keys(event.params.context).sort());

  const response = await dp.WebAudio.getRealtimeData({ contextId: contextId });
  testRunner.log(`got context realtime data: ${!response.result.realtimeData}`);
  testRunner.log(Object.keys(response.result.realtimeData).sort());

  // TODO(crbug.com/942615): Test |contextWillBeDestroyed| when the GC issue is
  // fixed.

  result = await dp.WebAudio.disable();
  testRunner.log(`Disabled successfully: ${!result.error}`);

  testRunner.completeTest();
});
