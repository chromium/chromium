(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Tests that disconnecting immediatelly after setting a quota works.\n`);
  testRunner.log('Initial storage');
  const origin = "http://localhost";
  const result1 = await dp.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result1.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.log('Applying override and immediatelly disconnecting.');
  dp.Storage.overrideQuotaForOrigin({ origin, quotaSize: 10000 });
  await session.disconnect();

  const dp2 = (await page.createSession()).protocol;
  const result4 = await dp2.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result4.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.completeTest();
})

