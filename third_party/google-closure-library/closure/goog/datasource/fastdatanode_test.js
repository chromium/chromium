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

goog.module('goog.ds.FastDataNodeTest');
goog.setTestOnly();

const DataManager = goog.require('goog.ds.DataManager');
const Expr = goog.require('goog.ds.Expr');
const FastDataNode = goog.require('goog.ds.FastDataNode');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');

let simpleObject;
let complexObject;
let dataChangeEvents;

function verifyDataChangeEvents(expected) {
  assertEquals(expected.length, dataChangeEvents.length);
  for (let i = 0; i < expected.length; ++i) {
    assertEquals(expected[i], dataChangeEvents[i]);
  }
  dataChangeEvents = [];
}

testSuite({
  setUp() {
    simpleObject = {Name: 'Jon Doe', Email: 'jon.doe@gmail.com'};
    complexObject = {
      Name: 'Jon Doe',
      Email: 'jon.doe@gmail.com',
      Emails: [
        {Address: 'jon.doe@gmail.com', Type: 'Home'},
        {Address: 'jon.doe@workplace.com', Type: 'Work'},
      ],
      GroupIds: [23, 42],
    };
    const dm = DataManager.getInstance();
    dataChangeEvents = [];
    dm.fireDataChange = (dataPath) => {
      dataChangeEvents.push(dataPath);
    };
  },

  tearDown() {
    DataManager.clearInstance();
  },

  testGetChildNodeValue() {
    const node = new FastDataNode(simpleObject, 'Simple');
    const value = node.getChildNodeValue('Name');
    assert(typeof value === 'string');
    assertEquals('Jon Doe', value);
  },

  testDataNameAndPath() {
    const node = new FastDataNode(simpleObject, 'Simple');
    assertEquals('DataName should be \'Simple\'', 'Simple', node.getDataName());
    assertEquals('DataPath should be \'Simple\'', 'Simple', node.getDataPath());
  },

  testStringChildNode() {
    const node = FastDataNode.fromJs(simpleObject, 'Simple');
    const name = node.getChildNode('Name');
    const email = node.getChildNode('Email');
    assertEquals('Jon Doe', name.get());
    assertEquals('jon.doe@gmail.com', email.get());

    assertEquals('Name', name.getDataName());
    assertEquals('Simple/Name', name.getDataPath());

    assertEquals('Email', email.getDataName());
    assertEquals('Simple/Email', email.getDataPath());
  },

  testGetChildNodes() {
    const node = FastDataNode.fromJs(simpleObject, 'Simple');
    const children = node.getChildNodes();
    assertEquals(2, children.getCount());
    const childValues = [];
    for (let i = 0; i < 2; ++i) {
      childValues.push(children.getByIndex(i).get());
    }
    googArray.sort(childValues);
    assertEquals('Jon Doe', childValues[0]);
    assertEquals('jon.doe@gmail.com', childValues[1]);
  },

  testGetDistinguishesBetweenOverloads() {
    const node = FastDataNode.fromJs(simpleObject, 'Simple');
    assertEquals(node, node.get());
    assertEquals('Jon Doe', node.getChildNodes().get('Name').get());
  },

  testGetChildNodesForPrimitiveNodes() {
    const node = FastDataNode.fromJs(simpleObject, 'Simple');
    const children = node.getChildNode('Name').getChildNodes();
    assertEquals(0, children.getCount());
  },

  testFastListNode() {
    const node = FastDataNode.fromJs(complexObject, 'Object');
    assertEquals('Jon Doe', node.getChildNodeValue('Name'));
    const emails = node.getChildNode('Emails');
    assertEquals(
        'jon.doe@gmail.com',
        emails.getChildNode('[0]').getChildNodeValue('Address'));
    assertEquals(
        'jon.doe@workplace.com',
        emails.getChildNode('[1]').getChildNodeValue('Address'));

    assertEquals(
        'Object/Emails/[0]/Address',
        emails.getChildNode('[0]').getChildNode('Address').getDataPath());

    const groups = node.getChildNode('GroupIds');
    assertEquals(23, groups.getChildNode('[0]').get());
    assertEquals(42, groups.getChildNodeValue('[1]'));

    const childValues = emails.getChildNodes();
    assertEquals(2, childValues.getCount());
    assertEquals(
        'jon.doe@gmail.com',
        childValues.getByIndex(0).getChildNodeValue('Address'));
  },

  testChildNodeValueForNonexistantAttribute() {
    const node = FastDataNode.fromJs(complexObject, 'Object');
    assertNull(node.getChildNodeValue('DoesNotExist'));
    assertNull(node.getChildNode('Emails').getChildNodeValue('[666]'));
  },

  testAllChildrenSelector() {
    const node = FastDataNode.fromJs(complexObject, 'Object');
    const allChildren = node.getChildNodes('*');
    assertEquals(4, allChildren.getCount());

    // not implemented, yet
    // var nameChild = node.getChildNodes('Name');
    // assertEquals(1, allChildren.getCount());
  },

  testExpression() {
    const node = FastDataNode.fromJs(complexObject, 'Object');
    assertEquals('Jon Doe', Expr.create('Name').getValue(node));
    assertEquals(
        'jon.doe@workplace.com',
        Expr.create('Emails/[1]/Address').getNode(node).get());
    const emails = Expr.create('Emails/*').getNodes(node);
    assertEquals(2, emails.getCount());
    assertEquals(
        'jon.doe@workplace.com',
        emails.getByIndex(1).getChildNodeValue('Address'));
  },

  testModifyNode() {
    const node = FastDataNode.fromJs(complexObject, 'Object');
    node.getChildNode('Name').set('Foo Bar');
    assertEquals('Foo Bar', node.getChildNodeValue('Name'));
  },

  testClone() {
    const node = FastDataNode.fromJs(complexObject, 'Object');
    const clone = node.clone();
    node.getChildNode('Name').set('Foo Bar');
    assertEquals('Jon Doe', clone.getChildNodeValue('Name'));
    const expr = Expr.create('Emails/[1]/Address');
    expr.getNode(clone).set('jon@doe.com');
    assertEquals('jon.doe@workplace.com', expr.getValue(node));
    assertEquals('jon@doe.com', expr.getValue(clone));
  },

  testSetChildNodeOnList() {
    const list = FastDataNode.fromJs([], 'list');
    const node = FastDataNode.fromJs({Id: '42', Name: 'Foo'}, '42', list);
    list.setChildNode('42', node);

    assertEquals(node, list.getChildNode('42'));
    assertEquals(node, list.getChildNodes().getByIndex(0));
    assertEquals(node, list.getChildNodes().get('42'));
  },

  testCreateNewValueWithSetChildNode() {
    const node = FastDataNode.fromJs({}, 'object');
    node.setChildNode('Foo', 'Bar');
    assertEquals('Bar', node.getChildNodeValue('Foo'));
  },

  testSetChildNotWithNull_object() {
    const node = new FastDataNode({Foo: 'Bar'}, 'test');
    node.setChildNode('Foo', null);
    assertNull('node should not have a Foo child', node.getChildNode('Foo'));
    assertEquals(
        'node should not have any children', 0,
        node.getChildNodes().getCount());
  },

  testSetChildNotWithNull_list() {
    const list = FastDataNode.fromJs([], 'list');
    list.setChildNode('foo', 'bar');
    list.setChildNode('gee', 'wizz');
    assertEquals('bar', list.getChildNodeValue('foo'));
    assertEquals('wizz', list.getChildNodes().getByIndex(1).get());
    list.setChildNode('foo', null);
    assertEquals(1, list.getChildNodes().getCount());
    assertEquals('wizz', list.getChildNodeValue('gee'));
    assertEquals('wizz', list.getChildNodes().getByIndex(0).get());
  },

  testNodeListIndexesOnId() {
    const list = FastDataNode.fromJs([{id: '^Freq', value: 'foo'}], 'list');
    assertEquals('foo', list.getChildNode('^Freq').getChildNodeValue('value'));
    list.setChildNode('^Temp', {id: '^Temp', value: 'bar'});
    assertEquals('bar', list.getChildNode('^Temp').getChildNodeValue('value'));
  },

  testFireDataChangeOnSet() {
    const node = new FastDataNode(simpleObject, 'Simple');
    node.getChildNode('Name').set('Foo Bar');
    verifyDataChangeEvents(['Simple/Name']);
  },

  testFireDataChangeOnSetChildNode_object() {
    const node = new FastDataNode(simpleObject, 'Simple');
    node.setChildNode('Name', 'Foo Bar');
    node.setChildNode('Email', null);
    verifyDataChangeEvents(['Simple/Name', 'Simple/Email']);
  },

  testFireDataChangeOnSetChildNode_list() {
    const node = new FastDataNode(complexObject, 'Node');
    node.getChildNode('GroupIds').setChildNode('[0]', 1001);
    verifyDataChangeEvents(['Node/GroupIds/[0]']);

    node.getChildNode('GroupIds').getChildNodes().add(1002);
    verifyDataChangeEvents(
        ['Node/GroupIds/[2]', 'Node/GroupIds', 'Node/GroupIds/count()']);

    node.getChildNode('GroupIds').setChildNode('foo', 1003);
    verifyDataChangeEvents(
        ['Node/GroupIds/foo', 'Node/GroupIds', 'Node/GroupIds/count()']);
  },
});
