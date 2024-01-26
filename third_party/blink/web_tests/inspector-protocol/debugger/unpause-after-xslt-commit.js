(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL('resources/xslt.xml',
      'Test that debugger breakpoints still work after reloading an xslt document');

  await dp.Debugger.enable();
  await dp.Page.enable();
  await dp.Runtime.enable();
  await Promise.all([
    dp.Page.reload(),
    dp.Debugger.oncePaused(),
  ]);
  testRunner.completeTest();
})
