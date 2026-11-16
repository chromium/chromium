// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <div id="outer"><span slot="a">Projected</span></div>
      <div id="request-host"><span slot="req">Requested</span></div>
      <div id="describe-host"><span slot="desc">Described</span></div>
      <script>
        const outer = document.getElementById('outer');
        const root = outer.attachShadow({mode: 'open'});
        const existingSlot = document.createElement('slot');
        existingSlot.name = 'a';
        root.append(existingSlot);

        const reqRoot =
            document.getElementById('request-host').attachShadow({mode: 'open'});
        const reqSlot = document.createElement('slot');
        reqSlot.name = 'req';
        reqRoot.append(reqSlot);

        const descRoot =
            document.getElementById('describe-host').attachShadow({mode: 'open'});
        const descSlot = document.createElement('slot');
        descSlot.name = 'desc';
        descRoot.append(descSlot);
      </script>
      `,
      'Tests that inserting a nested shadow host or a slot into an expanded shadow root with dirty slot assignment does not crash, while protocol commands return up-to-date distributedNodes.');

  function getAttribute(node, attrName) {
    const attrs = node.attributes || [];
    for (let i = 0; i + 1 < attrs.length; i += 2) {
      if (attrs[i] === attrName) {
        return attrs[i + 1];
      }
    }
    return undefined;
  }

  function findSlots(node, out = []) {
    if (node.localName === 'slot') {
      out.push(node);
    }
    for (const child of node.children || []) {
      findSlots(child, out);
    }
    for (const sr of node.shadowRoots || []) {
      findSlots(sr, out);
    }
    return out;
  }

  await dp.DOM.enable();
  await dp.Runtime.enable();

  // Populate document bindings without piercing into shadow roots so
  // #request-host and #describe-host shadow root children are not yet
  // requested.
  const shallowDoc = await dp.DOM.getDocument({depth: 3});
  const body = shallowDoc.result.root.children.find(n => n.localName === 'html')
                   .children.find(n => n.localName === 'body');
  const outerNode = body.children.find(n => getAttribute(n, 'id') === 'outer');
  const reqHostNode =
      body.children.find(n => getAttribute(n, 'id') === 'request-host');
  const descHostNode =
      body.children.find(n => getAttribute(n, 'id') === 'describe-host');

  // Expand only #outer's shadow root so its mutations emit childNodeInserted.
  await dp.DOM.requestChildNodes(
      {nodeId: outerNode.shadowRoots[0].nodeId, depth: -1, pierce: true});

  const insertedEvents = [];
  let slotBNodeId = null;
  dp.DOM.onChildNodeInserted(({params}) => {
    insertedEvents.push(params.node);
    if (params.node.localName === 'slot' &&
        getAttribute(params.node, 'name') === 'b') {
      slotBNodeId = params.node.nodeId;
    }
  });

  // Case 1: Append a nested shadow host with a light-DOM <slot> into #outer's
  // expanded shadow root while its SlotAssignment is dirty.
  // BuildObjectForNode(inner, 0) sets force_push_children because inner has a
  // ShadowRoot, recursing to depth 1 and inspecting the light-DOM <slot>.
  await session.evaluate(() => {
    const outer = document.getElementById('outer');
    const root = outer.shadowRoot;
    outer.firstElementChild.setAttribute('slot', 'b');
    const inner = document.createElement('div');
    inner.attachShadow({mode: 'open'});
    inner.append(document.createElement('slot'));
    root.append(inner);
  });

  const insertedInner = insertedEvents.find(n => n.localName === 'div');
  const innerSlot =
      (insertedInner.children || []).find(n => n.localName === 'slot');
  testRunner.log('Case 1 (nested shadow host): inserted children count=' +
                 (insertedInner.children || []).length +
                 ', inner slot distributedNodes=' +
                 (innerSlot && innerSlot.distributedNodes ?
                      innerSlot.distributedNodes.length :
                      -1));

  // Case 2: Append a <slot name="b"> directly into #outer's expanded shadow
  // root while its SlotAssignment is dirty, then verify distributedNodesUpdated
  // delivers the recalculated distribution after the lifecycle update.
  const slotBUpdatePromise = dp.DOM.onceDistributedNodesUpdated(
      ({params}) => slotBNodeId !== null &&
          params.insertionPointId === slotBNodeId &&
          params.distributedNodes.length > 0);
  await session.evaluate(() => {
    const outer = document.getElementById('outer');
    const root = outer.shadowRoot;
    outer.firstElementChild.setAttribute('slot', 'c');
    outer.firstElementChild.setAttribute('slot', 'b');
    const slotB = document.createElement('slot');
    slotB.name = 'b';
    root.append(slotB);
  });

  const insertedSlotB = insertedEvents.find(
      n => n.localName === 'slot' && getAttribute(n, 'name') === 'b');
  testRunner.log('Case 2 (direct slot insertion): initial distributedNodes=' +
                 insertedSlotB.distributedNodes.length);

  const slotBUpdate = await slotBUpdatePromise;
  testRunner.log(
      'Case 2 (after lifecycle update): distributedNodesUpdated count=' +
      slotBUpdate.params.distributedNodes.length);

  // Case 3: requestChildNodes on a shadow root whose slot assignment was
  // dirtied before children were requested.
  await session.evaluate(() => {
    const reqHost = document.getElementById('request-host');
    const span = document.createElement('span');
    span.slot = 'req';
    reqHost.append(span);
  });
  const setChildNodesPromise = dp.DOM.onceSetChildNodes(
      e => e.params.parentId === reqHostNode.shadowRoots[0].nodeId);
  await dp.DOM.requestChildNodes(
      {nodeId: reqHostNode.shadowRoots[0].nodeId, depth: -1, pierce: true});
  const setChildNodesEvent = await setChildNodesPromise;
  const reqSlotNode = setChildNodesEvent.params.nodes.find(
      n => n.localName === 'slot' && getAttribute(n, 'name') === 'req');
  testRunner.log(
      'Case 3 (requestChildNodes): slot[name=req] distributedNodes=' +
      reqSlotNode.distributedNodes.length);

  // Case 4: describeNode on a connected host with dirty slot assignment.
  await session.evaluate(() => {
    const descHost = document.getElementById('describe-host');
    const span = document.createElement('span');
    span.slot = 'desc';
    descHost.append(span);
  });
  const describedConnected = await dp.DOM.describeNode(
      {nodeId: descHostNode.nodeId, depth: -1, pierce: true});
  const descSlotNode = findSlots(describedConnected.result.node)
                           .find(s => getAttribute(s, 'name') === 'desc');
  testRunner.log(
      'Case 4 (describeNode connected): slot[name=desc] distributedNodes=' +
      descSlotNode.distributedNodes.length);

  // Case 5: describeNode on a detached host with a shadow root and slotted
  // child.
  const detachedEval = await dp.Runtime.evaluate({
    expression: `(() => {
      const host = document.createElement('div');
      const sr = host.attachShadow({mode: 'open'});
      const slot = document.createElement('slot');
      slot.name = 'det';
      sr.append(slot);
      const child = document.createElement('span');
      child.slot = 'det';
      host.append(child);
      return host;
    })()`
  });
  const describedDetached = await dp.DOM.describeNode({
    objectId: detachedEval.result.result.objectId,
    depth: -1,
    pierce: true,
  });
  const detachedSlotNode = findSlots(describedDetached.result.node)
                               .find(s => getAttribute(s, 'name') === 'det');
  testRunner.log(
      'Case 5 (describeNode detached): slot[name=det] distributedNodes=' +
      detachedSlotNode.distributedNodes.length);

  testRunner.completeTest();
})
