(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test performance domain enable method time domain specification.`
      );

  function logErrorMessage(result) {
    testRunner.log(result.error ? result.error.message : 'OK');
  }

  testRunner.log('--- Default enable/disable');
  logErrorMessage(await dp.Performance.enable());
  logErrorMessage(await dp.Performance.disable());

  testRunner.log('--- Default nested enable/disable');
  logErrorMessage(await dp.Performance.enable());
  logErrorMessage(await dp.Performance.enable());
  logErrorMessage(await dp.Performance.disable());
  logErrorMessage(await dp.Performance.disable());

  testRunner.log('--- Enable with time domain specification');
  logErrorMessage(await dp.Performance.enable({timeDomain: 'timeTicks'}));
  logErrorMessage(await dp.Performance.disable());

  logErrorMessage(await dp.Performance.enable({timeDomain: 'threadTicks'}));
  logErrorMessage(await dp.Performance.disable());

  logErrorMessage(await dp.Performance.enable({timeDomain: 'bogusTicks'}));
  logErrorMessage(await dp.Performance.disable());

  testRunner.log('--- Nested enable with time domain specification');
  logErrorMessage(await dp.Performance.enable());
  logErrorMessage(await dp.Performance.enable({timeDomain: 'timeTicks'}));
  logErrorMessage(await dp.Performance.enable({timeDomain: 'threadTicks'}));
  logErrorMessage(await dp.Performance.disable());
  logErrorMessage(await dp.Performance.disable());
  logErrorMessage(await dp.Performance.disable());

  testRunner.completeTest();
})
