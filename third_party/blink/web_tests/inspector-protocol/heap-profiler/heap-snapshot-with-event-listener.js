(async function(testRunner) {
  function normalizedName(node) {
    if (node.name().includes("::"))
      return "Detached InternalNode";
    if (node.name().startsWith("Window /"))
      return "Window";
    return node.name();
  }
  var {page, session, dp} = await testRunner.startBlank(
      `Test retaining path for an event listener.`);

  await session.evaluate(`
    function addEventListenerAndRunTest() {
      function myEventListener(e) {
        console.log('myEventListener');
      }
      document.body.addEventListener('click', myEventListener, true);
    }
    addEventListenerAndRunTest();
  `);

  var Helper = await testRunner.loadScript('resources/heap-snapshot-common.js');
  var helper = await Helper(testRunner, session);

  var snapshot = await helper.takeHeapSnapshot();
  var node;
  for (var it = snapshot._allNodes(); it.hasNext(); it.next()) {
    if (it.node.type() === 'closure' && it.node.name() === 'myEventListener') {
      node = it.node;
      break;
    }
  }
  if (node)
    testRunner.log('SUCCESS: found ' + node.name());
  else
    return testRunner.fail('cannot find myEventListener node');

  var retainers = helper.firstRetainingPath(node).map(normalizedName);
  var actual = retainers.join(', ');
  testRunner.log(`SUCCESS: retaining path = [${actual}]`);
  testRunner.completeTest();
})
