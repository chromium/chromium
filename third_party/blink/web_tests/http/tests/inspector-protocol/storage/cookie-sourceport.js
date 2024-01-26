(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
        `Verifies that the sourcePort field will be handled correctly.\n`);

    var cookies = [
        {url: 'https://127.0.0.1', name: 'sourcePort_Specified', value: 'bar1', sourcePort: 1234},
        {url: 'https://127.0.0.1', name: 'sourcePort_Unspecified', value: 'bar1'},
        {url: 'https://127.0.0.1:1234', name: 'sourcePort_Specified_Url_Specified_Match', value: 'bar1', sourcePort: 1234},
        {url: 'https://127.0.0.1:8443', name: 'sourcePort_Unspecified_Url_Specified', value: 'bar1'},
    ];
    await dp.Storage.setCookies({cookies});
    // Because this cookie expected to fail it must be set separately from the others.
    cookies = [{url: 'https://127.0.0.1:4433', name: 'sourcePort_Specified_Url_Specified_Different', value: 'bar1', sourcePort: 1234}];
    const invalidCookieResult = await dp.Storage.setCookies({cookies});

    const data = await dp.Storage.getCookies();

    for (let cookie of data.result.cookies) {
        testRunner.log(`${cookie.name}: ${cookie.sourcePort}`);
    }

    testRunner.log(invalidCookieResult);

    testRunner.completeTest();
  })
