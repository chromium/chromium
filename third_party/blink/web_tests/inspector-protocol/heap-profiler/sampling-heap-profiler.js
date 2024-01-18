(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Test sampling heap profiler.`);

  function findNode(root, name, depth) {
    if (depth < 1 && root.callFrame.functionName === name)
      return root;
    return root.children.reduce((found, child) => found || findNode(child, name, depth - 1), null);
  }

  function logNode(node) {
    var size = typeof node.selfSize === 'number' ? node.selfSize ? '>0' : '=0' : '-';
    testRunner.log(`size${size}   ${node.callFrame.functionName}:${node.callFrame.lineNumber}:${node.callFrame.columnNumber}`);
  }

  await dp.HeapProfiler.startSampling();
  testRunner.log('Sampling started');
  await session.evaluate(`









function testMain()
{
    makeDeepCallStack(10, junkGenerator);
}

function makeDeepCallStack(depth, action)
{
    if (depth)
        makeDeepCallStack(depth - 1, action);
    else
        action();
}

function junkGenerator()
{
    var junkArray = new Array(3000);
    window.junkArray = junkArray;
    for (var i = 1; i < junkArray.length; ++i)
        junkArray[i] = new Array(i);
}

testMain();
  `);

  var message = await dp.HeapProfiler.stopSampling();
  testRunner.log('Sampling stopped');
  var profile = message.result.profile;
  var head = profile.head;
  logNode(findNode(head, 'testMain', 1));
  logNode(findNode(head, 'makeDeepCallStack', 11));
  logNode(findNode(head, 'junkGenerator', 12));
  testRunner.completeTest();
})
