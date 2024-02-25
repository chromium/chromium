(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests Page.getFrameTree protocol method.');

  await dp.Page.enable();
  dp.Page.onFrameNavigated(async result => {
    var name = result.params.frame.name;
    if (name !== 'd')
      return;
    var frameTreeResponse = await dp.Page.getFrameTree();
    printFrameTree('', frameTreeResponse.result.frameTree);
    testRunner.completeTest();
  });

  page.loadHTML(
    `<iframe name='a' src='about:blank'></iframe>
     <iframe name='b' src='data:text/html,%3Ciframe%20name=%22d%22%20src=%22about:blank%22%3E%3C/iframe%3E'></iframe>`);

  function printFrameTree(indent, frameTree) {
    testRunner.log(`${indent}frame: ${frameTree.frame.name}`);
    if (frameTree.childFrames)
      frameTree.childFrames.forEach(printFrameTree.bind(null, `${indent}  `));
  }

})
