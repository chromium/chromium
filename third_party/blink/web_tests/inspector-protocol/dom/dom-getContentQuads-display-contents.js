(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startURL('resources/display-contents.html',
    'Tests DOM.getContentQuads method with text inside display:contents elements.');

  await dp.DOM.enable();
  const aLinkQuads = await quadsFor(`document.querySelector('a')`);
  testRunner.log('Returned quads count: ' + aLinkQuads.length);
  const bQuads = await quadsFor(`document.querySelector('b')`);
  testRunner.log('Returned quads count: ' + bQuads.length);

  testRunner.completeTest();

  async function quadsFor(expression) {
    const { result } = await dp.Runtime.evaluate({ expression });
    testRunner.log(result);
    return (await dp.DOM.getContentQuads({ objectId: result.result.objectId })).result.quads;
  }

})

