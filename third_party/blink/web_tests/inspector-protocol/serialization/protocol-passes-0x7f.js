(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that \\0x7f passes through the protocol as is');
  const page = await testRunner.createPage();
  const session = await page.createSession();
  const str = await session.evaluate(`"\x7f"`);
  testRunner.log(`Got: "${str}", ${str.charCodeAt(0)} (length: ${str.length})`);
  testRunner.completeTest();
})
