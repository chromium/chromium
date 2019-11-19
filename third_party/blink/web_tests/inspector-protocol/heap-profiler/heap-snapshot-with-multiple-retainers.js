(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test multiple retaining path for an object.`);

  await session.evaluate(`
    function run() {
      function leaking() {
        console.log('leaking');
      }
      var div = document.createElement('div');
      document.body.appendChild(div);
      div.addEventListener('click', leaking, true);
      document.body.addEventListener('click', leaking, true);
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
    return testRunner.fail('cannot find the leaking node');

  let retainerPaths = [];
  for (let it = node.retainers(); it.hasNext(); it.next()) {
    let retainer = it.retainer.node();
    let path = helper.firstRetainingPath(retainer);
    path.unshift(retainer);
    retainerPaths.push(path);
  }
  // Sort alphabetically to make the test robust.
  function toNames(path) {
    let names = [];
    for (node of path) {
      if (node.name().includes('::')) {
        names.push('InternalNode');
      } else if (node.name() == 'Window / file://') {
        // In MacOS 10.10 it's sometimes just Window, so we always make it
        // so to avoid flakiness.
        names.push('Window');
      } else {
        names.push(node.name());
      }
    }
    return names;
  }
  retainerPaths.sort((path1, path2) => {
    let s1 = toNames(path1).join('->');
    let s2 = toNames(path2).join('->');
    return s1 < s2 ? -1 : (s1 == s2) ? 0 : 1;
  });

  let v8EventListenerCount = 0;
  for (let i = 0; i < retainerPaths.length; ++i) {
    let path = retainerPaths[i];
    // Two paths of [V8EventListener, EventListener, ...] are expected.
    testRunner.log(`path${i+1} = [${toNames(path)}]`);
    if (path[0].name() !== 'V8EventListener') {
      continue;
    }
    ++v8EventListenerCount;

    if (path[0].retainersCount() === 1) {
      testRunner.log('SUCCESS: found a single retaining path for V8EventListener.');
    } else {
      return testRunner.fail('cannot find a single retaining path for V8EventListener.');
    }
    if (path[1].name() === 'EventListener') {
      testRunner.log('SUCCESS: V8EventListener has an immediate retainer of EventListener.');
    } else {
      return testRunner.fail('cannot find an EventListener.');
    }
  }
  if (v8EventListenerCount === 2) {
    testRunner.log('SUCCESS: found 2 V8EventListeners as retainers.');
  } else {
    return testRunner.fail('cannot find 2 V8EventListeners as retainers.');
  }

  testRunner.completeTest();
})
