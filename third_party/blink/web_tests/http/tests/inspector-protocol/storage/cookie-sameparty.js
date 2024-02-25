(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
        `Verifies that the sameParty field will be handled correctly.\n`);

    var cookies = [
        {url: 'https://127.0.0.1', name: 'sameParty_true', value: 'bar1', sameParty: true, secure: true},
        {url: 'https://127.0.0.1', name: 'sameParty_false', value: 'bar1', sameParty: false, secure: true},
        // sameParty: false cookies should still be settable if secure if false;
        {url: 'http://127.0.0.1', name: 'sameParty_false_InsecureShouldSet', value: 'bar1', sameParty: false, secure: false},
    ];

    await dp.Storage.setCookies({cookies});

    testRunner.completeTest();
  })
