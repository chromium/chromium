// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(class SetOuterHTMLTest {
  constructor(testRunner, session, dp) {
    this.testRunner = testRunner;
    this.session = session;
    this.dp = dp;
    this.events = [];
    this.rootNodeId = 0;
    this.containerId = 0;
    this.containerText = '';
    this.nodesById = new Map();
    this._pendingRequests = [];
  }

  _indexNode(node) {
    this.nodesById.set(node.nodeId, node);
    if (node.children) {
      for (const child of node.children) {
        this._indexNode(child);
      }
    }
  }

  async setUp() {
    await this.dp.DOM.enable();
    const {result: {root}} = await this.dp.DOM.getDocument({depth: -1});
    this.rootNodeId = root.nodeId;
    this._indexNode(root);
    this.containerId = await this.querySelector('#container');
    this.containerText = (await this.dp.DOM.getOuterHTML({
                           nodeId: this.containerId
                         })).result.outerHTML;

    this.dp.DOM.onSetChildNodes(({params: {nodes}}) => {
      for (const node of nodes) {
        this._indexNode(node);
      }
    });
    this.dp.DOM.onChildNodeInserted(({params: {node}}) => {
      this._indexNode(node);
      this.events.push('Event NodeInserted: ' + node.nodeName);
      this._pendingRequests.push(
          this.dp.DOM.requestChildNodes({nodeId: node.nodeId, depth: -1}));
    });
    this.dp.DOM.onChildNodeRemoved(({params: {nodeId}}) => {
      const node = this.nodesById.get(nodeId);
      this.events.push('Event NodeRemoved: ' + (node ? node.nodeName : ''));
    });
    this.dp.DOM.onAttributeModified(({params: {nodeId}}) => {
      const node = this.nodesById.get(nodeId);
      this.events.push('Event AttrModified: ' + (node ? node.nodeName : ''));
    });
    this.dp.DOM.onAttributeRemoved(({params: {nodeId}}) => {
      const node = this.nodesById.get(nodeId);
      this.events.push('Event AttrRemoved: ' + (node ? node.nodeName : ''));
    });
    this.dp.DOM.onCharacterDataModified(({params: {nodeId}}) => {
      const node = this.nodesById.get(nodeId);
      this.events.push('Event CharacterDataModified: ' +
                       (node ? node.nodeName : ''));
    });
  }

  async querySelector(selector) {
    const {result: {nodeId}} = await this.dp.DOM.querySelector({
      nodeId: this.rootNodeId,
      selector,
    });
    return nodeId;
  }

  async patchOuterHTML(pattern, replacement) {
    this.testRunner.log('Replacing \'' + pattern + '\' with \'' + replacement +
                        '\'\n');
    await this.setOuterHTML(this.containerText.replace(pattern, replacement));
  }

  async patchOuterHTMLUseUndo(pattern, replacement) {
    this.testRunner.log('Replacing \'' + pattern + '\' with \'' + replacement +
                        '\'\n');
    await this.setOuterHTMLUseUndo(
        this.containerText.replace(pattern, replacement));
  }

  async setOuterHTML(newText) {
    await this.innerSetOuterHTML(newText, false);
    this.testRunner.log('\nBringing things back\n');
    await this.innerSetOuterHTML(this.containerText, true);
  }

  async setOuterHTMLUseUndo(newText) {
    await this.innerSetOuterHTML(newText, false);
    this.testRunner.log('\nBringing things back\n');
    await this.dp.DOM.undo();
    await this._dumpOuterHTML(true);
  }

  async innerSetOuterHTML(newText, last) {
    await this.dp.DOM.setOuterHTML(
        {nodeId: this.containerId, outerHTML: newText});
    await this.dp.DOM.markUndoableState();
    await this._dumpOuterHTML(last);
  }

  async _dumpOuterHTML(last) {
    await Promise.all(this._pendingRequests.splice(0));
    const identity = await this.session.evaluate(
        () => document.getElementById('identity').wrapperIdentity);
    this.testRunner.log('Wrapper identity: ' + identity);
    this.events.sort();
    for (let i = 0; i < this.events.length; ++i) {
      this.testRunner.log(this.events[i]);
    }
    this.events = [];
    const text = (await this.dp.DOM.getOuterHTML({
                   nodeId: this.containerId
                 })).result.outerHTML;
    this.testRunner.log('==========8<==========');
    this.testRunner.log(text);
    this.testRunner.log('==========>8==========');
    if (last) {
      this.testRunner.log('\n\n\n');
    }
  }
})
