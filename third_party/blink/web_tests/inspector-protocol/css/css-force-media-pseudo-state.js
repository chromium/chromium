(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <style>
      video { color: red; }
      video:buffering { color: green; }
      video:stalled { color: blue; }
      video:buffering:stalled { color: purple; }
    </style>
    <video></video>
  `,
      'Test CSS.forcePseudoState method for :buffering and :stalled');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);
  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, 'video');

  async function logColor(description) {
    const color = await session.evaluate(
        () => getComputedStyle(document.querySelector('video')).color);
    testRunner.log(`${description}: ${color}`);
  }

  await logColor('Not forced');
  for (const forcedPseudoClasses
           of [['buffering'], ['stalled'], ['buffering', 'stalled']]) {
    await dp.CSS.forcePseudoState({nodeId, forcedPseudoClasses});
    await logColor(`Forced [${forcedPseudoClasses.join(', ')}]`);
  }

  await dp.CSS.disable();
  await dp.DOM.disable();
  testRunner.completeTest();
});
