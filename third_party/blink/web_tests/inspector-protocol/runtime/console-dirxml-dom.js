// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests console.dirxml and console.log with DOM nodes, document fragments, and node arrays.');

  await dp.DOM.enable();
  await dp.DOM.getDocument();
  await dp.Runtime.enable();

  const consoleEvents = [];
  const allLogged = new Promise(resolve => {
    dp.Runtime.onConsoleAPICalled(event => {
      consoleEvents.push(event.params);
      if (consoleEvents.length === 5) {
        resolve();
      }
    });
  });

  await session.evaluate(`
    function logToConsole() {
      const fragment = document.createDocumentFragment();
      fragment.appendChild(document.createElement('p'));

      console.dirxml(document);
      console.dirxml(fragment);
      console.dirxml(fragment.firstChild);
      console.log([fragment.firstChild]);
      console.dirxml([document, fragment, document.createElement('span')]);
    }
    logToConsole();
  `);

  await allLogged;

  for (const params of consoleEvents) {
    const arg = params.args[0];
    const topFrame = params.stackTrace?.callFrames?.[0];
    const logEntry = {
      type: params.type,
      caller: topFrame ? {
        functionName: topFrame.functionName,
        lineNumber: topFrame.lineNumber,
      } :
                         null,
      arg: {
        type: arg.type,
        subtype: arg.subtype,
        className: arg.className,
        description: arg.description,
        hasObjectId: Boolean(arg.objectId),
      },
    };
    if (arg.subtype === 'array') {
      logEntry.arg.preview = arg.preview;
    }
    testRunner.log(logEntry);

    if (arg.subtype === 'node' && arg.objectId) {
      const requestNodeResponse =
          await dp.DOM.requestNode({objectId: arg.objectId});
      if (requestNodeResponse.error) {
        testRunner.log({requestNodeError: requestNodeResponse.error});
        continue;
      }
      const {nodeId} = requestNodeResponse.result;
      const describeNodeResponse =
          await dp.DOM.describeNode({nodeId, depth: 1});
      if (describeNodeResponse.error) {
        testRunner.log({describeNodeError: describeNodeResponse.error});
        continue;
      }
      const {node} = describeNodeResponse.result;
      testRunner.log({
        resolvedNode: {
          hasNodeId: nodeId > 0,
          nodeType: node.nodeType,
          nodeName: node.nodeName,
          localName: node.localName,
          hasInspectorProtocolPageUrl: Boolean(node.documentURL?.endsWith(
              '/inspector-protocol/resources/inspector-protocol-page.html')),
          childNodeCount: node.childNodeCount,
          children: (node.children || []).map(child => ({
                                                nodeType: child.nodeType,
                                                nodeName: child.nodeName,
                                                localName: child.localName,
                                              })),
        },
      });
    }
  }

  testRunner.completeTest();
})
