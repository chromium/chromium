(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session, dp} = await testRunner.startBlank(
      'Tests Emulation.setDeviceMetricsOverride(scrollbarType) affects scrollbar width.');

  await session.navigate('../resources/set-scrollbars-hidden.html');

  async function getScrollbarWidth() {
    return await session.evaluate(`
      (function() {
        var outer = document.querySelector('.outer');
        // Force layout
        outer.scrollTop = 200;
        return outer.offsetWidth - outer.clientWidth;
      })()
    `);
  }

  const initialScrollbarWidth = await getScrollbarWidth();
  testRunner.log(`Initial scrollbar width: ${initialScrollbarWidth}`);

  // Test 1: Mobile: false, ScrollbarType: overlay
  testRunner.log('1. Mobile: false, ScrollbarType: overlay');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 0,
    height: 0,
    deviceScaleFactor: 0,
    mobile: false,
    scrollbarType: 'overlay'
  });
  const overlayWidth = await getScrollbarWidth();
  testRunner.log(
      overlayWidth === 0 ?
          'Scrollbar width is 0 (overlay)' :
          `ERROR: Scrollbar width is expected to be 0, but got ${
              overlayWidth}`);

  // Test 2: Mobile: true, ScrollbarType: default
  testRunner.log('2. Mobile: true, ScrollbarType: default');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 400,
    height: 600,
    deviceScaleFactor: 2,
    mobile: true,
    scrollbarType: 'default'
  });
  const mobileDefaultWidth = await getScrollbarWidth();
  testRunner.log(
      mobileDefaultWidth === 0 ?
          'Scrollbar width is 0 (overlay forced by mobile)' :
          `ERROR: Scrollbar width is expected to be 0, but got ${
              mobileDefaultWidth}`);

  // Test 3: Mobile: true, ScrollbarType: overlay
  testRunner.log('3. Mobile: true, ScrollbarType: overlay');
  await dp.Emulation.setDeviceMetricsOverride({
    width: 400,
    height: 600,
    deviceScaleFactor: 2,
    mobile: true,
    scrollbarType: 'overlay'
  });
  const mobileOverlayWidth = await getScrollbarWidth();
  testRunner.log(
      mobileOverlayWidth === 0 ?
          'Scrollbar width is 0 (overlay)' :
          `ERROR: Scrollbar width is expected to be 0, but got ${
              mobileOverlayWidth}`);

  // Test 4: Mobile: false, ScrollbarType: default
  testRunner.log('4. Mobile: false, ScrollbarType: default');
  // Explicitly clear override to see if it resets.
  // Although setDeviceMetricsOverride should handle it.
  await dp.Emulation.clearDeviceMetricsOverride();
  await dp.Emulation.setDeviceMetricsOverride({
    width: 0,
    height: 0,
    deviceScaleFactor: 0,
    mobile: false,
    scrollbarType: 'default'
  });
  const defaultWidth = await getScrollbarWidth();
  testRunner.log(
      defaultWidth === initialScrollbarWidth ?
          'Scrollbar width is restored to initial (default)' :
          `ERROR: Scrollbar width is expected to be ${
              initialScrollbarWidth}, but got ${defaultWidth}`);

  await dp.Emulation.clearDeviceMetricsOverride();
  testRunner.completeTest();
})
