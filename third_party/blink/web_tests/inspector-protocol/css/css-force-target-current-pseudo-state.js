(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <style>
      #target {
        color: red;
      }
      #target:target-current {
        color: green;
      }
    </style>
    <div id="target">test</div>
  `, 'Test CSS.forcePseudoStates method for :target-current');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#target');

  async function getTargetColor() {
    return await session.evaluate(() => {
      return window.getComputedStyle(document.querySelector('#target')).color;
    });
  }

  testRunner.log('Color without forced :target-current: ' + await getTargetColor());

  await dp.CSS.forcePseudoState({
    nodeId,
    forcedPseudoClasses: ['target-current'],
  });

  testRunner.log('Color with forced :target-current: ' + await getTargetColor());

  await dp.CSS.disable();
  await dp.DOM.disable();

  testRunner.completeTest();
});
