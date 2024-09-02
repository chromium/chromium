(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <style>
      #scroller {
        overflow: scroll;
        height: 200px;
        border: 1px solid black;
      }
      #scroller #contents {
        height: 200%;
      }
    </style>
    <div id="scroller">
      <div id="contents"></div>
    </div>
  `, 'Tests isScrollable property of a node using DOM.describeNode method.');
  dp.Runtime.enable();
  const {result}= await dp.Runtime.evaluate({expression: `document.getElementById('scroller')`});
  const objectId = result.result.objectId;
  const description = await dp.DOM.describeNode({objectId});
  testRunner.log(`isScrollable=${description.result.node.isScrollable}`);
  testRunner.completeTest();
})
