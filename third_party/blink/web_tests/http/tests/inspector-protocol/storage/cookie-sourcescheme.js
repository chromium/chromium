(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
        `Verifies that the sourceScheme field will be handled correctly.\n`);

    const cookies = [
        {url: 'https://127.0.0.1', name: 'sourceScheme_Secure', value: 'bar1', sourceScheme: 'Secure'},
        {url: 'http://127.0.0.1', name: 'sourceScheme_NonSecure', value: 'bar1', sourceScheme: 'NonSecure'},
        {url: 'http://127.0.0.1', name: 'sourceScheme_Unset', value: 'bar1', sourceScheme: 'Unset'},
        {url: 'https://127.0.0.1', name: 'sourceScheme_UndefinedSecureURL', value: 'bar1'},
        {url: 'http://127.0.0.1', name: 'sourceScheme_UndefinedNonSecureURL', value: 'bar1'},
        {url: 'http://127.0.0.1', name: 'sourceScheme_UndefinedNonSecureURL_SecureAttribute', value: 'bar1', secure: true}
    ];
    await dp.Storage.setCookies({cookies});

    const data = await dp.Storage.getCookies();

    for (let cookie of data.result.cookies) {
        testRunner.log(`${cookie.name}: ${cookie.sourceScheme}`);
    }

    testRunner.completeTest();
  })
