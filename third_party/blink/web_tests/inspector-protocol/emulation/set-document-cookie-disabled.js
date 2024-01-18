(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests Emulation.setDocumentCookieDisabled.');
  await dp.Emulation.setDocumentCookieDisabled({ disabled: true });
  const cookie = 'foo';
  testRunner.log(`Setting cookie to '${cookie}'`);
  await session.evaluate(`document.cookie = '${cookie}'`);
  testRunner.log(`Reading cookie: ${await session.evaluate(`'' + document.cookie`)}`);
  testRunner.completeTest();
})
