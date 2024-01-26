(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that two quota messages are handled correctly.\n`);
  testRunner.log('Initial storage');
  const originA = 'http://localhost';
  const originB = 'http://example.com';
  let result;

  testRunner.log('Applying overrides');
  const override1 = dp.Storage.overrideQuotaForOrigin(
      {origin: originA, quotaSize: 10000 });
  const override2 = dp.Storage.overrideQuotaForOrigin(
      {origin: originB, quotaSize: 9100 });

  await Promise.all([override1, override2]);

  testRunner.log('Quota for origin A');
  result = await dp.Storage.getUsageAndQuota({origin: originA});
  testRunner.log(
      JSON.stringify(result.result, ['usage', 'quota', 'overrideActive'], 2));
  testRunner.log('Quota for origin B');
  result = await dp.Storage.getUsageAndQuota({origin: originB});
  testRunner.log(
      JSON.stringify(result.result, ['usage', 'quota', 'overrideActive'], 2));

  testRunner.log('Disabling quota for origin A');
  await dp.Storage.overrideQuotaForOrigin(
      {origin: originA });

  testRunner.log('Quota for origin A');
  result = await dp.Storage.getUsageAndQuota({origin: originA});
  testRunner.log(
      JSON.stringify(result.result, ['usage', 'quota', 'overrideActive'], 2));
  testRunner.log('Quota for origin B');
  result = await dp.Storage.getUsageAndQuota({origin: originB});
  testRunner.log(
      JSON.stringify(result.result, ['usage', 'quota', 'overrideActive'], 2));

  testRunner.log('Disconnect session');
  await session.disconnect();

  const dp2 = (await page.createSession()).protocol;
  testRunner.log('Quota for origin A');
  result = await dp2.Storage.getUsageAndQuota({origin: originA});
  testRunner.log(
      JSON.stringify(result.result, ['usage', 'quota', 'overrideActive'], 2));
  testRunner.log('Quota for origin B');
  result = await dp2.Storage.getUsageAndQuota({origin: originB});
  testRunner.log(
      JSON.stringify(result.result, ['usage', 'quota', 'overrideActive'], 2));

  testRunner.completeTest();
})
