(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, page} = await testRunner.startBlank(
      `Tests that cookies are read and written.`);

  async function logCookies(title, browserContextId) {
    var data = (await dp.Storage.getCookies({ browserContextId })).result;
    testRunner.log(`\n${title}: ${data.cookies.length} cookies`);
    data.cookies.sort((a, b) => a.name.localeCompare(b.name));
    for (var cookie of data.cookies) {
      var suffix = ''
      if (cookie.secure)
        suffix += `, secure`;
      if (cookie.httpOnly)
        suffix += `, httpOnly`;
      if (cookie.session)
        suffix += `, session`;
      if (cookie.sameSite)
        suffix += `, ${cookie.sameSite}`;
      if (cookie.expires !== -1)
        suffix += `, expires`;
      testRunner.log(`name: ${cookie.name}, value: ${cookie.value}, domain: ${cookie.domain}, path: ${cookie.path}${suffix}`);
    }

    const body = (await dp.Runtime.evaluate({
        expression: `
            fetch('/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_COOKIE',
                { credentials: 'same-origin' })
            .then(r => r.text())`,
        awaitPromise: true,
        returnByValue: true
    })).result.result.value;
    testRunner.log(`Cookies as seen on server: ${JSON.stringify(body)}`);
  }

  async function setCookieViaFetch() {
    await dp.Runtime.evaluate({
        expression: `fetch('/inspector-protocol/network/resources/cookie.pl', { credentials: 'same-origin' })`,
        awaitPromise: true
    });
  }


  testRunner.runTestSuite([
    async function testPageContext() {
      await logCookies('Initial');

      await setCookieViaFetch();
      await logCookies('Post-fetch');

      const cookies = [
        {url: 'http://127.0.0.1', name: 'foo', value: 'bar1'},
        {url: 'http://127.0.0.1', name: 'foo', value: 'second bar2'},
        {url: 'http://127.0.0.1', name: 'foo2', value: 'bar1'}
      ];
      await dp.Storage.setCookies({cookies});
      await logCookies('Post-set');

      await dp.Storage.clearCookies();
      await logCookies('Post-clear');
    },

    async function testInvalidContext() {
      var data = (await dp.Storage.getCookies({ browserContextId: 'invalid' }));
      testRunner.log(data);
    }
  ]);

})
