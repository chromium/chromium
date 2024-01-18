(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.setBypassCSP works for main frame.`);

  await dp.Page.enable();
  await dp.Runtime.enable();

  testRunner.log('Verify CSP works when set with <meta>');
  await page.navigate('./resources/csp.html');
  await dumpCSPEnabled();

  testRunner.log('Verify CSP works when set with header');
  await page.navigate('./resources/csp.php');
  await dumpCSPEnabled();

  testRunner.log('\n>> ENABLING CSP BYPASS <<\n');
  await dp.Page.setBypassCSP({ enabled: true });

  testRunner.log('Verify CSP is bypassed when set with <meta>');
  await page.navigate('./resources/csp.html');
  await dumpCSPEnabled();

  testRunner.log('Verify CSP is bypassed when set with header');
  await page.navigate('./resources/csp.php');
  await dumpCSPEnabled();

  testRunner.log('Check bypass after cross-origin navigation');
  await page.navigate('http://127.0.0.1:8000/inspector-protocol/page/resources/csp.php');
  await page.navigate('https://127.0.0.1:8443/inspector-protocol/page/resources/csp.php');
  await dumpCSPEnabled();

  testRunner.completeTest();

  async function dumpCSPEnabled() {
    const message = await dp.Runtime.evaluate({ expression: 'window.__injected' });
    testRunner.log('  CSP bypassed: ' + (message.result.result.value === 42));
  }
})
