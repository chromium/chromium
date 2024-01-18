(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='div'><span>text</span></div>
  `, 'Tests DOM.desctibeNode method.');
  dp.Runtime.enable();
  var {result} = await dp.Runtime.evaluate({expression: `document.getElementById('div')`});
  var {result} = await dp.DOM.describeNode({objectId: result.result.objectId});
  testRunner.log(result.node, 'DIV');

  var {result} = await dp.Runtime.evaluate({expression: `document.body`});
  var {result} = await dp.DOM.describeNode({objectId: result.result.objectId});
  testRunner.log(result.node, 'BODY');

  var {result} = await dp.Runtime.evaluate({expression: `document.body`});
  var {result} = await dp.DOM.describeNode({objectId: result.result.objectId, depth: -1});
  testRunner.log(result.node, 'BODY DEEP');

  testRunner.completeTest();
})
