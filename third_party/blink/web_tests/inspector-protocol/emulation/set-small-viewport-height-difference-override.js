(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that the value of the svh unit can be overridden.');

  async function logViewportSizeDifference() {
    const offset = {};
    for (const unit of ['svh', 'lvh']) {
      offset[unit] = await session.evaluate((unit) => {
        const el = document.createElement('div');
        el.style.position = 'fixed';
        el.style.top = `100${unit}`;
        document.body.appendChild(el);
        const result = el.offsetTop;
        document.body.removeChild(el);
        return result;
      }, unit);
    }
    const diff = offset['lvh'] - offset['svh'];
    testRunner.log(`Difference between 100lvh and 100svh: ${diff}`);
  }

  testRunner.log('Initial state:');
  await logViewportSizeDifference();

  testRunner.log('Overriding small viewport height');
  await dp.Emulation.setSmallViewportHeightDifferenceOverride({difference: 10});

  await logViewportSizeDifference();

  testRunner.log('Reloading');
  await dp.Page.enable();
  let loadEvent = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await loadEvent;

  await logViewportSizeDifference();

  testRunner.log('Unsetting override');
  await dp.Emulation.setSmallViewportHeightDifferenceOverride({difference: 0});

  await logViewportSizeDifference();

  testRunner.log('Overriding and navigating cross-origin');
  await dp.Emulation.setSmallViewportHeightDifferenceOverride({difference: 10});
  loadEvent = dp.Page.onceLoadEventFired();
  await dp.Page.navigate({url: 'data:text/html,hello!'});
  await loadEvent;

  await logViewportSizeDifference();

  testRunner.completeTest();
})
