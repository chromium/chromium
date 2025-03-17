(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {dp, session} = await testRunner.startHTML(`
      <style>
      /* The following styles set the margin top/left/bottom/right to the
         values where the display feature between segments is, and the width and
         height of the div to the width and height of the display feature */
      @media (horizontal-viewport-segments: 2) {
        div {
          margin: env(viewport-segment-top 0 0, 10px)
                  env(viewport-segment-left 1 0, 10px)
                  env(viewport-segment-bottom 0 0, 10px)
                  env(viewport-segment-right 0 0, 10px);
          width: calc(env(viewport-segment-left 1 0, 10px) -
                      env(viewport-segment-right 0 0, 0px));
          height: env(viewport-segment-height 0 0, 10px);
        }
      }

      @media (vertical-viewport-segments: 2) {
        div {
          margin: env(viewport-segment-bottom 0 0, 11px)
                  env(viewport-segment-right 0 1, 11px)
                  env(viewport-segment-top 0 1, 11px)
                  env(viewport-segment-left 0 0, 11px);
          width: env(viewport-segment-width 0 0, 11px);
          height: calc(env(viewport-segment-top 0 1, 11px) -
                        env(viewport-segment-bottom 0 0, 0px));
        }
      }

      @media (horizontal-viewport-segments: 1) and
              (vertical-viewport-segments: 1) {
        div { opacity: 0.1; margin: 1px; width: 1px; height: 1px; }
      }

      @media (horizontal-viewport-segments: 2) and
              (vertical-viewport-segments: 1) {
        div { opacity: 0.2; }
      }

      @media (horizontal-viewport-segments: 1) and
              (vertical-viewport-segments: 2) {
        div { opacity: 0.3; }
      }
      </style>
      <div id='target'></div>
      `, `Test the Viewport Segments Environment Variables.`);

  async function dumpTargetComputedStyle() {
    testRunner.log(`getComputedStyle(target).marginTop :`);
    testRunner.log(await session.evaluate(`getComputedStyle(target).marginTop`));
    testRunner.log(`getComputedStyle(target).marginRight :`);
    testRunner.log(await session.evaluate(`getComputedStyle(target).marginRight`));
    testRunner.log(`getComputedStyle(target).marginBottom :`);
    testRunner.log(await session.evaluate(`getComputedStyle(target).marginBottom`));
    testRunner.log(`getComputedStyle(target).marginLeft :`);
    testRunner.log(await session.evaluate(`getComputedStyle(target).marginLeft`));
    testRunner.log(`getComputedStyle(target).width :`);
    testRunner.log(await session.evaluate(`getComputedStyle(target).width`));
    testRunner.log(`getComputedStyle(target).height :`);
    testRunner.log(await session.evaluate(`getComputedStyle(target).height`));
    testRunner.log(`getComputedStyle(target).opacity :`);
    testRunner.log(await session.evaluate(`getComputedStyle(target).opacity`));
  }

  const displayFeatureLength = 10;
  await session.evaluate(`
    let target = document.querySelector('#target');`);
  testRunner.log(`Initial layout for viewport size : ${window.innerWidth}x${window.innerHeight}`);
  await dumpTargetComputedStyle();

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
  await dumpTargetComputedStyle();

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
  await dumpTargetComputedStyle();

  await dp.Emulation.clearDisplayFeaturesOverride();
  testRunner.log(`Clearing the display feature should revert to the initial layout`);
  await dumpTargetComputedStyle();

  testRunner.completeTest();
})
