(async function(testRunner) {
  function printNodeAndChildren(node, leadingSpace = "") {
    // TODO(crbug.com/1063155): remove this workaround when
    // RuntimeEnabledFeatures::AccessibilityExposeHTMLElementEnabled()
    // is enabled everywhere.
    if (node.role.value == "generic" &&
        node.parent.role.value == "WebArea" &&
        node.children.length == 1 &&
        node.children[0].role.value == "generic") {
      return printNodeAndChildren(node.children[0], leadingSpace);
    }

    let string = leadingSpace;
    if (node.role)
      string += node.role.value;
    else
      string += "<no role>";
    string += (node.name && node.name.value ? ` "${node.name.value}"` : "");
    for (const child of node.children)
      string += "\n" + printNodeAndChildren(child, leadingSpace + "  ");
    return string;
  }

  var {page, session, dp} = await testRunner.startHTML(`
  <main>
    <article>
      <h1>Article</h1>
      <p>First paragraph</p>
    </article>
  </main>
  `, 'Tests Accessibility.getChildAXNodes');
  await dp.Accessibility.enable();
  let {result} = await dp.Accessibility.getFullAXTree({max_depth: 2});
  const nodeMap = new Map();

  for (const node of result.nodes)
    nodeMap.set(node.nodeId, node);
  for (const [nodeId, node] of nodeMap.entries()) {
    node.children = [];
    for (const childId of node.childIds || []) {
      const child = nodeMap.get(childId);
      if (!child)
        continue;
      child.parent = node;
      node.children.push(child);
    }
  }
  const rootNode = Array.from(nodeMap.values()).find(node => !node.parent);
  testRunner.log("\nAfter getFullAXTree:\n" + printNodeAndChildren(rootNode))

  const article = rootNode.children[0].children[0];
  let childResult = await dp.Accessibility.getChildAXNodes({id: article.nodeId});
  result = childResult.result;
  for (const node of result.nodes) {
    nodeMap.set(node.nodeId, node);
    node.children = [];
  }

  for (const childId of article.childIds) {
    const child = nodeMap.get(childId);
    if (!child) {
      testRunner.log("Should have gotten child with id " + childId);
      continue;
    }
    child.parent = article;
    article.children.push(child);
  }

  testRunner.log("\nAfter getChildAXNodes:\n" + printNodeAndChildren(rootNode));
  testRunner.completeTest();
});
