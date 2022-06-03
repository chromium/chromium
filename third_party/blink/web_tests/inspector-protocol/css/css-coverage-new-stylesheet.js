(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Test that CSS coverage works for a newly added stylesheet.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  await dp.CSS.startRuleUsageTracking();
  await session.evaluateAsync(function(url) {
    const div = document.createElement('div');
    div.classList.add('usedAtTheVeryEnd');
    document.body.appendChild(div);

    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = url;
    document.head.appendChild(link);
    return new Promise(fulfill => link.onload = fulfill);
  }, testRunner.url('./resources/coverage2.css'));

  // The onload is not enough to guarantee that the rendering has completed
  // so we await an animation frame, too.
  await session.evaluateAsync(function() {
    let r;
    const p = new Promise(resolve => r = resolve);
    window.requestAnimationFrame(r);
    return p;
  });

  const response = await dp.CSS.stopRuleUsageTracking();

  if (response.result.ruleUsage.length === 1) {
    testRunner.log('Successfully reported CSS coverage.');
  } else {
    testRunner.log('ERROR!');
    testRunner.log(response.result.ruleUsage);
  }
  testRunner.completeTest();
});
