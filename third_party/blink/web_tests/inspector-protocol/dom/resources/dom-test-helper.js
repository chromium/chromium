(class DomTestHelper {
  /**
   * @param {Array<!Node>} nodes
   * @param {string} attrValue
   * @return {?Node}
   */
  static searchNodesByAttributeValue(nodes, attrValue) {
    for (let node of nodes) {
      if (node.attributes && (node.attributes.indexOf(attrValue) !== -1))
        return node;

      if (node.children) {
        const childSearch = DomTestHelper.searchNodesByAttributeValue(node.children, attrValue);
        if (childSearch)
          return childSearch;
      }
    }

    return null;
  }

  /**
   * Queries DevTools protocol for document to find body node
   *
   * @return {?Node}
   */
  static async findBodyNode(dp) {
    /** Query for depth = 3 in conventional files (specifically our resource files)
     * #document
     *  - #doctype
     *  - html
     *   - head
     *   - body
     */
    const documentMessage = await dp.DOM.getDocument({ depth: 2 });
    /** Do a breadth-first traversal to find a node where nodeName === 'BODY' */
    const toVisit = [documentMessage.result.root];
    while (toVisit.length > 0) {
      const toCheck = toVisit.shift();
      if (toCheck.nodeName === 'BODY')
        return toCheck;

      if (toCheck.children)
        toVisit.push(...toCheck.children);

    }

    return null;
  }

  /**
   * Searches an array of nodes and returns the first which has a
   * matching id.
   *
   * @param {Array<!Node>} nodes
   * @param {number} nodeId
   * @return {?Node}
   */
  static searchNodesByNodeId(nodes, nodeId) {
    return nodes.find(node => node.nodeId === nodeId) || null;
  }

  /**
   * Performs a depth first search of an array of nodes and their
   * children for the first iframe contentDocument
   *
   * @param {Array<!Node>} nodes
   * @return {?Node}
   */
  static searchNodesForContentDocument(nodes) {
    for (let node of nodes) {
      if (node.contentDocument)
        return node.contentDocument;

      if (node.children) {
        const childSearch = DomTestHelper.searchNodesForContentDocument(node.children);
        if (childSearch)
          return childSearch;
      }
    }

    return null;
  }
})
