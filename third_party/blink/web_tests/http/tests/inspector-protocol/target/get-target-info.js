(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that getTargetInfo works.`);

  const targetInfo = (await dp.Target.getTargetInfo()).result.targetInfo;
  const newTargetInfo = (await dp.Target.getTargetInfo()).result.targetInfo;
  if (JSON.stringify(targetInfo) !== JSON.stringify(newTargetInfo)) {
    testRunner.log('ERROR: targetInfo mismatch!');
    testRunner.log(targetInfo);
    testRunner.log(newTargetInfo);
    testRunner.completeTest();
    return;
  }
  testRunner.log(targetInfo);
  testRunner.completeTest();
})
