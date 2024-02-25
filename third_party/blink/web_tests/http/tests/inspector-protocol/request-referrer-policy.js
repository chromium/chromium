(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that network requests are annotated with the correct referrer policy.`);

  await dp.Network.enable();
  testRunner.log('Network agent enabled');

  var policies = [
    'unsafe-url', 'no-referrer-when-downgrade', 'no-referrer', 'origin',
    'origin-when-cross-origin'
  ];
  for (var policy of policies) {
    session.evaluate(`
      var img = document.createElement('img');
      img.referrerPolicy = '${policy}';
      img.src = '/resources/square.png?' + Math.random();
      document.body.appendChild(img);
    `);
    var evt = await dp.Network.onceRequestWillBeSent();
    var req = evt.params.request;
    if (req.referrerPolicy === policy)
      testRunner.log(`PASS: Request with expected policy ${policy} observed`);
    else
      testRunner.log(`FAIL: Request with policy ${req.referrerPolicy} observed (expected ${policy})`);
  }
  testRunner.completeTest();
})
