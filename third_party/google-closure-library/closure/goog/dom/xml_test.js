/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.xmlTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const domXml = goog.require('goog.dom.xml');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

testSuite({
  testSerialize() {
    const doc = domXml.createDocument();
    const node = doc.createElement('root');
    doc.appendChild(node);

    const serializedNode = domXml.serialize(node);
    assertTrue(/<root ?\/>/.test(serializedNode));

    const serializedDoc = domXml.serialize(doc);
    assertTrue(/(<\?xml version="1.0"\?>)?<root ?\/>/.test(serializedDoc));
  },

  testSerializeWithActiveX() {
    // Prefer ActiveXObject if available.
    const doc = domXml.createDocument('', '', true);
    const node = doc.createElement('root');
    doc.appendChild(node);

    const serializedNode = domXml.serialize(node);
    assertTrue(/<root ?\/>/.test(serializedNode));

    const serializedDoc = domXml.serialize(doc);
    assertTrue(/(<\?xml version="1.0"\?>)?<root ?\/>/.test(serializedDoc));
  },

  testSelectSingleNodeNoActiveX() {
    if (userAgent.IE) {
      return;
    }

    const xml = domXml.loadXml('<a><b><c>d</c></b></a>');
    const node = xml.firstChild;
    const bNode = domXml.selectSingleNode(node, 'b');
    assertNotNull(bNode);
  },

  testSelectSingleNodeWithActiveX() {
    // Enable ActiveXObject so IE has xpath support.
    const xml = domXml.loadXml('<a><b><c>d</c></b></a>', true);
    const node = xml.firstChild;
    const bNode = domXml.selectSingleNode(node, 'b');
    assertNotNull(bNode);
  },

  testSelectNodesNoActiveX() {
    if (userAgent.IE) {
      return;
    }

    const xml = domXml.loadXml('<a><b><c>d</c></b><b>foo</b></a>');
    const node = xml.firstChild;
    const bNodes = domXml.selectNodes(node, 'b');
    assertNotNull(bNodes);
    assertEquals(2, bNodes.length);
  },

  testSelectNodesWithActiveX() {
    const xml = domXml.loadXml('<a><b><c>d</c></b><b>foo</b></a>', true);
    const node = xml.firstChild;
    const bNodes = domXml.selectNodes(node, 'b');
    assertNotNull(bNodes);
    assertEquals(2, bNodes.length);
  },

  testSetAttributes() {
    const xmlElement = domXml.createDocument().createElement('root');
    const domElement = dom.createElement(TagName.DIV);
    const attrs =
        {name: 'test3', title: 'A title', random: 'woop', cellpadding: '123'};

    domXml.setAttributes(xmlElement, attrs);
    domXml.setAttributes(domElement, attrs);

    assertEquals('test3', xmlElement.getAttribute('name'));
    assertEquals('test3', domElement.getAttribute('name'));

    assertEquals('A title', xmlElement.getAttribute('title'));
    assertEquals('A title', domElement.getAttribute('title'));

    assertEquals('woop', xmlElement.getAttribute('random'));
    assertEquals('woop', domElement.getAttribute('random'));

    assertEquals('123', xmlElement.getAttribute('cellpadding'));
    assertEquals('123', domElement.getAttribute('cellpadding'));
  },
});
