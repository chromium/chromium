(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.log('Test that prerender fails if a frame target without tab target is attached.');

  const targetId = (await testRunner.browserP().Target.createTarget({
                     url: 'about:blank',
                     forTab: false,
                   })).result.targetId;
  const sessionId = (await testRunner.browserP().Target.attachToTarget({
                      targetId,
                      flatten: true
                    })).result.sessionId;
  const session = testRunner.browserSession().createChild(sessionId);
  const dp = session.protocol;

  await dp.Page.enable();
  await dp.Preload.enable();

  session.navigate('resources/simple-prerender.html');

  testRunner.log(await dp.Preload.oncePrerenderStatusUpdated());

  testRunner.completeTest();
});
