/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fx.DragDropGroupTest');
goog.setTestOnly();

const DragDropGroup = goog.require('goog.fx.DragDropGroup');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

let s1;
let s2;
let t1;
let t2;

let source = null;
let target = null;

function addElementsToGroups() {
  source.addItem(s1);
  source.addItem(s2);
  target.addItem(t1);
  target.addItem(t2);
}

testSuite({
  setUpPage() {
    s1 = document.getElementById('s1');
    s2 = document.getElementById('s2');
    t1 = document.getElementById('t1');
    t2 = document.getElementById('t2');
  },

  setUp() {
    source = new DragDropGroup();
    source.setSourceClass('ss');
    source.setTargetClass('st');

    target = new DragDropGroup();
    target.setSourceClass('ts');
    target.setTargetClass('tt');

    source.addTarget(target);
  },

  tearDown() {
    source.removeItems();
    target.removeItems();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddItemsBeforeInit() {
    addElementsToGroups();
    source.init();
    target.init();

    assertEquals(2, source.items_.length);
    assertEquals(2, target.items_.length);

    assertEquals('s ss', s1.className);
    assertEquals('s ss', s2.className);
    assertEquals('t tt', t1.className);
    assertEquals('t tt', t2.className);

    assertTrue(events.hasListener(s1));
    assertTrue(events.hasListener(s2));
    assertFalse(events.hasListener(t1));
    assertFalse(events.hasListener(t2));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddItemsAfterInit() {
    source.init();
    target.init();
    addElementsToGroups();

    assertEquals(2, source.items_.length);
    assertEquals(2, target.items_.length);

    assertEquals('s ss', s1.className);
    assertEquals('s ss', s2.className);
    assertEquals('t tt', t1.className);
    assertEquals('t tt', t2.className);

    assertTrue(events.hasListener(s1));
    assertTrue(events.hasListener(s2));
    assertFalse(events.hasListener(t1));
    assertFalse(events.hasListener(t2));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveItems() {
    source.init();
    target.init();
    addElementsToGroups();

    assertEquals(2, source.items_.length);
    assertEquals(s1, source.items_[0].element);
    assertEquals(s2, source.items_[1].element);

    assertEquals('s ss', s1.className);
    assertEquals('s ss', s2.className);
    assertTrue(events.hasListener(s1));
    assertTrue(events.hasListener(s2));

    source.removeItems();

    assertEquals(0, source.items_.length);

    assertEquals('s', s1.className);
    assertEquals('s', s2.className);
    assertFalse(events.hasListener(s1));
    assertFalse(events.hasListener(s2));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveSourceItem1() {
    source.init();
    target.init();
    addElementsToGroups();

    assertEquals(2, source.items_.length);
    assertEquals(s1, source.items_[0].element);
    assertEquals(s2, source.items_[1].element);

    assertEquals('s ss', s1.className);
    assertEquals('s ss', s2.className);
    assertTrue(events.hasListener(s1));
    assertTrue(events.hasListener(s2));

    source.removeItem(s1);

    assertEquals(1, source.items_.length);
    assertEquals(s2, source.items_[0].element);

    assertEquals('s', s1.className);
    assertEquals('s ss', s2.className);
    assertFalse(events.hasListener(s1));
    assertTrue(events.hasListener(s2));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveSourceItem2() {
    source.init();
    target.init();
    addElementsToGroups();

    assertEquals(2, source.items_.length);
    assertEquals(s1, source.items_[0].element);
    assertEquals(s2, source.items_[1].element);

    assertEquals('s ss', s1.className);
    assertEquals('s ss', s2.className);
    assertTrue(events.hasListener(s1));
    assertTrue(events.hasListener(s2));

    source.removeItem(s2);

    assertEquals(1, source.items_.length);
    assertEquals(s1, source.items_[0].element);

    assertEquals('s ss', s1.className);
    assertEquals('s', s2.className);
    assertTrue(events.hasListener(s1));
    assertFalse(events.hasListener(s2));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveTargetItem1() {
    source.init();
    target.init();
    addElementsToGroups();

    assertEquals(2, target.items_.length);
    assertEquals(t1, target.items_[0].element);
    assertEquals(t2, target.items_[1].element);

    assertEquals('t tt', t1.className);
    assertEquals('t tt', t2.className);
    assertFalse(events.hasListener(t1));
    assertFalse(events.hasListener(t2));

    target.removeItem(t1);

    assertEquals(1, target.items_.length);
    assertEquals(t2, target.items_[0].element);

    assertEquals('t', t1.className);
    assertEquals('t tt', t2.className);
    assertFalse(events.hasListener(t1));
    assertFalse(events.hasListener(t2));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveTargetItem2() {
    source.init();
    target.init();
    addElementsToGroups();

    assertEquals(2, target.items_.length);
    assertEquals(t1, target.items_[0].element);
    assertEquals(t2, target.items_[1].element);

    assertEquals('t tt', t1.className);
    assertEquals('t tt', t2.className);
    assertFalse(events.hasListener(t1));
    assertFalse(events.hasListener(t2));

    target.removeItem(t2);

    assertEquals(1, target.items_.length);
    assertEquals(t1, target.items_[0].element);

    assertEquals('t tt', t1.className);
    assertEquals('t', t2.className);
    assertFalse(events.hasListener(t1));
    assertFalse(events.hasListener(t2));
  },
});
