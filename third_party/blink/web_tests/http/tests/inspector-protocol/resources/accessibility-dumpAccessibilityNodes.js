// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function initialize_DumpAccessibilityNodesTest(testRunner, session) {

  function trackGetChildNodesEvents(nodeInfo, callback)
  {
    session.protocol.DOM.onSetChildNodes(setChildNodes);

    function setChildNodes(message)
    {
      var nodes = message.params.nodes;
      for (var i = 0; i < nodes.length; ++i)
        addNode(nodeInfo, nodes[i]);
      if (callback)
        callback();
    }
  }

  function addNode(nodeInfo, node)
  {
    nodeInfo[node.nodeId] = node;
    delete node.nodeId;
    var children = node.children || [];
    for (var i = 0; i < children.length; ++i)
      addNode(nodeInfo, children[i]);
    var shadowRoots = node.shadowRoots || [];
    for (var i = 0; i < shadowRoots.length; ++i)
      addNode(nodeInfo, shadowRoots[i]);
  }

  async function requestDocumentNodeId(callback)
  {
    var result = (await session.protocol.DOM.getDocument()).result;
    if (callback)
      callback(result.root.nodeId);
    return result.root.nodeId;
  };

  async function requestNodeId(documentNodeId, selector, callback)
  {
    var result = (await session.protocol.DOM.querySelector({"nodeId": documentNodeId , "selector": selector})).result;
    if (callback)
      callback(result.nodeId);
    return result.nodeId;
  };

var nodeInfo = {};
trackGetChildNodesEvents(nodeInfo);

function dumpAccessibilityNodesBySelectorAndCompleteTest(selector, fetchRelatives, msg) {
    if (msg.error) {
        testRunner.log(msg.error.message);
        testRunner.completeTest();
        return;
    }

    var rootNode = msg.result.root;
    var rootNodeId = rootNode.nodeId;
    addNode(nodeInfo, rootNode);

    sendQuerySelectorAll(rootNodeId, selector)
        .then((msg) => { return getAXNodes(msg, fetchRelatives || false) } )
        .then(() => { done(); })
        .then(() => {
            testRunner.completeTest();
        })
        .catch((msg) => { testRunner.log("Error: " + JSON.stringify(msg)); })
}

function done()
{
    session.protocol.Runtime.evaluate({expression: "done();"});
}

function sendQuerySelectorAll(nodeId, selector)
{
    return session.protocol.DOM.querySelectorAll({"nodeId": nodeId, "selector": selector });
}

function getAXNodes(msg, fetchRelatives)
{
    var promise = Promise.resolve();
    if (!msg.result || !msg.result.nodeIds) {
        testRunner.log("Unexpected result: " + JSON.stringify(msg));
        testRunner.completeTest();
    }
    msg.result.nodeIds.forEach((id) => {
        if (fetchRelatives) {
            promise = promise.then(() => {
                return session.protocol.Accessibility.getPartialAXTree({ "nodeId": id, "fetchRelatives": true });
            });
            promise = promise.then((msg) => { return rewriteRelatedNodes(msg, id); })
                             .then((msg) => { return dumpTreeStructure(msg); });

        }
        promise = promise.then(() => { return session.protocol.Accessibility.getPartialAXTree({ "nodeId": id, "fetchRelatives": false }); })
                         .then((msg) => { return rewriteRelatedNodes(msg, id); })
                         .then((msg) => { return dumpNode(msg); });

    });
    return promise;
}

function describeDomNode(nodeData)
{
    var description = nodeData.nodeName.toLowerCase();
    switch (nodeData.nodeType) {
    case Node.ELEMENT_NODE:
        var p = nodeData.attributes.indexOf("id");
        if (p >= 0)
            description += "#" + nodeData.attributes[p + 1];
    }
    return description;
}

function rewriteBackendDomNodeId(axNode, selectedNodeId, promises)
{
    if (!("backendDOMNodeId" in axNode))
        return;

    function rewriteBackendDomNodeIdPromise(resolve, reject)
    {
        if (!("backendDOMNodeId" in axNode)) {
            resolve();
            return;
        }
        var backendDOMNodeId = axNode.backendDOMNodeId;

        function onDomNodeResolved(backendDOMNodeId, message)
        {
            if (!message.result || !message.result.nodeIds) {
                testRunner.log("Unexpected result for pushNodesByBackendIdsToFrontend: " + JSON.stringify(message));
                testRunner.completeTest();
                return;
            }
            var nodeId = message.result.nodeIds[0];
            if (!(nodeId in nodeInfo)) {
                axNode.domNode = "[NODE NOT FOUND]";
                resolve();
                return;
            }
            var domNode = nodeInfo[nodeId];
            delete axNode.backendDOMNodeId;
            axNode.domNode = describeDomNode(domNode);
            if (nodeId === selectedNodeId)
              axNode.selected = true;
            resolve();
        }

        var params = { "backendNodeIds": [ backendDOMNodeId ] };
        session.protocol.DOM.pushNodesByBackendIdsToFrontend(params).then(onDomNodeResolved.bind(null, backendDOMNodeId));
    }
    promises.push(new Promise(rewriteBackendDomNodeIdPromise));
}

function rewriteRelatedNode(relatedNode)
{
    function rewriteRelatedNodePromise(resolve, reject)
    {
        if (!("backendDOMNodeId" in relatedNode)) {
            reject("Could not find backendDOMNodeId in " + JSON.stringify(relatedNode));
            return;
        }
        var backendDOMNodeId = relatedNode.backendDOMNodeId;

        function onNodeResolved(backendDOMNodeId, message)
        {
            if (!message.result || !message.result.nodeIds) {
                testRunner.log("Unexpected result for pushNodesByBackendIdsToFrontend: " + JSON.stringify(message));
                testRunner.completeTest();
                return;
            }
            var nodeId = message.result.nodeIds[0];
            if (!(nodeId in nodeInfo)) {
                relatedNode.nodeResult = "[NODE NOT FOUND]";
                resolve();
                return;
            }
            var domNode = nodeInfo[nodeId];
            delete relatedNode.backendDOMNodeId;
            relatedNode.nodeResult = describeDomNode(domNode);
            resolve();
        }
        var params = { "backendNodeIds": [ backendDOMNodeId ] };
        session.protocol.DOM.pushNodesByBackendIdsToFrontend(params).then(onNodeResolved.bind(null, backendDOMNodeId));

    }
    return new Promise(rewriteRelatedNodePromise);
}

function checkExists(path, obj)
{
    var pathComponents = path.split(".");
    var currentPath = [];
    var currentObject = obj;
    for (var component of pathComponents) {
        var isArray = false;
        var index = -1;
        var matches = component.match(/(\w+)\[(\d+)\]/);
        if (matches) {
            isArray = true;
            component = matches[1];
            index = Number.parseInt(matches[2], 10);
        }
        currentPath.push(component);
        if (!(component in currentObject)) {
            testRunner.log("Could not find " + currentPath.join(".") + " in " + JSON.stringify(obj, null, "  "));
            testRunner.completeTest();
        }
        if (isArray)
            currentObject = currentObject[component][index];
        else
            currentObject = currentObject[component];
    }
    return true;
}

function check(condition, errorMsg, obj)
{
    if (condition)
        return true;
    throw new Error(errorMsg + " in " + JSON.stringify(obj, null, "  "));
}

function rewriteRelatedNodeValue(value, promises)
{
    checkExists("relatedNodes", value);
    var relatedNodeArray = value.relatedNodes;
    check(Array.isArray(relatedNodeArray), "relatedNodes should be an array", JSON.stringify(value));
    for (var relatedNode of relatedNodeArray) {
        promises.push(rewriteRelatedNode(relatedNode));
    }
}

function rewriteRelatedNodes(msg, nodeId)
{
    if (msg.error) {
        throw new Error(msg.error.message);
    }

    var promises = [];
    for (var node of msg.result.nodes) {
        if (node.ignored) {
            checkExists("ignoredReasons", node);
            var properties = node.ignoredReasons;
        } else {
            checkExists("properties", node);
            var properties = node.properties;
        }
        if (node.name && node.name.sources) {
            for (var source of node.name.sources) {
            var value;
                if (source.value)
                    value = source.value;
                if (source.attributeValue)
                    value = source.attributeValue;
                if (!value)
                    continue;
                if (value.type === "idrefList" ||
                    value.type === "idref" ||
                    value.type === "nodeList")
                    rewriteRelatedNodeValue(value, promises);
            }
        }
        for (var property of properties) {
            if (property.value.type === "idrefList" ||
                property.value.type === "idref" ||
                property.value.type === "nodeList")
                rewriteRelatedNodeValue(property.value, promises);
        }
        rewriteBackendDomNodeId(node, nodeId, promises);
    }
    return Promise.all(promises).then(() => { return msg; });
}

function dumpNode(msg)
{
    if (!msg.result || !msg.result.nodes || msg.result.nodes.length !== 1) {
        testRunner.log("Expected exactly one node in " + JSON.stringify(msg, null, "  "));
        return;
    }
    delete msg.result.nodes[0]['selected'];
    testRunner.log(msg.result.nodes[0], null, ["id", "backendDOMNodeId", "nodeId", "parentId", "childIds"]);
}

function dumpTreeStructure(msg)
{
    function printNodeAndChildren(node, leadingSpace)
    {
        leadingSpace = leadingSpace || "";
        var string = leadingSpace;
        if (node.selected)
            string += "*";
        if (node.role)
            string += node.role.value;
        else
            string += "<no role>";
        string += (node.name && node.name.value !== "" ? " \"" + node.name.value + "\"" : "");
        if (node.children) {
            for (var child of node.children)
                string += "\n" + printNodeAndChildren(child, leadingSpace + "  ");
        }
        return string;
    }

    var nodeMap = {};
    if ("result" in msg && "nodes" in msg.result) {
        for (var node of msg.result.nodes)
            nodeMap[node.nodeId] = node;
    }
    for (var nodeId in nodeMap) {
        var node = nodeMap[nodeId];
        if (node.childIds) {
            node.children = [];
            for (var i = 0; i < node.childIds.length && node.childIds.length > 0;) {
                var childId = node.childIds[i];
                if (childId in nodeMap) {
                    var child = nodeMap[childId];
                    child.parentId = nodeId;
                    node.children.push(child);

                    node.childIds.splice(i, 1);
                } else {
                    node.childIds[i] = "<string>";
                    i++;
                }
            }
            if (!node.childIds.length)
                delete node.childIds;
            if (!node.children.length)
                delete node.children;
        }
    }
    var rootNode = Object.values(nodeMap).find((node) => !("parentId" in node));
    for (var node of Object.values(nodeMap))
        delete node.parentId;

    testRunner.log("\n" + printNodeAndChildren(rootNode));
}

return dumpAccessibilityNodesBySelectorAndCompleteTest;

})
