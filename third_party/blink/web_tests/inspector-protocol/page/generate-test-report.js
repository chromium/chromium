(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      "Tests that Page.generateTestReport works for main frame.");

  await session.evaluate(`var observer = new ReportingObserver(function(reports, observer) {
    window.console.log("Reports: " + reports.length);

    // Ensure that the contents of the report are valid.
    window.console.log("Report Type: " + reports[0].type);
    window.console.log("Good URL: " + reports[0].url.endsWith("inspector-protocol-page.html"));
    window.console.log("Report Message: \\"" + reports[0].body.message + "\\"");
    window.console.log("DONE");
  });`);
  await session.evaluate(`observer.observe()`);

  await dp.Page.enable();
  await dp.Runtime.enable();
  testRunner.log("\n>> GENERATING TEST REPORT <<\n");
  await dp.Page.generateTestReport({ message: "Test message 42." });

  dp.Runtime.onConsoleAPICalled(result => {
    testRunner.log(result.params.args[0].value);
    if (result.params.args[0].value === 'DONE')
      testRunner.completeTest();
  });
})
