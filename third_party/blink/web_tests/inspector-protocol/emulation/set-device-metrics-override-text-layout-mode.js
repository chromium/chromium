(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const html = `
    <div id="unadjusted" style="font-size: 16px; text-size-adjust: none;">Text</div>
    <div id="adjusted" style="font-size: 16px; text-size-adjust: 200%;">Text</div>
  `;

  const {page, session, dp} = await testRunner.startBlank(
      'Tests Emulation.setDeviceMetricsOverride(textLayoutMode) affects CSS text-size-adjust.');

  async function getState() {
    // Navigate to apply text-size-adjust changes (https://crbug.com/564804235).
    await session.navigate('about:blank');
    await page.loadHTML(html);
    return await session.evaluate(() => {
      const unadjusted = parseFloat(
          getComputedStyle(document.getElementById('unadjusted')).fontSize);
      const adjusted = parseFloat(
          getComputedStyle(document.getElementById('adjusted')).fontSize);
      return adjusted === unadjusted * 2;
    });
  }

  async function assertState(expected, description) {
    const actual = await getState();
    if (actual === expected) {
      testRunner.log(`PASSED: ${description}`);
    } else {
      testRunner.log(
          `FAIL!: ${description} (expected ${expected}, got ${actual})`);
    }
  }

  const baseMetrics = {
    width: 800,
    height: 600,
    deviceScaleFactor: 1,
  };

  const initialState = await getState();

  testRunner.log('1. Mobile: false, textLayoutMode: "mobile"');
  await dp.Emulation.setDeviceMetricsOverride({
    ...baseMetrics,
    mobile: false,
    textLayoutMode: 'mobile',
  });
  await assertState(true, 'text-size-adjust is applied after navigation');

  testRunner.log('2. Mobile: false, textLayoutMode: "default"');
  await dp.Emulation.setDeviceMetricsOverride({
    ...baseMetrics,
    mobile: false,
    textLayoutMode: 'default',
  });
  await assertState(
      initialState,
      'text-size-adjust is restored to initial default after navigation');

  testRunner.log('3. Mobile: true, textLayoutMode: "default"');
  await dp.Emulation.setDeviceMetricsOverride({
    ...baseMetrics,
    mobile: true,
    textLayoutMode: 'default',
  });
  await assertState(true, 'text-size-adjust is applied after navigation');

  testRunner.log('4. Mobile: true, textLayoutMode: "mobile"');
  await dp.Emulation.setDeviceMetricsOverride({
    ...baseMetrics,
    mobile: true,
    textLayoutMode: 'mobile',
  });
  await assertState(true, 'text-size-adjust is applied after navigation');

  testRunner.log('5. Defaulting when textLayoutMode is omitted');
  await dp.Emulation.setDeviceMetricsOverride({
    ...baseMetrics,
    mobile: false,
  });
  await assertState(
      initialState, 'mobile: false restores initial default after navigation');

  await dp.Emulation.setDeviceMetricsOverride({
    ...baseMetrics,
    mobile: true,
  });
  await assertState(true, 'mobile: true applies text-size-adjust after navigation');

  testRunner.log('6. Clear device metrics override');
  await dp.Emulation.clearDeviceMetricsOverride();
  await assertState(
      initialState,
      'text-size-adjust is restored to initial default after navigation');

  testRunner.completeTest();
})
