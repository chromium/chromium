(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
        `Verifies that the priority field will be handled correctly.\n`);

    const cookies = [
        {url: 'http://127.0.0.1', name: 'priority_high', value: 'bar1', priority: 'High'},
        {url: 'http://127.0.0.1', name: 'priority_low', value: 'bar1', priority: 'Low'},
        {url: 'http://127.0.0.1', name: 'priority_medium', value: 'bar1', priority: 'Medium'},
        {url: 'http://127.0.0.1', name: 'priority_invalid', value: 'bar1', priority: 'Invalid'},
        {url: 'http://127.0.0.1', name: 'priority_default', value: 'bar1'},
    ];

    await dp.Storage.setCookies({cookies});

    var data = await dp.Storage.getCookies();
    for(let cookie of data.result.cookies) {
        testRunner.log(`${cookie.name}: ${cookie.priority}`);
    }

    testRunner.completeTest();
  })
