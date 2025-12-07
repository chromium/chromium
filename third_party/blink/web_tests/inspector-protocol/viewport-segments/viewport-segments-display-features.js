(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
    `Test the CDP Display Features.`);
  testRunner.log(`Viewport size : ${window.innerWidth}x${window.innerHeight}`);

  // Two valid display features.
  let response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'vertical',
      maskLength: 10,
      offset: 10
    }]
  })
  testRunner.log(response.error ? 'FAIL' : 'PASS');

  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'horizontal',
      maskLength: 10,
      offset: 10
    }]
  })
  testRunner.log(response.error ? 'FAIL' : 'PASS');

  // DisplayFeatures with negative value should fail.
  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'horizontal',
      maskLength: -10,
      offset: 10
    }]
  })
  testRunner.log(response.error ? 'PASS: ' + response.error.message : 'FAIL');

  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'horizontal',
      maskLength: 10,
      offset: -10
    }]
  })
  testRunner.log(response.error ? 'PASS: ' + response.error.message : 'FAIL');

  // DisplayFeatures with wrong orientation should fail.
  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'test',
      maskLength: 10,
      offset: -10
    }]
  })
  testRunner.log(response.error ? 'PASS: ' + response.error.message : 'FAIL');

  // DisplayFeatures outside of the viewport bounds should fail.
  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'horizontal',
      maskLength: window.innerHeight,
      offset: 20
    }]
  })
  testRunner.log(response.error ? 'PASS: ' + response.error.message : 'FAIL');

  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'horizontal',
      maskLength: 20,
      offset: window.innerWidth
    }]
  })
  testRunner.log(response.error ? 'PASS: ' + response.error.message : 'FAIL');

  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'vertical',
      maskLength: 10,
      offset: window.innerWidth
    }]
  })
  testRunner.log(response.error ? 'PASS: ' + response.error.message : 'FAIL');

  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'vertical',
      maskLength: window.innerWidth,
      offset: 10
    }]
  })
  testRunner.log(response.error ? 'PASS: ' + response.error.message : 'FAIL');

  // Chromium specific, we only supports one display feature.
  // TODO(crbug.com/40113439): Remove this and test it.
  response = await dp.Emulation.setDisplayFeaturesOverride({
    features : [{
      orientation: 'horizontal',
      maskLength: 10,
      offset: 10
    },
    {
      orientation: 'vertical',
      maskLength: 10,
      offset: 10
    }],
  })
  testRunner.log(response.error ? 'PASS: ' + response.error.message : 'FAIL');

  await session.evaluate(`const horizontalMQL = window.matchMedia('(horizontal-viewport-segments: 2)');`);
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
    features : [ { orientation: 'vertical', maskLength: 200, offset: 200 } ]
  })
  testRunner.log(
    `Horizontal MQ matches: ${await mediaQueryHorizontalViewportChanged}`);

  //If the new emulated viewport size is making the display feature invalid the MQs shouldn't match.
  await dp.Emulation.setDeviceMetricsOverride({
    width: 100,
    height: 100,
    deviceScaleFactor: 2.5,
    mobile: true
  });

  testRunner.log(`Horizontal MQ matches: ` +
    await session.evaluate(`window.matchMedia('(horizontal-viewport-segments: 2)').matches;`));

  testRunner.completeTest();
})
