(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startURL('resources/shadow-dom-link.html',
    'Tests DOM.getContentQuads method with text nodes inside shadow DOM.');

  await dp.DOM.enable();
  const aLinkQuads = await quadsFor(`document.querySelector('my-link').shadowRoot.querySelector('a')`);
  testRunner.log('Returned quads count: ' + aLinkQuads.length);
  const outerQuads = await quadsFor(`document.querySelector('my-link')`);
  testRunner.log('Quads are equal: ' + (JSON.stringify(aLinkQuads) === JSON.stringify(outerQuads)));

  testRunner.completeTest();

  async function quadsFor(expression) {
    const { result } = await dp.Runtime.evaluate({ expression });
    testRunner.log(result);
    return (await dp.DOM.getContentQuads({ objectId: result.result.objectId })).result.quads;
  }

})

