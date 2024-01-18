(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <input></input>
    <input id='second'></input>
  `, 'Tests DOM.focus method.');

  testRunner.log(await session.evaluate(getActiveElement));
  var document = (await dp.DOM.getDocument()).result.root;
  var node = (await dp.DOM.querySelector({nodeId: document.nodeId, selector: '#second'})).result;
  await dp.DOM.focus({nodeId: node.nodeId});
  testRunner.log(await session.evaluate(getActiveElement));
  testRunner.completeTest();

  function getActiveElement() {
    var el = document.activeElement;
    return el ? (el.id || el.tagName) : '(none)';
  }
})

