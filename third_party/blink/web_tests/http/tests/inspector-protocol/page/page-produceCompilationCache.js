(async function(testRunner) {
    const {page, session, dp} = await testRunner.startHTML(`
    <script>
    function loadScript(url) {
      var script = document.createElement('script');
      script.src = url;
      document.head.appendChild(script);
    }
    </script>`, 'Tests that compilation cache is only being produced for specified scripts');

    dp.Page.onCompilationCacheProduced(async result => {
      testRunner.log('Compilation cache produced for: ' + result.params.url);
    });

    dp.Page.enable();
    const scriptUrls = [
        testRunner.url('resources/script-1.js'),
        testRunner.url('resources/script-2.js')
    ];
    dp.Page.produceCompilationCache({
        scripts: [{url: scriptUrls[0]}]
    });
    dp.Runtime.enable();

    for (let i = 0; i < 2; ++i) {
      session.evaluate(`loadScript("${scriptUrls[i]}")`);
      const message = (await dp.Runtime.onceConsoleAPICalled()).params.args[0].value;
      testRunner.log(`Page: ${message}`);
    }

    dp.Page.produceCompilationCache({
      scripts: [{url: scriptUrls[0] + '?v2'}]
    });
    dp.Page.setProduceCompilationCache({ enabled: false });
    dp.Page.produceCompilationCache({
        scripts: [{url: scriptUrls[1] + '?v2'}]
    });

    for (let i = 0; i < 2; ++i) {
      session.evaluate(`loadScript("${scriptUrls[i]}?v2")`);
      const message = (await dp.Runtime.onceConsoleAPICalled()).params.args[0].value;
      testRunner.log(`Page: ${message}`);
    }

    testRunner.completeTest();
})
