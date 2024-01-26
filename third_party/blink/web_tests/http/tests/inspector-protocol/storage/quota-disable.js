(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Tests that withdrawing maintains the quota.\n`);
  testRunner.log('Initial storage');
  const origin = "http://localhost";
  const result1 = await dp.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result1.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.log('Applying overrides from two sessions');
  await dp.Storage.overrideQuotaForOrigin({ origin, quotaSize: 10000 });

  const dp2 = (await page.createSession()).protocol;
  await dp2.Storage.overrideQuotaForOrigin({ origin, quotaSize: 9000 });

  const dp3 = (await page.createSession()).protocol;
  const result2 = await dp3.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result2.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.log('Disabling the override');
  await dp3.Storage.overrideQuotaForOrigin({ origin });

  const result3 = await dp.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result3.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.completeTest();
})

