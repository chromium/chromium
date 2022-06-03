(async function(testRunner) {
  const {page, session, dp} = await testRunner.startHTML('<div></div>',
     "Test replacing the whole document via setOuterHTML and ensure running " +
     "an event handler in the new document does not crash");

  const root = (await dp.DOM.getDocument()).result.root;
  const bodyId = (await dp.DOM.querySelector({ nodeId: root.nodeId, selector: "body" })).result.nodeId;

  await dp.DOM.setOuterHTML({nodeId: 0, outerHTML: "<a href='#' id='a' onclick='void(0);'></a>"});
  const evalMessage = await dp.Runtime.evaluate({ expression: `document.getElementById('a').click();` });
  testRunner.log((await dp.DOM.getOuterHTML({nodeId: bodyId})).result);
  testRunner.completeTest();
})
