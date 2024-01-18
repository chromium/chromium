(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that call to console after inspected context was destroyed shouldn't produce crash.`);
  await session.evaluate(`
    var iframe = document.createElement('iframe');
    document.body.appendChild(iframe);
    var a = window.console;
    window.console = iframe.contentWindow.console;
    iframe.contentWindow.console = a;
    iframe.remove();
  `);
  testRunner.log(await dp.Runtime.evaluate({expression: 'console.log(239);'}));
  testRunner.completeTest();
})
