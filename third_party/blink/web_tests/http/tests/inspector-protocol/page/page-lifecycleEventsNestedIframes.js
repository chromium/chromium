(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startBlank(
    'Tests that Page.lifecycleEvent is issued order correctly for nested iframes.'
  );

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });

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

  dp.Page.onLifecycleEvent((event) => {
    if (
      event.params.name === 'DOMContentLoaded' || event.params.name === 'load'
    ) {
      const frameNumber = getOrSetFrameNumber(event.params.frameId);
      testRunner.log('Frame ' + frameNumber + ': ' + event.params.name);
    }
  });

  await Promise.all([
    dp.Page.navigate({
      url: 'http://devtools.test:8000/inspector-protocol/page/resources/iframe-src-nested.html',
    }),
    dp.Page.onceLifecycleEvent((event) => event.params.name === 'networkIdle'),
  ]);
  testRunner.completeTest();
});
