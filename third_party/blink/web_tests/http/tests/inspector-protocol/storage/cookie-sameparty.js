(async function(testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
        `Verifies that the sameParty field will be handled correctly.\n`);

    var cookies = [
        {url: 'https://127.0.0.1', name: 'sameParty_true', value: 'bar1', sameParty: true, secure: true},
        {url: 'https://127.0.0.1', name: 'sameParty_false', value: 'bar1', sameParty: false, secure: true},
        // sameParty: false cookies should still be settable if secure if false;
        {url: 'http://127.0.0.1', name: 'sameParty_false_InsecureShouldSet', value: 'bar1', sameParty: false, secure: false},
    ];

    await dp.Storage.setCookies({cookies});

    // sameParty: true cookies should fail if secure is false. Since this cookie
    // is expected to fail it needed to be set separately from the others (A
    // single invalid cookie will prevent the entire group from being set).
    cookies = [{url: 'http://127.0.0.1', name: 'sameParty_shouldFailToSet', value: 'bar1', sameParty: true, secure: false}];
    const invalidCookieResult = await dp.Storage.setCookies({cookies});

    const data = await dp.Storage.getCookies();

    for (let cookie of data.result.cookies) {
        testRunner.log(`${cookie.name}: ${cookie.sameParty}`);
    }

    testRunner.log(invalidCookieResult);

    testRunner.completeTest();
  })
