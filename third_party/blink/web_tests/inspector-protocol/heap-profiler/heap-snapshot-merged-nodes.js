(async function(testRunner) {
  function normalizedName(node) {
    if (node.name().includes("::"))
      return "Detached InternalNode";
    if (node.name().startsWith("Window /"))
      return "Window";
    return node.name();
  }
  var {page, session, dp} = await testRunner.startBlank(
      `Test that DOM node and its JS wrapper appear as a single node.`);

  await session.evaluate(`
    var retainer = null;
    function run() {
      function leaking() {
        console.log('leaking');
      }
      var div = document.createElement('div');
      div.addEventListener('click', leaking, true);
      retainer = div;
    }
    run();
  `);

  var Helper = await testRunner.loadScript('resources/heap-snapshot-common.js');
  var helper = await Helper(testRunner, session);

  var snapshot = await helper.takeHeapSnapshot();
  var node;
  for (var it = snapshot._allNodes(); it.hasNext(); it.next()) {
    if (it.node.type() === 'closure' && it.node.name() === 'leaking') {
      node = it.node;
      break;
    }
  }
  if (node)
    testRunner.log('SUCCESS: found ' + node.name());
  else
    return testRunner.fail('cannot find leaking node');

  var retainers = helper.firstRetainingPath(node).map(normalizedName);
  var actual = retainers.join(', ');
  testRunner.log(`SUCCESS: retaining path = [${actual}]`);
  testRunner.completeTest();
})
