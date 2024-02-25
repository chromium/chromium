(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session: initialSession, dp} = await testRunner.startBlank(
      'Tests that disconnecting from the session clears the media query emulations');

  await initialSession.navigate('../resources/restore-emulated-media-on-disconnect.html');
  const features = [
    {
      name: 'prefers-color-scheme',
      value: 'dark',
    },
    {
      name: 'prefers-reduced-motion',
      value: 'reduce',
    },
    {
      name: 'prefers-reduced-data',
      value: 'reduce',
    },
    {
      name: 'prefers-reduced-transparency',
      value: 'reduce',
    },
    {
      name: 'prefers-contrast',
      value: 'more',
    },
    {
      name: 'color-gamut',
      value: 'p3',
    },
    {
      name: 'forced-colors',
      value: 'active',
    }
  ];

  async function formatComputedValues(activeSession) {
    let props = features.map(f => `--${f.name}`);
    let values = [];
    for (let prop of props) {
      const value = await activeSession.evaluate(`getComputedStyle(p).getPropertyValue('${prop}')`);
      values.push(`    ${prop}: ${value}`);
    }

    return values.join(';\n');
  }

  testRunner.log(`Computed values before setting emulated media:\n${await formatComputedValues(initialSession)}`);

  await dp.Emulation.setEmulatedMedia({
    features
  });
  testRunner.log(`Computed values after setting emulated media:\n${await formatComputedValues(initialSession)}`);

  await initialSession.disconnect();
  const nextSession = await page.createSession();
  testRunner.log(`Computed values after disconnect:\n${await formatComputedValues(nextSession)}`);

  testRunner.completeTest();
});
