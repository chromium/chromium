// Copyright 2006 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

goog.module('goog.ds.JsDataSourceTest');
goog.setTestOnly();

const DataManager = goog.require('goog.ds.DataManager');
const JsDataSource = goog.require('goog.ds.JsDataSource');
const SortedNodeList = goog.require('goog.ds.SortedNodeList');
const XmlDataSource = goog.require('goog.ds.XmlDataSource');
const testSuite = goog.require('goog.testing.testSuite');
const xml = goog.require('goog.dom.xml');

let xmlDs;
let jsDs;

/** Constructs an array of data nodes from a javascript array. */
function createDataNodesArrayFromJs(jsObj) {
  const jsds = new JsDataSource(jsObj, 'MYJSDS', null);
  const dataNodes = jsds.getChildNodes();
  const dataNodesArray = [];
  const dataNodesCount = dataNodes.getCount();
  for (let i = 0; i < dataNodesCount; i++) {
    dataNodesArray[i] = dataNodes.getByIndex(i);
  }
  return dataNodesArray;
}

function valueSort(a, b) {
  const valueA = a.getChildNode('Value').get();
  const valueB = b.getChildNode('Value').get();

  return (valueA - valueB);
}
testSuite({
  setUp() {
    const xmltext = '<test><node value="5">some data</node></test>';
    const doc = xml.loadXml(xmltext);
    xmlDs = new XmlDataSource(doc.documentElement, null, null);

    const jsObj = {
      node: {
        '@value': 5,
        '#text': 'some data',
        name: 'bob',
        age: 35,
        alive: true,
        aliases: ['bobbo', 'robbo'],
      },
    };
    jsDs = new JsDataSource(jsObj, 'JSDS', null);
  },

  testJsDataSource() {
    const child = jsDs.getChildNode('node');
    const attr = child.getChildNode('@value');
    const text = child.getChildNode('#text');
    const name = child.getChildNode('name');
    const age = child.getChildNode('age');
    const alive = child.getChildNode('alive');
    const aliases = child.getChildNode('aliases');

    assertEquals('Attribute get', attr.get(), 5);
    assertEquals('Text get', text.get(), 'some data');
    assertEquals('string node get', name.get(), 'bob');
    assertEquals('Number get', age.get(), 35);
    assertEquals('Boolean get', alive.get(), true);
    assertEquals('Array value', aliases.get().getByIndex(1).get(), 'robbo');
    assertEquals('Array length', aliases.get().getCount(), 2);

    assertEquals('Datasource name', jsDs.getDataName(), 'JSDS');
  },

  testXmlDataSource() {
    const child = xmlDs.getChildNode('node');
    const attr = child.getChildNode('@value');
    const text = child.getChildNode('#text');

    assertEquals('Attribute get', attr.get(), '5');
    assertEquals('Text get', text.get(), 'some data');
    assertEquals(
        'Attr child node value', child.getChildNodeValue('@value'), '5');
  },

  testChildNodeValue() {
    const child = jsDs.getChildNode('node');
    assertEquals('Child node value', child.getChildNodeValue('age'), 35);
  },

  testJsSet() {
    assertNull('Get new child node is null', jsDs.getChildNode('Newt'));

    jsDs.setChildNode('Newt', 'A newt');
    assertEquals(
        'New string child node', jsDs.getChildNode('Newt').get(), 'A newt');

    jsDs.setChildNode('Number', 35);
    assertEquals('New number child node', jsDs.getChildNodeValue('Number'), 35);

    const numNode = jsDs.getChildNode('Number');
    jsDs.getChildNode('Number').set(38);
    assertEquals('Changed number child node', numNode.get(), 38);

    assertThrows('Shouldn\'t be able to set a group node yet', () => {
      jsDs.set(5);
    });
  },

  testDataManager() {
    const dm = DataManager.getInstance();
    assertNotNull('DataManager exists', dm);
    assertTrue('No datasources yet', dm.getChildNodes().getCount() == 0);
    dm.addDataSource(jsDs, true);
    assertTrue('One data source', dm.getChildNodes().getCount() == 1);
    assertEquals(
        'Renamed to global prefix', '$JSDS',
        dm.getChildNodes().getByIndex(0).getDataName());
  },

  testSortedNodeListConstruction() {
    const dataNodesArray = createDataNodesArrayFromJs([
      {'Value': 2, 'id': 'C'},
      {'Value': 0, 'id': 'A'},
      {'Value': 1, 'id': 'B'},
      {'Value': 3, 'id': 'D'},
    ]);

    const sortedNodeList = new SortedNodeList(valueSort, dataNodesArray);

    assertEquals('SortedNodeList count', 4, sortedNodeList.getCount());

    const expectedValues = [0, 1, 2, 3];
    for (let i = 0; i < expectedValues.length; i++) {
      assertEquals(
          'SortedNodeList position after construction', expectedValues[i],
          sortedNodeList.getByIndex(i).getChildNode('Value').get());
    }
  },

  testSortedNodeListAdd() {
    const sortedNodeList = new SortedNodeList(valueSort);

    const dataNodesArray = createDataNodesArrayFromJs([
      {'Value': 2, 'id': 'C'},
      {'Value': 0, 'id': 'A'},
      {'Value': 1, 'id': 'B'},
      {'Value': 3, 'id': 'D'},
    ]);

    for (let i = 0; i < dataNodesArray.length; i++) {
      sortedNodeList.add(dataNodesArray[i]);
    }

    assertEquals('SortedNodeList count', 4, sortedNodeList.getCount());

    const expectedValues = [0, 1, 2, 3];
    for (let i = 0; i < expectedValues.length; i++) {
      assertEquals(
          'SortedNodeList position after construction', expectedValues[i],
          sortedNodeList.getByIndex(i).getChildNode('Value').get());
    }
  },

  testSortedNodeListAppend() {
    const sortedNodeList = new SortedNodeList(valueSort);

    const dataNodesArray = createDataNodesArrayFromJs([
      {'Value': 2, 'id': 'C'},
      {'Value': 0, 'id': 'A'},
      {'Value': 1, 'id': 'B'},
      {'Value': 3, 'id': 'D'},
    ]);

    for (let i = 0; i < dataNodesArray.length; i++) {
      sortedNodeList.append(dataNodesArray[i]);
    }

    assertEquals(
        'SortedNodeList count', dataNodesArray.length,
        sortedNodeList.getCount());

    const expectedValues = [2, 0, 1, 3];
    for (let i = 0; i < expectedValues.length; i++) {
      assertEquals(
          'SortedNodeList position after construction', expectedValues[i],
          sortedNodeList.getByIndex(i).getChildNode('Value').get());
    }
  },

  testSortedNodeListSet() {
    const dataNodesArray = createDataNodesArrayFromJs([
      {'Value': 4, 'id': 'C'},
      {'Value': 0, 'id': 'A'},
      {'Value': 2, 'id': 'B'},
      {'Value': 6, 'id': 'D'},
    ]);

    const sortedNodeList = new SortedNodeList(valueSort, dataNodesArray);

    assertEquals('SortedNodeList count', 4, sortedNodeList.getCount());

    // test set that replaces an existing node
    const replaceNode =
        createDataNodesArrayFromJs([{'Value': 5, 'id': 'B'}])[0];
    sortedNodeList.setNode('B', replaceNode);

    assertEquals('SortedNodeList count', 4, sortedNodeList.getCount());
    assertEquals(
        'SortedNodeList replacement node correct', replaceNode,
        sortedNodeList.get('B'));

    let expectedValues = [0, 4, 5, 6];
    for (let i = 0; i < expectedValues.length; i++) {
      assertEquals(
          'SortedNodeList position after set', expectedValues[i],
          sortedNodeList.getByIndex(i).getChildNode('Value').get());
    }

    // test a set that adds a new node
    const addedNode = createDataNodesArrayFromJs([{'Value': 1, 'id': 'E'}])[0];
    sortedNodeList.setNode('E', addedNode);

    assertEquals('SortedNodeList count', 5, sortedNodeList.getCount());
    assertEquals(
        'SortedNodeList added node correct', addedNode,
        sortedNodeList.get('E'));

    expectedValues = [0, 1, 4, 5, 6];
    for (let i = 0; i < expectedValues.length; i++) {
      assertEquals(
          'SortedNodeList position after set', expectedValues[i],
          sortedNodeList.getByIndex(i).getChildNode('Value').get());
    }
  },
});
