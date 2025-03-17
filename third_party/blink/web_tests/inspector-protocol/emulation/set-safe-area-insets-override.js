(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that the safe-area-(max-)inset-* environment variables can be overridden.');

  async function logEnvironment() {
    for (const position of ['top', 'left', 'bottom', 'right']) {
      for (const max of ['', 'max-']) {
        const envVar = `safe-area-${max}inset-${position}`;
        const offset = await session.evaluate((envVar) => {
          const el = document.createElement('div');
          el.style.position = 'fixed';
          el.style.top = `env(${envVar}, 42)`;
          document.body.appendChild(el);
          const result = el.offsetTop;
          document.body.removeChild(el);
          return result;
        }, envVar);
        testRunner.log(`${envVar}: ${offset}`);
      }
    }
  }

  testRunner.log('Initial environment:');
  await logEnvironment();

  testRunner.log('Overriding insets');
  await dp.Emulation.setSafeAreaInsetsOverride({
    insets: {
      top: 1,
      topMax: 2,
      left: 3,
      leftMax: 4,
      bottom: 5,
      bottomMax: 6,
      right: 7,
      rightMax: 8
    }
  });

  testRunner.log('New environment:');
  await logEnvironment();

  testRunner.log('Reloading');
  await dp.Page.enable();
  let loadEvent = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await loadEvent;

  testRunner.log('Environment:');
  await logEnvironment();

  testRunner.log('Unsetting override');
  await dp.Emulation.setSafeAreaInsetsOverride({insets: {}});

  testRunner.log('Environment:');
  await logEnvironment();

  testRunner.log('Overriding and navigating cross-origin');
  await dp.Emulation.setSafeAreaInsetsOverride({
    insets: {
      top: 1,
      topMax: 2,
      left: 3,
      leftMax: 4,
      bottom: 5,
      bottomMax: 6,
      right: 7,
      rightMax: 8
    }
  });
  loadEvent = dp.Page.onceLoadEventFired();
  await dp.Page.navigate({url: 'data:text/html,hello!'});
  await loadEvent;

  testRunner.log('Environment:');
  await logEnvironment();

  testRunner.completeTest();
})
