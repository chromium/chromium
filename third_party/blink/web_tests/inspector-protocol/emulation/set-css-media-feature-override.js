(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that CSS media features can be overridden.');

  await session.navigate('../resources/css-media-features.html');

  // For each emulated media feature, produce a list of corresponding
  // custom properties to inspect.
  async function formatComputedValues(features) {
    let props = features.map(f => `--${f}`);
    let values = [];
    for (let prop of props)
      values.push(await session.evaluate(`getComputedStyle(p).getPropertyValue('${prop}')`));
    return values.join('; ');
  }

  async function setEmulatedMediaFeature(feature, value) {
    await dp.Emulation.setEmulatedMedia({
      features: [
        {
          name: feature,
          value: value,
        },
      ],
    });
    const mediaQuery = `(${feature}: ${value})`;
    const code = `matchMedia(${JSON.stringify(mediaQuery)}).matches`;
    const result = await session.evaluate(code);
    testRunner.log(`${code}: ${result}`);
    const applied = await formatComputedValues([feature]);
    testRunner.log(`${code} applied: ${applied}`);
  }

  async function setEmulatedMediaFeatures({ features, mediaQuery }) {
    await dp.Emulation.setEmulatedMedia({
      features,
    });
    const code = `matchMedia(${JSON.stringify(mediaQuery)}).matches`;
    const result = await session.evaluate(code);
    testRunner.log(`${code}: ${result}`);
    const applied = await formatComputedValues(features.map(f => f.name));
    testRunner.log(`${code} applied: ${applied}`);
  }

  // Test `prefers-color-scheme`.
  // https://drafts.csswg.org/mediaqueries-5/#prefers-color-scheme
  await setEmulatedMediaFeature('prefers-color-scheme', '__invalid__');
  await setEmulatedMediaFeature('prefers-color-scheme', 'no-preference');
  await setEmulatedMediaFeature('prefers-color-scheme', 'light');
  await setEmulatedMediaFeature('prefers-color-scheme', 'dark');
  await setEmulatedMediaFeature('prefers-color-scheme', '__invalid__');

  // Test `prefers-reduced-motion`.
  // https://drafts.csswg.org/mediaqueries-5/#prefers-reduced-motion
  await setEmulatedMediaFeature('prefers-reduced-motion', '__invalid__');
  await setEmulatedMediaFeature('prefers-reduced-motion', 'no-preference');
  await setEmulatedMediaFeature('prefers-reduced-motion', 'reduce');
  await setEmulatedMediaFeature('prefers-reduced-motion', '__invalid__');

  // Test `prefers-reduced-data`.
  // https://drafts.csswg.org/mediaqueries-5/#prefers-reduced-data
  await setEmulatedMediaFeature('prefers-reduced-data', '__invalid__');
  await setEmulatedMediaFeature('prefers-reduced-data', 'no-preference');
  await setEmulatedMediaFeature('prefers-reduced-data', 'reduce');
  await setEmulatedMediaFeature('prefers-reduced-data', '__invalid__');

  // Test `prefers-reduced-transparency`.
  // https://drafts.csswg.org/mediaqueries-5/#prefers-reduced-transparency
  await setEmulatedMediaFeature('prefers-reduced-transparency', '__invalid__');
  await setEmulatedMediaFeature('prefers-reduced-transparency', 'no-preference');
  await setEmulatedMediaFeature('prefers-reduced-transparency', 'reduce');
  await setEmulatedMediaFeature('prefers-reduced-transparency', '__invalid__');

  // Test `prefers-contrast`.
  // https://drafts.csswg.org/mediaqueries-5/#prefers-contrast
  await setEmulatedMediaFeature('prefers-contrast', '__invalid__');
  await setEmulatedMediaFeature('prefers-contrast', 'no-preference');
  await setEmulatedMediaFeature('prefers-contrast', 'more');
  await setEmulatedMediaFeature('prefers-contrast', 'less');
  await setEmulatedMediaFeature('prefers-contrast', 'custom');
  await setEmulatedMediaFeature('prefers-contrast', '__invalid__');

  // Test `color-gamut`.
  // https://drafts.csswg.org/mediaqueries-5/#color-gamut
  await setEmulatedMediaFeature('color-gamut', '__invalid__');
  await setEmulatedMediaFeature('color-gamut', 'p3');
  await setEmulatedMediaFeature('color-gamut', 'rec2020');
  await setEmulatedMediaFeature('color-gamut', '__invalid__');

  // Test `forced-colors`.
  // https://drafts.csswg.org/mediaqueries-5/#forced-colors
  await setEmulatedMediaFeature('forced-colors', '__invalid__');
  await setEmulatedMediaFeature('forced-colors', 'active');
  await setEmulatedMediaFeature('forced-colors', 'none');
  await setEmulatedMediaFeature('forced-colors', '__invalid__');

  // Test combinations.
  await setEmulatedMediaFeatures({
    features: [
      { name: 'prefers-color-scheme', value: 'dark' },
      { name: 'prefers-reduced-motion', value: 'reduce' },
    ],
    mediaQuery: '(prefers-color-scheme: dark) and (prefers-reduced-motion: reduce)',
  });
  await setEmulatedMediaFeatures({
    features: [
      { name: 'prefers-color-scheme', value: '__invalid__' },
    ],
    mediaQuery: '(prefers-color-scheme: __invalid__)',
  });

  testRunner.completeTest();
});
