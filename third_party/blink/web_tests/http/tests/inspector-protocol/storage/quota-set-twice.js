(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that two quota messages are handled correctly.\n`);
  testRunner.log('Initial storage');
  const origin = "http://localhost";
  const result1 = await dp.Storage.getUsageAndQuota({origin});
  testRunner.log(JSON.stringify(result1.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.log('Applying override');
  const result2 = dp.Storage.overrideQuotaForOrigin({origin, quotaSize: 10000 });
  const result3 = dp.Storage.overrideQuotaForOrigin({origin, quotaSize: 9100 });

  await Promise.all([result2, result3]);

  const result4 = await dp.Storage.getUsageAndQuota({origin});
  testRunner.log(JSON.stringify(result4.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.completeTest();
})
