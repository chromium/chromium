(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id="none-1">No stack</div>
    <script>
      var el = document.createElement('div');
      el.id = 'none-2';
      document.body.append(el);
    </script>
  `, 'Tests DOM.getNodeStackTraces method.');

  await dp.DOM.enable();
  await dp.DOM.setNodeStackTracesEnabled({enable: true});

  var document = (await dp.DOM.getDocument()).result.root;

  async function logStackTraces(selector) {
    var node = (await dp.DOM.querySelector({nodeId: document.nodeId, selector})).result;
    var stackTraces = (await dp.DOM.getNodeStackTraces({nodeId: node.nodeId})).result;
    testRunner.log(selector);
    testRunner.log(stackTraces);
  }

  await logStackTraces('#none-1');
  // No stack traces because `setNodeStackTracesEnabled` was called after node creation.
  await logStackTraces('#none-2');
  await session.evaluate(function createElementForStackTraces() {
    var el = document.createElement('div');
    el.id = 'stacks';
    document.body.append(el);
  });
  await logStackTraces('#stacks');

  testRunner.completeTest();
})
