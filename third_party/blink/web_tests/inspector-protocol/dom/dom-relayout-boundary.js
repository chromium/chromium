(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <head>
      <style>
      .relayout-boundary {
          width: 200px;
          height: 40px;
          overflow: hidden;
          position: relative;
      }
      </style>
    </head>
    <body>
    <div id='outer'></div>
    <div class='relayout-boundary' id='boundary'>
        <div id='inner'></div>
        <div style='display: none'>
            <div id='hidden'></div>
        </div>
    </div>
    </body>
  `, 'Tests DOM.getRelayoutBoundary method.');

  var DOMHelper = await testRunner.loadScript('../resources/dom-helper.js');
  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);
  var response = await dp.DOM.getDocument();
  nodeTracker.addDocumentNode(response.result.root);
  await dp.DOM.requestChildNodes({nodeId: response.result.root.nodeId, depth: -1});

  var nodeByIdAttribute = {};
  for (var node of nodeTracker.nodes())
    nodeByIdAttribute[DOMHelper.attributes(node).get('id')] = node;

  await dumpRelayoutBoundary(nodeByIdAttribute['outer']);
  await dumpRelayoutBoundary(nodeByIdAttribute['boundary']);
  await dumpRelayoutBoundary(nodeByIdAttribute['inner']);
  await dumpRelayoutBoundary(nodeByIdAttribute['hidden']);
  testRunner.completeTest();

  function nodeLabel(node) {
    var result = node.localName;
    var id = DOMHelper.attributes(node).get('id');
    return result + (id ? '#' + id : '');
  }

  async function dumpRelayoutBoundary(node) {
    var response = await dp.DOM.getRelayoutBoundary({nodeId: node.nodeId});
    var text;
    if (response.error) {
      text = response.error.message;
    } else {
      var boundaryNode = nodeTracker.nodeForId(response.result.nodeId);
      text = boundaryNode ? nodeLabel(boundaryNode) : 'null';
    }
    testRunner.log('Relayout boundary for ' + nodeLabel(node) + ' is: ' + text);
  }
});
