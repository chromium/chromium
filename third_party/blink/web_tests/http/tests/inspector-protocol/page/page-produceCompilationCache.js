(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startHTML(`
    <script>
    function loadScript(url) {
      var script = document.createElement('script');
      script.src = url;
      document.head.appendChild(script);
    }
    </script>`, 'Tests that compilation cache is only being produced for specified scripts');

    dp.Page.enable();
    const scriptUrls = [
        testRunner.url('resources/script-1.js'),
        testRunner.url('resources/script-2.js')
    ];
    dp.Page.produceCompilationCache({
        scripts: [{url: scriptUrls[0]}]
    });
    dp.Runtime.enable();

    dp.Page.onCompilationCacheProduced(result => {
      testRunner.log('Compilation cache produced for: ' + result.params.url);
      const newUrl = result.params.url.replace("-1.js", "-2.js?v2");
      // Forge a new cache entry for the wrong script.
      dp.Page.addCompilationCache({url: newUrl, data: result.params.data});
    });

    for (let i = 0; i < 2; ++i) {
      testRunner.log(`Running ${scriptUrls[i]}`);
      session.evaluate(`loadScript("${scriptUrls[i]}")`);
      const message = (await dp.Runtime.onceConsoleAPICalled()).params.args[0].value;
      testRunner.log(`Page: ${message}`);
    }

    // This should still run the right script.
    testRunner.log(`Running ${scriptUrls[1]}?v1`);
    session.evaluate(`loadScript("${scriptUrls[1]}?v1")`);
    const message = (await dp.Runtime.onceConsoleAPICalled()).params.args[0].value;
    testRunner.log(`Page: ${message}`);

    testRunner.completeTest();
})
