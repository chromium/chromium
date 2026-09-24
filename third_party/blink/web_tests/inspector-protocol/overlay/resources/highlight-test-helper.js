// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(class OverlayHelper {
  constructor(testRunner, dp, session) {
    this._testRunner = testRunner;
    this._dp = dp;
    this._session = session;
  }

  async init() {
    await this._dp.DOM.enable();
    await this._dp.Overlay.enable();
    await this._dp.Runtime.enable();
    await this._session.evaluateAsync(async () => {
      if (document.readyState !== 'complete') {
        await new Promise(r =>
                              window.addEventListener('load', r, {once: true}));
      }
      for (const iframe of document.querySelectorAll('iframe')) {
        try {
          if (iframe.src &&
              (!iframe.contentDocument ||
               iframe.contentDocument.readyState !== 'complete')) {
            await new Promise(
                r => iframe.addEventListener('load', r, {once: true}));
          }
        } catch (e) {
        }
      }
      if (document.fonts && document.fonts.ready) {
        await document.fonts.ready;
      }
      await new Promise(
          r => requestAnimationFrame(() => requestAnimationFrame(r)));
    });
    const {result: {root}} =
        await this._dp.DOM.getDocument({depth: -1, pierce: true});
    this._wrappedRoot = this._wrapTree(root, null);
  }

  _wrapTree(raw, parent = null) {
    const attrs = new Map();
    if (Array.isArray(raw.attributes)) {
      for (let i = 0; i < raw.attributes.length; i += 2) {
        attrs.set(raw.attributes[i], raw.attributes[i + 1]);
      }
    }
    const wrapped = {
      id: raw.nodeId,
      backendNodeId: raw.backendNodeId,
      parentNode: parent,
      _children: [],
      nodeType() {
        return raw.nodeType;
      },
      nodeName() {
        return raw.nodeName;
      },
      nodeValue() {
        return raw.nodeValue || '';
      },
      children() {
        return this._children;
      },
      getAttribute(name) {
        return attrs.get(name);
      },
    };
    const childList = [];
    if (Array.isArray(raw.children)) {
      for (const c of raw.children) {
        childList.push(this._wrapTree(c, wrapped));
      }
    }
    if (Array.isArray(raw.shadowRoots)) {
      for (const sr of raw.shadowRoots) {
        childList.push(this._wrapTree(sr, wrapped));
      }
    }
    if (raw.contentDocument) {
      childList.push(this._wrapTree(raw.contentDocument, wrapped));
    }
    if (raw.templateContent) {
      childList.push(this._wrapTree(raw.templateContent, wrapped));
    }
    wrapped._children = childList;
    return wrapped;
  }

  async findNode(predicate) {
    if (!this._wrappedRoot) {
      const {result: {root}} =
          await this._dp.DOM.getDocument({depth: -1, pierce: true});
      this._wrappedRoot = this._wrapTree(root, null);
    }
    function search(node) {
      if (predicate(node)) {
        return node;
      }
      for (const c of node.children()) {
        const found = search(c);
        if (found) {
          return found;
        }
      }
      return null;
    }
    let match = search(this._wrappedRoot);
    if (!match) {
      const {result: {root}} =
          await this._dp.DOM.getDocument({depth: -1, pierce: true});
      this._wrappedRoot = this._wrapTree(root, null);
      match = search(this._wrappedRoot);
    }
    if (match && !match.id && match.backendNodeId) {
      const {result: {nodeIds}} =
          await this._dp.DOM.pushNodesByBackendIdsToFrontend({
            backendNodeIds: [match.backendNodeId],
          });
      match.id = nodeIds[0];
    }
    return match;
  }

  async nodeWithId(idValue) {
    return await this.findNode(node => node.getAttribute('id') === idValue);
  }

  _sortHighlightObject(obj) {
    if (Array.isArray(obj)) {
      return obj.map(item => this._sortHighlightObject(item));
    }
    if (obj !== null && typeof obj === 'object') {
      const sortedObj = {};
      for (const key of Object.keys(obj)) {
        if (key === 'areaNames' && obj[key] !== null &&
            typeof obj[key] === 'object') {
          const sortedAreas = {};
          for (const areaName of Object.keys(obj[key]).sort()) {
            sortedAreas[areaName] = obj[key][areaName];
          }
          sortedObj[key] = sortedAreas;
        } else if ((key === 'rowLineNameOffsets' ||
                    key === 'columnLineNameOffsets') &&
                   Array.isArray(obj[key])) {
          const sortedLines =
              obj[key].map(item => this._sortHighlightObject(item));
          sortedLines.sort((a, b) => {
            if (a.name !== b.name) {
              return a.name.localeCompare(b.name);
            }
            if (a.x !== b.x) {
              return a.x - b.x;
            }
            return a.y - b.y;
          });
          sortedObj[key] = sortedLines;
        } else {
          sortedObj[key] = this._sortHighlightObject(obj[key]);
        }
      }
      return sortedObj;
    }
    return obj;
  }

  async dumpHighlight(idValue, attributes) {
    const attributeSet =
        Array.isArray(attributes) ? new Set(attributes) : new Set();
    const node = await this.nodeWithId(idValue);
    const {result: {highlight}} =
        await this._dp.Overlay.getHighlightObjectForTest({nodeId: node.id});
    const view = attributeSet.size ? {} : highlight;
    for (const key of Object.keys(highlight).filter(k => attributeSet.has(k))) {
      view[key] = highlight[key];
    }
    this._testRunner.log(idValue + JSON.stringify(view, null, 2));
  }

  async dumpStableHighlight(idValue) {
    const node = await this.nodeWithId(idValue);
    const {result: {highlight}} =
        await this._dp.Overlay.getHighlightObjectForTest({nodeId: node.id});
    const sorted = this._sortHighlightObject(highlight);
    this._testRunner.log(idValue + JSON.stringify(sorted, null, 2));
  }

  async dumpGridHighlights(idValues) {
    const nodeIds = [];
    for (const id of idValues) {
      const node = await this.nodeWithId(id);
      nodeIds.push(node.id);
    }
    const {result: {highlights}} =
        await this._dp.Overlay.getGridHighlightObjectsForTest({nodeIds});
    this._testRunner.log(JSON.stringify(highlights, null, 2));
  }

  async dumpStableGridHighlights(idValues) {
    const nodeIds = [];
    for (const id of idValues) {
      const node = await this.nodeWithId(id);
      nodeIds.push(node.id);
    }
    const {result: {highlights}} =
        await this._dp.Overlay.getGridHighlightObjectsForTest({nodeIds});
    const sorted = this._sortHighlightObject(highlights);
    this._testRunner.log(JSON.stringify(sorted, null, 2));
  }

  async dumpHighlightStyle(idValue) {
    const node = await this.nodeWithId(idValue);
    const {result: {highlight}} =
        await this._dp.Overlay.getHighlightObjectForTest(
            {nodeId: node.id, includeStyle: true});
    const info =
        highlight['elementInfo'] ? highlight['elementInfo']['style'] : null;
    if (!info) {
      this._testRunner.log(`${idValue}: No style info`);
    } else {
      if (info['font-family']) {
        info['font-family'] = '<font-family value>';
      }
      this._testRunner.log(idValue + JSON.stringify(info, null, 2));
    }
  }
});
