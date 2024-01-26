(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const TIMEOUT_MS = 1000;

  const {dp} = await testRunner.startBlank(
      'Tests that ProcessInfo is not available from frame target');

  const error = await dp.SystemInfo.getProcessInfo();
  testRunner.log(error);

  testRunner.completeTest();
})
