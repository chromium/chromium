(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Tests a basic quota setting workflow.\n`);

  testRunner.log('Initial storage');
  const origin = "http://localhost";
  const result1 = await dp.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result1.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.log('Applying override');
  await dp.Storage.overrideQuotaForOrigin(
      {origin, quotaSize: 10000 });

  const result3 = await dp.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result3.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.log('Connecting a second session');

  const dp2 = (await page.createSession()).protocol;
  const result4 = await dp2.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result4.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.log('Disconnecting original session (which held the override)');
  await session.disconnect();

  const result5 = await dp2.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result5.result, ["usage", "quota", "overrideActive"], 2));

  testRunner.completeTest();
})
