(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that one session receives DOM notifications when the other modifies the DOM.');
  var page = await testRunner.createPage();
  await page.loadHTML(`<div id='test' attr='value'>textContent</div>`);

  async function connect(name) {
    var session = await page.createSession();
    session.nodes = new Map();
    await session.protocol.DOM.enable();

    session.protocol.DOM.onSetChildNodes(event => setNodes(event.params.nodes));

    session.protocol.DOM.onAttributeModified(event => {
      var node = session.nodes.get(event.params.nodeId);
      testRunner.log(`Attribute modified in ${name}: ${node.nodeName}[${event.params.name}=${event.params.value}]`);
    });

    session.protocol.DOM.onAttributeRemoved(event => {
      var node = session.nodes.get(event.params.nodeId);
      testRunner.log(`Attribute removed in ${name}: ${node.nodeName}[${event.params.name}]`);
    });

    session.protocol.DOM.onChildNodeRemoved(event => {
      var node = session.nodes.get(event.params.nodeId);
      testRunner.log(`Node removed in ${name}: ${node.nodeName}`);
    });

    session.protocol.DOM.onChildNodeInserted(event => {
      var node = event.params.node;
      session.nodes.set(node.nodeId, node);
      testRunner.log(`Node inserted in ${name}: ${node.nodeName}`);
    });

    session.doc = (await session.protocol.DOM.getDocument()).result.root;
    session.nodes.set(session.doc.nodeId, session.doc);
    var divId = (await session.protocol.DOM.querySelector({nodeId: session.doc.nodeId, selector: '#test'})).result.nodeId;
    session.div = session.nodes.get(divId);
    return session;

    function setNodes(nodes) {
      for (var node of nodes) {
        session.nodes.set(node.nodeId, node);
        if (node.children)
          setNodes(node.children);
      }
    }
  }

  var session1 = await connect(1);
  var session2 = await connect(2);

  testRunner.log('\nModifying attribute value in 1');
  await session1.protocol.DOM.setAttributeValue({nodeId: session1.div.nodeId, name: 'attr', value: 'newValue'});
  testRunner.log('\nRemoving attribute in 2');
  await session2.protocol.DOM.markUndoableState();
  await session2.protocol.DOM.removeAttribute({nodeId: session2.div.nodeId, name: 'attr'});
  testRunner.log('\nRemoving node in 1');
  await session1.protocol.DOM.markUndoableState();
  await session1.protocol.DOM.removeNode({nodeId: session1.div.nodeId});
  testRunner.log('\nUndoing node removal in 1');
  await session1.protocol.DOM.undo();
  testRunner.log('\nUndoing attribute removal in 2');
  await session2.protocol.DOM.undo();

  testRunner.completeTest();
})
