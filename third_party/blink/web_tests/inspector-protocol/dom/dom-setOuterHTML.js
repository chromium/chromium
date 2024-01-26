(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id="id">Привет мир</div>
    <div>Привет мир 2</div>
  `, 'Tests how DOM domain works with outerHTML.');

  var message = await dp.DOM.getDocument();
  message = await dp.DOM.querySelector({ nodeId: message.result.root.nodeId, selector: "body" });
  var bodyId = message.result.nodeId;

  message = await dp.DOM.querySelector({ nodeId: bodyId, selector: "#id" });
  await dp.DOM.setOuterHTML({nodeId: message.result.nodeId, outerHTML: "<div>Привет мир 1</div>"});
  message = await dp.DOM.getOuterHTML({nodeId: bodyId});
  testRunner.log(message);
  testRunner.completeTest();
})
