(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <body id="body"></body>
  `, 'Tests that the isScrollable property for a document is updated after the style of a node is changed using DOM.describeNode method.');
  function isScrollable(description) {
    return 'isScrollable' in description.result.node && description.result.node.isScrollable;
  }
  function forceLayoutUpdate() {
    const forceLayout = document.body.offsetWidth;
  }
  dp.Runtime.enable();
  const {result}= await dp.Runtime.evaluate({expression: 'document'});
  const objectId = result.result.objectId;
  let description = await dp.DOM.describeNode({objectId});
  testRunner.log(`Initial: isScrollable=${isScrollable(description)}`);

  await dp.Runtime.evaluate({expression: `document.getElementById('body').style.height = '200%';`});
  await session.evaluate(forceLayoutUpdate);

  description = await dp.DOM.describeNode({objectId});
  testRunner.log(`After style change: isScrollable=${isScrollable(description)}`);
  testRunner.completeTest();
})
