(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Verify that interception works for file URLs`);

  await dp.Network.enable();
  await dp.Network.setRequestInterception({
    patterns: [{ urlPattern: '*' }]
  });
  await dp.Runtime.enable();
  const navigationPromise = session.navigate('./resources/simple.html');
  const mainResource = (await dp.Network.onceRequestIntercepted()).params;
  testRunner.log(`Intercepted: ${testRunner.trimURL(mainResource.request.url)}`);
  dp.Network.continueInterceptedRequest({
    interceptionId: mainResource.interceptionId
  });
  await navigationPromise;
  // If resource is interpreted as text/plain instead of text/html, the HTML will be escaped.
  testRunner.log(await session.evaluate(() => document.body.innerHTML));
  session.evaluate(`(() => {
    let script = document.createElement('script');
    script.src = 'simple.js';
    document.body.appendChild(script);
  })()`);
  const subresource = (await dp.Network.onceRequestIntercepted()).params;
  testRunner.log(`Intercepted: ${testRunner.trimURL(subresource.request.url)}`);
  dp.Network.continueInterceptedRequest({
    interceptionId: subresource.interceptionId
  });

  testRunner.completeTest();
})
