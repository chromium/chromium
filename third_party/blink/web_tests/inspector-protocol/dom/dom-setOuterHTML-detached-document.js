(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id="id">something</div> `, 'Tests that setOuterHTML does not crash on detached document.');

  var message = await dp.DOM.getDocument();

  await dp.Runtime.enable();
  await dp.Runtime.evaluate({
    expression: "document.documentElement.remove()"
  });

  var result = await dp.DOM.setOuterHTML({nodeId: message.result.root.nodeId, outerHTML: "<div>update</div>"});
  testRunner.log(result);
  testRunner.completeTest();
})
