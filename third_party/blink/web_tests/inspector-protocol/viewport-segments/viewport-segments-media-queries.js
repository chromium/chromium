(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
    `Test the Viewport Segments Media Queries.`);

  testRunner.log(`matchMedia('(vertical-viewport-segments: 2)').matches :`);
  testRunner.log(await session.evaluate(`(window.matchMedia('(vertical-viewport-segments: 2)').matches)`));

  testRunner.log(`matchMedia('(horizontal-viewport-segments: 2)').matches :`);
  testRunner.log(await session.evaluate(`(window.matchMedia('(horizontal-viewport-segments: 2)').matches)`));

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
    features : [ { orientation: 'vertical', maskLength: 20, offset: 20 } ]
  })
  testRunner.log(
    `Media Query change event horizontal matches: ${await mediaQueryHorizontalViewportChanged}`);


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
    features : [ { orientation: 'horizontal', maskLength: 20, offset: 20 } ]
  })
  testRunner.log(
    `Media Query change event vertical matches: ${await mediaQueryVerticalViewportChanged}`);
  testRunner.completeTest();
})
