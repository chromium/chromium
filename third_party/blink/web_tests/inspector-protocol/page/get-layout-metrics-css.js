(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var { page, session, dp } = await testRunner.startHTML(`
        <div style='height: 10000px; width: 10000px'>content</div>
    `, 'Tests that Page.getLayoutMetrics returns reasonable values.');

  async function logLayoutMetrics(logMessage) {
    testRunner.log(logMessage);
    const metrics = await dp.Page.getLayoutMetrics();
    testRunner.log(metrics.result.cssLayoutViewport, "cssLayoutViewport");
    testRunner.log(metrics.result.cssContentSize, "cssContentSize");
    testRunner.log(metrics.result.cssVisualViewport, "cssVisualViewport");
  }

  await logLayoutMetrics("# initial metrics");

  await session.evaluate('window.scrollTo(100, 100);');
  await logLayoutMetrics("# scroll (100, 100)");

  await session.evaluate('internals.setPageScaleFactorLimits(2, 2)');
  await logLayoutMetrics("# internals.setPageScaleFactor(2)");

  await session.evaluate('internals.setPageScaleFactorLimits(0.5, 0.5)');
  await logLayoutMetrics("# internals.setPageScaleFactor(0.5)");

  testRunner.completeTest();
})
