(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(`Test the Viewport Segments Segments Property.`);

  await session.navigate('../resources/device-emulation.html');
  const displayFeatureLength = 10;
  testRunner.log(`Initial layout for viewport size : ${window.innerWidth}x${window.innerHeight}`);
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));


  testRunner.log(`matchMedia('(horizontal-viewport-segments: 2)').matches :`);
  testRunner.log(await session.evaluate(`
    const horizontalMQL = window.matchMedia('(horizontal-viewport-segments: 2)');
    horizontalMQL.matches;
  `));
  const mediaQueryHorizontalViewportChanged = session.evaluateAsync(`
    new Promise(resolve => {
      horizontalMQL.addEventListener(
        'change',
        () => { resolve(horizontalMQL.matches); },
        { once: true }
      );
    })
  `);
  await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'vertical',
      maskLength: displayFeatureLength,
      offset: window.innerWidth / 2 - displayFeatureLength / 2
    }]
  })
  testRunner.log(
    `Media Query change event horizontal matches: ${await mediaQueryHorizontalViewportChanged}`);

  testRunner.log(`Horizontal layout`);
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));

  testRunner.log(`matchMedia('(vertical-viewport-segments: 2)').matches :`);
  testRunner.log(await session.evaluate(`
    const verticalMQL = window.matchMedia('(vertical-viewport-segments: 2)');
    verticalMQL.matches;
  `));
  const mediaQueryVerticalViewportChanged = session.evaluateAsync(`
    new Promise(resolve => {
      verticalMQL.addEventListener(
        'change',
        () => { resolve(verticalMQL.matches); },
        { once: true }
      );
    })
  `);
  await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'horizontal',
      maskLength: displayFeatureLength,
      offset: window.innerHeight / 2 - displayFeatureLength / 2
    }]
  })
  testRunner.log(
    `Media Query change event vertical matches: ${await mediaQueryVerticalViewportChanged}`);

  testRunner.log(`Vertical layout`);
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));

  await dp.Emulation.clearDisplayFeaturesOverride();
  testRunner.log(`Clearing the display feature should clear the segments property`);
  testRunner.log(await session.evaluate(`dumpViewportSegments()`));

  testRunner.completeTest();
})
