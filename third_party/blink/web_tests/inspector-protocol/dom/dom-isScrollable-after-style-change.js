(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <style>
      #scroller {
        overflow: scroll;
        height: 200px;
        border: 1px solid black;
      }
    </style>
    <div id="scroller">
      <div id="contents"></div>
    </div>
  `, 'Tests that the isScrollable property is updated after the style of a node is changed using DOM.describeNode method.');
  await dp.DOM.enable();
  await dp.DOM.getDocument();

  function isScrollable(description) {
    return 'isScrollable' in description.result.node && description.result.node.isScrollable;
  }
  function forceLayoutUpdate() {
    const forceLayout = document.body.offsetWidth;
  }

  const {result}= await dp.Runtime.evaluate({expression: `document.getElementById('scroller')`});
  const objectId = result.result.objectId;
  let description = await dp.DOM.describeNode({objectId});
  testRunner.log(`Initial: isScrollable=${isScrollable(description)}`);

  await dp.Runtime.evaluate({expression: `document.getElementById('contents').style.height = '200%';`});
  await session.evaluate(forceLayoutUpdate);

  description = await dp.DOM.describeNode({objectId});
  testRunner.log(`After style change: isScrollable=${isScrollable(description)}`);
  testRunner.completeTest();
})
