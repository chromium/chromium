(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <style>
      body {
        height: 200%;
      }
    </style>
    <body></body>
  `, 'Tests isScrollable property for document using DOM.describeNode method.');
  dp.Runtime.enable();
  const {result}= await dp.Runtime.evaluate({expression: 'document'});
  const objectId = result.result.objectId;
  const description = await dp.DOM.describeNode({objectId});
  testRunner.log(`document.documentElement: isScrollable=${description.result.node.isScrollable}`);
  testRunner.completeTest();
})
