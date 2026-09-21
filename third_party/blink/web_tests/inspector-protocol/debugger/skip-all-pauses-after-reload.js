(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that skip-all-pauses set while paused survives a reload.');

  await Promise.all([
    dp.Runtime.enable(),
    dp.Debugger.enable(),
    dp.Page.enable(),
  ]);

  const initialPause = dp.Debugger.oncePaused();
  dp.Runtime.evaluate({expression: 'debugger;'});
  await initialPause;
  testRunner.log('Paused before reload');

  await dp.Debugger.setSkipAllPauses({skip: true});
  const loadEvent = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await loadEvent;
  testRunner.log('Reloaded while paused');

  let pausedCount = 0;
  dp.Debugger.onPaused(() => {
    ++pausedCount;
    dp.Debugger.resume();
  });

  const result = await dp.Runtime.evaluate({expression: 'debugger; 42'});
  testRunner.log(`Evaluation result: ${result.result.result.value}`);
  testRunner.log(`Paused on new page: ${pausedCount !== 0}`);

  const pausesBeforeDisablingSkip = pausedCount;
  await dp.Debugger.setSkipAllPauses({skip: false});
  await dp.Runtime.evaluate({expression: 'debugger;'});
  testRunner.log(
      `Paused after disabling skip: ${pausedCount === pausesBeforeDisablingSkip + 1}`);

  testRunner.completeTest();
})
