(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
  <script>
  function loadScript1() {
    var script = document.createElement('script');
    script.src = '../page/resources/script-1.js';
    document.head.appendChild(script);
  }

  function loadScript2() {
    var script = document.createElement('script');
    script.src = '../page/resources/script-2.js';
    document.head.appendChild(script);
  }
  </script>`, 'Tests that compilation cache is being generated and used');

  let counter = 0;
  dp.Runtime.onConsoleAPICalled(result => {
    testRunner.log(result.params.args[0].value);
    if (++counter == 2)
      testRunner.completeTest();
  });

  dp.Page.onCompilationCacheProduced(async result => {
    testRunner.log('Compilation cache produced for: ' + result.params.url);
    const url2 = result.params.url.replace(/-1/, '-2');
    testRunner.log('Poisoning cache for: ' + url2);
    await dp.Page.addCompilationCache({url: url2, data: result.params.data});
    testRunner.log('Loading script 2 with cache poisoined with script 1...');
    await session.evaluate('loadScript2()');
  });

  await dp.Page.enable();
  await dp.Page.setProduceCompilationCache({enabled: true});
  await dp.Runtime.enable();
  testRunner.log('Loading script 1...');
  await session.evaluate('loadScript1()');
})