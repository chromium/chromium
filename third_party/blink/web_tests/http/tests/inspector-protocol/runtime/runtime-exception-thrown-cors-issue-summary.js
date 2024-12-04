(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Runtime.exceptionThrown comes with a CORS issue summary.`);

  // This url should be cross origin.
  const url =
      `https://127.0.0.1:8443/inspector-protocol/network/resources/cors-headers.php`;

  await dp.Runtime.enable();

  const exceptionsThrown = [];
  const evaluate = async (url, options = {}) => {
    const exceptionThrownPromise = dp.Runtime.onceExceptionThrown();
    session.evaluate(`fetch('${url}', ${JSON.stringify(options)});`);
    const exceptionThrown = await exceptionThrownPromise;
    exceptionsThrown.push(exceptionThrown);
  };

  await evaluate(url, {mode: 'same-origin'});
  await evaluate(url);
  await evaluate(`${url}?origin=${encodeURIComponent('http://127.0.0.1')}`);
  await evaluate(`${url}?methods=GET&origin=1`, {
    method: 'POST',
    mode: 'cors',
    body: 'FOO',
    cache: 'no-cache',
    headers: {'Content-Type': 'application/json'},
  });

  for (const exception of exceptionsThrown) {
    const {exceptionMetaData} = exception.params.exceptionDetails;
    testRunner.log(exceptionMetaData, 'Exception: ', ['requestId', 'issueId']);
  }

  testRunner.completeTest();
})
