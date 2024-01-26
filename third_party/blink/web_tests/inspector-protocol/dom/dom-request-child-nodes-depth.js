(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id="depth-1">
        <div id="depth-2">
            <div id="depth-3">
                <div id="depth-4">
                    <div id="depth-5">
                        <div id="depth-6">
                            <div id="depth-7">
                                <div id="depth-8">
                                    <div id="depth-9">
                                        <div id="depth-10">
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
  `, 'Tests that DOM.requestChildNodes respects depth parameter.');
  testRunner.log("\n=== Get the Document ===\n");
  var response = await dp.DOM.getDocument();
  var bodyId = response.result.root.children[0].children[1].nodeId;

  testRunner.log("\n=== Get immediate children of the body ===\n");
  dp.DOM.requestChildNodes({nodeId: bodyId});
  var message = await dp.DOM.onceSetChildNodes();
  var firstDiv = message.params.nodes[0];
  assert("First child is a div", firstDiv.localName, "div");
  assert("First child is div#depth-1", firstDiv.attributes[1], "depth-1");
  assert("First child has one child", firstDiv.childNodeCount, 1);
  assert("First child has no .children property", firstDiv.children, undefined);

  testRunner.log("\n=== Get children of div#depth-1 three levels deep ===\n");
  dp.DOM.requestChildNodes({nodeId: firstDiv.nodeId, depth: 3});
  var message = await dp.DOM.onceSetChildNodes();
  var depth = 1;
  var firstChild = message.params.nodes[0];
  var node = firstChild;
  while (node && node.children) {
    depth++;
    node = node.children[0];
  }
  assert("div#depth-1 has nodes 3 levels deep", depth, 3);

  testRunner.log("\n=== Get all children of body ===\n");
  dp.DOM.requestChildNodes({nodeId: firstDiv.nodeId, depth: -1});
  var message = await dp.DOM.onceSetChildNodes();
  var depth = 0;
  var firstChild = message.params.nodes[0];
  var node = firstChild;
  while (node && node.children) {
    depth++;
    node = node.children[0];
  }
  // We have requested nodes 3-level deep so far, so
  // we should have gotten an additional 6 levels of depth.
  assert("div#depth-1 has nodes 9 levels deep", depth, 6);

  testRunner.log("\n=== Pass an invalid depth ===\n");
  var response = await dp.DOM.requestChildNodes({nodeId: firstDiv.nodeId, depth: 0});
  if (response.error)
    testRunner.log("Backend error: " + response.error.message + " (" + response.error.code + ")\n");
  testRunner.completeTest();

  function assert(message, actual, expected) {
    if (actual === expected) {
      testRunner.log("PASS: " + message);
    } else {
      testRunner.log("FAIL: " + message + ", expected \"" + expected + "\" but got \"" + actual + "\"");
      testRunner.completeTest();
    }
  };
});

