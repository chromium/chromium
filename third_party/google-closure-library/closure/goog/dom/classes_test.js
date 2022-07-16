/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Shared code for classes_test.html & classes_quirks_test.html.
 */

goog.module('goog.dom.classes_test');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const classes = goog.require('goog.dom.classes');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testGet() {
    const el = dom.createElement(TagName.DIV);
    assertArrayEquals([], classes.get(el));
    el.className = 'C';
    assertArrayEquals(['C'], classes.get(el));
    el.className = 'C D';
    assertArrayEquals(['C', 'D'], classes.get(el));
    el.className = 'C\nD';
    assertArrayEquals(['C', 'D'], classes.get(el));
    el.className = ' C ';
    assertArrayEquals(['C'], classes.get(el));
  },

  testSetAddHasRemove() {
    const el = dom.getElement('p1');
    classes.set(el, 'SOMECLASS');
    assertTrue('Should have SOMECLASS', classes.has(el, 'SOMECLASS'));

    classes.set(el, 'OTHERCLASS');
    assertTrue('Should have OTHERCLASS', classes.has(el, 'OTHERCLASS'));
    assertFalse('Should not have SOMECLASS', classes.has(el, 'SOMECLASS'));

    classes.add(el, 'WOOCLASS');
    assertTrue('Should have OTHERCLASS', classes.has(el, 'OTHERCLASS'));
    assertTrue('Should have WOOCLASS', classes.has(el, 'WOOCLASS'));

    classes.add(el, 'ACLASS', 'BCLASS', 'CCLASS');
    assertTrue('Should have OTHERCLASS', classes.has(el, 'OTHERCLASS'));
    assertTrue('Should have WOOCLASS', classes.has(el, 'WOOCLASS'));
    assertTrue('Should have ACLASS', classes.has(el, 'ACLASS'));
    assertTrue('Should have BCLASS', classes.has(el, 'BCLASS'));
    assertTrue('Should have CCLASS', classes.has(el, 'CCLASS'));

    classes.remove(el, 'CCLASS');
    assertTrue('Should have OTHERCLASS', classes.has(el, 'OTHERCLASS'));
    assertTrue('Should have WOOCLASS', classes.has(el, 'WOOCLASS'));
    assertTrue('Should have ACLASS', classes.has(el, 'ACLASS'));
    assertTrue('Should have BCLASS', classes.has(el, 'BCLASS'));
    assertFalse('Should not have CCLASS', classes.has(el, 'CCLASS'));

    classes.remove(el, 'ACLASS', 'BCLASS');
    assertTrue('Should have OTHERCLASS', classes.has(el, 'OTHERCLASS'));
    assertTrue('Should have WOOCLASS', classes.has(el, 'WOOCLASS'));
    assertFalse('Should not have ACLASS', classes.has(el, 'ACLASS'));
    assertFalse('Should not have BCLASS', classes.has(el, 'BCLASS'));
  },

  // While support for this isn't implied in the method documentation,
  // this is a frequently used pattern.
  testAddWithSpacesInClassName() {
    const el = dom.getElement('p1');
    classes.add(el, 'CLASS1 CLASS2', 'CLASS3 CLASS4');
    assertTrue('Should have CLASS1', classes.has(el, 'CLASS1'));
    assertTrue('Should have CLASS2', classes.has(el, 'CLASS2'));
    assertTrue('Should have CLASS3', classes.has(el, 'CLASS3'));
    assertTrue('Should have CLASS4', classes.has(el, 'CLASS4'));
  },

  testSwap() {
    const el = dom.getElement('p1');
    classes.set(el, 'SOMECLASS FIRST');

    assertTrue('Should have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have FIRST class', classes.has(el, 'SOMECLASS'));
    assertFalse('Should not have second class', classes.has(el, 'second'));

    classes.swap(el, 'FIRST', 'second');

    assertFalse('Should not have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have FIRST class', classes.has(el, 'SOMECLASS'));
    assertTrue('Should have second class', classes.has(el, 'second'));

    classes.swap(el, 'second', 'FIRST');

    assertTrue('Should have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have FIRST class', classes.has(el, 'SOMECLASS'));
    assertFalse('Should not have second class', classes.has(el, 'second'));
  },

  testEnable() {
    const el = dom.getElement('p1');
    classes.set(el, 'SOMECLASS FIRST');

    assertTrue('Should have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have SOMECLASS class', classes.has(el, 'SOMECLASS'));

    classes.enable(el, 'FIRST', false);

    assertFalse('Should not have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have SOMECLASS class', classes.has(el, 'SOMECLASS'));

    classes.enable(el, 'FIRST', true);

    assertTrue('Should have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have SOMECLASS class', classes.has(el, 'SOMECLASS'));
  },

  testToggle() {
    const el = dom.getElement('p1');
    classes.set(el, 'SOMECLASS FIRST');

    assertTrue('Should have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have SOMECLASS class', classes.has(el, 'SOMECLASS'));

    classes.toggle(el, 'FIRST');

    assertFalse('Should not have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have SOMECLASS class', classes.has(el, 'SOMECLASS'));

    classes.toggle(el, 'FIRST');

    assertTrue('Should have FIRST class', classes.has(el, 'FIRST'));
    assertTrue('Should have SOMECLASS class', classes.has(el, 'SOMECLASS'));
  },

  testAddNotAddingMultiples() {
    const el = dom.getElement('span6');
    assertTrue(classes.add(el, 'A'));
    assertEquals('A', el.className);
    assertFalse(classes.add(el, 'A'));
    assertEquals('A', el.className);
    assertFalse(classes.add(el, 'B', 'B'));
    assertEquals('A B', el.className);
  },

  testAddRemoveString() {
    const el = dom.getElement('span6');
    el.className = 'A';

    classes.addRemove(el, 'A', 'B');
    assertEquals('B', el.className);

    classes.addRemove(el, null, 'C');
    assertEquals('B C', el.className);

    classes.addRemove(el, 'C', 'D');
    assertEquals('B D', el.className);

    classes.addRemove(el, 'D', null);
    assertEquals('B', el.className);
  },

  testAddRemoveArray() {
    const el = dom.getElement('span6');
    el.className = 'A';

    classes.addRemove(el, ['A'], ['B']);
    assertEquals('B', el.className);

    classes.addRemove(el, [], ['C']);
    assertEquals('B C', el.className);

    classes.addRemove(el, ['C'], ['D']);
    assertEquals('B D', el.className);

    classes.addRemove(el, ['D'], []);
    assertEquals('B', el.className);
  },

  testAddRemoveMultiple() {
    const el = dom.getElement('span6');
    el.className = 'A';

    classes.addRemove(el, ['A'], ['B', 'C', 'D']);
    assertEquals('B C D', el.className);

    classes.addRemove(el, [], ['E', 'F']);
    assertEquals('B C D E F', el.className);

    classes.addRemove(el, ['C', 'E'], []);
    assertEquals('B D F', el.className);

    classes.addRemove(el, ['B'], ['G']);
    assertEquals('D F G', el.className);
  },

  // While support for this isn't implied in the method documentation,
  // this is a frequently used pattern.
  testAddRemoveWithSpacesInClassName() {
    const el = dom.getElement('p1');
    classes.addRemove(el, '', 'CLASS1 CLASS2');
    assertTrue('Should have CLASS1', classes.has(el, 'CLASS1'));
    assertTrue('Should have CLASS2', classes.has(el, 'CLASS2'));
  },

  testHasWithNewlines() {
    const el = dom.getElement('p3');
    assertTrue('Should have SOMECLASS', classes.has(el, 'SOMECLASS'));
    assertTrue('Should also have OTHERCLASS', classes.has(el, 'OTHERCLASS'));
    assertFalse('Should not have WEIRDCLASS', classes.has(el, 'WEIRDCLASS'));
  },

  testEmptyClassNames() {
    const el = dom.getElement('span1');
    // At the very least, make sure these do not error out.
    assertFalse('Should not have an empty class', classes.has(el, ''));
    classes.add(el, '');
    classes.toggle(el, '');
    assertFalse('Should not remove an empty class', classes.remove(el, ''));
    classes.swap(el, '', 'OTHERCLASS');
    classes.swap(el, 'TEST1', '');
    classes.addRemove(el, '', '');
  },
});
