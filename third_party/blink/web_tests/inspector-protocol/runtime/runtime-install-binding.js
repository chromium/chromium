(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Runtime.installBinding is preserved on navigations and ' +
      'injected before the addScriptOnNewDocument is run.');
  dp.Runtime.enable();
  dp.Page.enable();

  testRunner.log('Install binding..');
  testRunner.log(await dp.Runtime.addBinding({name: 'send'}));
  dp.Runtime.onConsoleAPICalled(msg => testRunner.log(msg));
  testRunner.log('Add script to replace console.debug with binding on load..');
  await dp.Page.addScriptToEvaluateOnNewDocument({source: `
    console.debug = send;
  `});
  testRunner.log('Add iframe with console.debug call..');
  session.evaluateAsync(`
    function appendIframe(url) {
      var frame = document.createElement('iframe');
      frame.id = 'iframe';
      frame.src = url;
      document.body.appendChild(frame);
      return new Promise(resolve => frame.onload = resolve);
    }
    appendIframe('${testRunner.url('../resources/binding.html')}')
  `);
  testRunner.log(await dp.Runtime.onceBindingCalled());
  testRunner.log('Navigate to page with console.debug..');
  await dp.Page.navigate({url: testRunner.url('../resources/binding.html')});
  testRunner.log(await dp.Runtime.onceBindingCalled());
  testRunner.completeTest();
})
