(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that timezone and locale overrides are rejected for untrusted sessions.');

  testRunner.setIsTrusted(false);
  const untrustedSession = await page.createSession();
  const untrustedDp = untrustedSession.protocol;

  testRunner.log('Calling Emulation.setTimezoneOverride on untrusted session:');
  const tzResult = await untrustedDp.Emulation.setTimezoneOverride({ timezoneId: 'Pacific/Honolulu' });
  testRunner.log(tzResult.error);

  testRunner.log('Calling Emulation.setLocaleOverride on untrusted session:');
  const localeResult = await untrustedDp.Emulation.setLocaleOverride({ locale: 'ar-EG' });
  testRunner.log(localeResult.error);

  testRunner.log('Calling Emulation.setTimezoneOverride on trusted session:');
  const trustedTzResult = await dp.Emulation.setTimezoneOverride({ timezoneId: 'Pacific/Honolulu' });
  testRunner.log(trustedTzResult.error ? trustedTzResult.error : 'SUCCESS');

  testRunner.log('Calling Emulation.setLocaleOverride on trusted session:');
  const trustedLocaleResult = await dp.Emulation.setLocaleOverride({ locale: 'ar-EG' });
  testRunner.log(trustedLocaleResult.error ? trustedLocaleResult.error : 'SUCCESS');

  testRunner.completeTest();
})
