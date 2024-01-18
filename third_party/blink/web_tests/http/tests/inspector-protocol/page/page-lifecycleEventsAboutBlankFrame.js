(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const idToFrameMap = new Map();
  let frameNumber = 1;

  function getOrSetFrameNumber(id) {
    let number = idToFrameMap.get(id);
    if (!number) {
      number = frameNumber;
      idToFrameMap.set(id, frameNumber);
      frameNumber++;
    }

    return number;
  }

  const { dp } = await testRunner.startBlank(
    'Tests that Page.lifecycleEvent is issued order correctly for iframes with explicit about:blank.',
  );

  await dp.Page.enable();
  await Promise.all([
    // Insure the initial navigation has completed
    dp.Page.onceLifecycleEvent((event) => {
      if (event.params.name === 'networkIdle') {
        // Track the main frame id.
        getOrSetFrameNumber(event.params.frameId);
        return true;
      }
      return false;
    }),
    dp.Page.setLifecycleEventsEnabled({ enabled: true }),
  ]);

  dp.Page.onLifecycleEvent((event) => {
    if (
      event.params.name === 'DOMContentLoaded' ||
      event.params.name === 'load'
    ) {
      const frameNumber = getOrSetFrameNumber(event.params.frameId);
      testRunner.log('Frame ' + frameNumber + ': ' + event.params.name);
    }
  });

  await Promise.all([
    /**
     * When we use about:blank
     * The events come in slightly different order
     * (observed in browser implementation as well at the time)
     * The reason seems that about:blank is parser immediately at creation
     * That is OK as we want to verify that:
     *  - Only 2 pairs are emitted
     *  - The load event of the main frame comes after the child frame was loaded
     * 1) iframe DomContentLoaded
     * 2) iframe Loaded
     * 3) mainFrame DomContentLoaded
     * 4) mainFrame Loaded
     */
    dp.Page.navigate({
      url: 'http://devtools.test:8000/inspector-protocol/page/resources/iframe-about-blank.html',
    }),
    dp.Page.onceLifecycleEvent((event) => event.params.name === 'networkIdle'),
  ]);
  testRunner.completeTest();
});
