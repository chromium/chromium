(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that CSS media features can be overridden.');

  await session.navigate('../resources/css-media-features.html');

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
    const width = await session.evaluate('getComputedStyle(p).width');
    const height = await session.evaluate('getComputedStyle(p).height');
    testRunner.log(`${code} applied: ${width} x ${height}`);
  }

  async function setEmulatedMediaFeatures({ features, mediaQuery }) {
    await dp.Emulation.setEmulatedMedia({
      features,
    });
    const code = `matchMedia(${JSON.stringify(mediaQuery)}).matches`;
    const result = await session.evaluate(code);
    testRunner.log(`${code}: ${result}`);
    const width = await session.evaluate('getComputedStyle(p).width');
    const height = await session.evaluate('getComputedStyle(p).height');
    testRunner.log(`${code} applied: ${width} x ${height}`);
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
