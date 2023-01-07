(function initialize_DumpAccessibilityNodesTest(testRunner, session) {

  function dumpAccessibilityNodes(nodes) {
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

      if (node.ignored) {
        return node.children.map((child) => printNodeAndChildren(child, leadingSpace)).join("\n");
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

    const nodeMap = new Map();
    for (const node of nodes)
      nodeMap.set(node.nodeId, node);
    for (const [nodeId, node] of nodeMap.entries()) {
      node.children = [];
      for (const childId of node.childIds || []) {
        const child = nodeMap.get(childId);
        child.parent = node;
        node.children.push(child);
      }
    }
    const rootNode = Array.from(nodeMap.values()).find(node => !node.parent);
    testRunner.log("\n" + printNodeAndChildren(rootNode));
    testRunner.completeTest();
  }

return dumpAccessibilityNodes;
})
