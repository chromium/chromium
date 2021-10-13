/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Shared code for classlist_test.html. */

goog.module('goog.dom.classlist_test');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let expectedFailures;

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
  },

  tearDown() {
    expectedFailures.handleTearDown();
  },

  testGet() {
    const el = dom.createElement(TagName.DIV);
    assertTrue(classlist.get(el).length == 0);
    el.className = 'C';
    assertElementsEquals(['C'], classlist.get(el));
    el.className = 'C D';
    assertElementsEquals(['C', 'D'], classlist.get(el));
    el.className = 'C\nD';
    assertElementsEquals(['C', 'D'], classlist.get(el));
    el.className = ' C ';
    assertElementsEquals(['C'], classlist.get(el));
  },

  testGetSvg() {
    const el = dom.createElement(TagName.SVG);
    assertTrue(classlist.get(el).length == 0);
    el.setAttribute('class', 'C');
    assertElementsEquals(['C'], classlist.get(el));
    el.setAttribute('class', 'C D');
    assertElementsEquals(['C', 'D'], classlist.get(el));
    el.setAttribute('class', 'C\nD');
    assertElementsEquals(['C', 'D'], classlist.get(el));
    el.setAttribute('class', ' C ');
    assertElementsEquals(['C'], classlist.get(el));
  },

  testContainsWithNewlines() {
    const el = dom.getElement('p1');
    assertTrue('Should have SOMECLASS', classlist.contains(el, 'SOMECLASS'));
    assertTrue(
        'Should also have OTHERCLASS', classlist.contains(el, 'OTHERCLASS'));
    assertFalse(
        'Should not have WEIRDCLASS', classlist.contains(el, 'WEIRDCLASS'));
  },

  testContainsCaseSensitive() {
    const el = dom.getElement('p2');
    assertFalse(
        'Should not have camelcase', classlist.contains(el, 'camelcase'));
    assertFalse(
        'Should not have CAMELCASE', classlist.contains(el, 'CAMELCASE'));
    assertTrue('Should have camelCase', classlist.contains(el, 'camelCase'));
  },

  testAddNotAddingMultiples() {
    const el = dom.createElement(TagName.DIV);
    classlist.add(el, 'A');
    assertEquals('A', el.className);
    classlist.add(el, 'A');
    assertEquals('A', el.className);
    classlist.add(el, 'B');
    assertEquals('A B', el.className);
  },

  testAddNotAddingMultiplesSvg() {
    const el = dom.createElement(TagName.SVG);
    classlist.add(el, 'A');
    assertEquals('A', el.getAttribute('class'));
    classlist.add(el, 'A');
    assertEquals('A', el.getAttribute('class'));
    classlist.add(el, 'B');
    assertEquals('A B', el.getAttribute('class'));
  },

  testAddCaseSensitive() {
    const el = dom.createElement(TagName.DIV);
    classlist.add(el, 'A');
    assertTrue(classlist.contains(el, 'A'));
    assertFalse(classlist.contains(el, 'a'));
    classlist.add(el, 'a');
    assertTrue(classlist.contains(el, 'A'));
    assertTrue(classlist.contains(el, 'a'));
    assertEquals('A a', el.className);
  },

  testAddCaseSensitiveSvg() {
    const el = dom.createElement(TagName.SVG);
    classlist.add(el, 'A');
    assertTrue(classlist.contains(el, 'A'));
    assertFalse(classlist.contains(el, 'a'));
    classlist.add(el, 'a');
    assertTrue(classlist.contains(el, 'A'));
    assertTrue(classlist.contains(el, 'a'));
    assertEquals('A a', el.getAttribute('class'));
  },

  testAddAll() {
    const elem = dom.createElement(TagName.DIV);
    elem.className = 'foo goog-bar';

    classlist.addAll(elem, ['goog-baz', 'foo']);
    assertEquals(3, classlist.get(elem).length);
    assertTrue(classlist.contains(elem, 'foo'));
    assertTrue(classlist.contains(elem, 'goog-bar'));
    assertTrue(classlist.contains(elem, 'goog-baz'));
  },

  testAddAllSvg() {
    const elem = dom.createElement(TagName.SVG);
    elem.setAttribute('class', 'foo goog-bar');

    classlist.addAll(elem, ['goog-baz', 'foo']);
    assertEquals(3, classlist.get(elem).length);
    assertTrue(classlist.contains(elem, 'foo'));
    assertTrue(classlist.contains(elem, 'goog-bar'));
    assertTrue(classlist.contains(elem, 'goog-baz'));
  },

  testAddAllEmpty() {
    const classes = 'foo bar';
    const elem = dom.createElement(TagName.DIV);
    elem.className = classes;

    classlist.addAll(elem, []);
    assertEquals(elem.className, classes);
  },

  testRemove() {
    const el = dom.createElement(TagName.DIV);
    el.className = 'A B C';
    classlist.remove(el, 'B');
    assertEquals('A C', el.className);
  },

  testRemoveSvg() {
    const el = dom.createElement(TagName.SVG);
    el.setAttribute('class', 'A B C');
    classlist.remove(el, 'B');
    assertEquals('A C', el.getAttribute('class'));
  },

  testRemoveCaseSensitive() {
    const el = dom.createElement(TagName.DIV);
    el.className = 'A B C';
    classlist.remove(el, 'b');
    assertEquals('A B C', el.className);
  },

  testRemoveAll() {
    const elem = dom.createElement(TagName.DIV);
    elem.className = 'foo bar baz';

    classlist.removeAll(elem, ['bar', 'foo']);
    assertFalse(classlist.contains(elem, 'foo'));
    assertFalse(classlist.contains(elem, 'bar'));
    assertTrue(classlist.contains(elem, 'baz'));
  },

  testRemoveAllSvg() {
    const elem = dom.createElement(TagName.SVG);
    elem.setAttribute('class', 'foo bar baz');

    classlist.removeAll(elem, ['bar', 'foo']);
    assertFalse(classlist.contains(elem, 'foo'));
    assertFalse(classlist.contains(elem, 'bar'));
    assertTrue(classlist.contains(elem, 'baz'));
  },

  testRemoveAllOne() {
    const elem = dom.createElement(TagName.DIV);
    elem.className = 'foo bar baz';

    classlist.removeAll(elem, ['bar']);
    assertFalse(classlist.contains(elem, 'bar'));
    assertTrue(classlist.contains(elem, 'foo'));
    assertTrue(classlist.contains(elem, 'baz'));
  },

  testRemoveAllSomeNotPresent() {
    const elem = dom.createElement(TagName.DIV);
    elem.className = 'foo bar baz';

    classlist.removeAll(elem, ['a', 'bar']);
    assertTrue(classlist.contains(elem, 'foo'));
    assertFalse(classlist.contains(elem, 'bar'));
    assertTrue(classlist.contains(elem, 'baz'));
  },

  testRemoveAllCaseSensitive() {
    const elem = dom.createElement(TagName.DIV);
    elem.className = 'foo bar baz';

    classlist.removeAll(elem, ['BAR', 'foo']);
    assertFalse(classlist.contains(elem, 'foo'));
    assertTrue(classlist.contains(elem, 'bar'));
    assertTrue(classlist.contains(elem, 'baz'));
  },

  testEnable() {
    const el = dom.getElement('p1');
    classlist.set(el, 'SOMECLASS FIRST');

    assertTrue('Should have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue(
        'Should have SOMECLASS class', classlist.contains(el, 'SOMECLASS'));

    classlist.enable(el, 'FIRST', false);

    assertFalse('Should not have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue(
        'Should have SOMECLASS class', classlist.contains(el, 'SOMECLASS'));

    classlist.enable(el, 'FIRST', true);

    assertTrue('Should have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue(
        'Should have SOMECLASS class', classlist.contains(el, 'SOMECLASS'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEnableNotAddingMultiples() {
    const el = dom.createElement(TagName.DIV);
    classlist.enable(el, 'A', true);
    assertEquals('A', el.className);
    classlist.enable(el, 'A', true);
    assertEquals('A', el.className);
    classlist.enable(el, 'B', 'B', true);
    assertEquals('A B', el.className);
  },

  testEnableAllRemove() {
    const elem = dom.createElement(TagName.DIV);
    elem.className = 'foo bar baz';

    // Test removing some classes (some not present).
    classlist.enableAll(elem, ['a', 'bar'], false /* enable */);
    assertTrue(classlist.contains(elem, 'foo'));
    assertFalse(classlist.contains(elem, 'bar'));
    assertTrue(classlist.contains(elem, 'baz'));
    assertFalse(classlist.contains(elem, 'a'));
  },

  testEnableAllAdd() {
    const elem = dom.createElement(TagName.DIV);
    elem.className = 'foo bar';

    // Test adding some classes (some duplicate).
    classlist.enableAll(elem, ['a', 'bar', 'baz'], true /* enable */);
    assertTrue(classlist.contains(elem, 'foo'));
    assertTrue(classlist.contains(elem, 'bar'));
    assertTrue(classlist.contains(elem, 'baz'));
    assertTrue(classlist.contains(elem, 'a'));
  },

  testSwap() {
    const el = dom.getElement('p1');
    classlist.set(el, 'SOMECLASS FIRST');

    assertTrue('Should have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue('Should have FIRST class', classlist.contains(el, 'SOMECLASS'));
    assertFalse(
        'Should not have second class', classlist.contains(el, 'second'));

    classlist.swap(el, 'FIRST', 'second');

    assertFalse('Should not have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue('Should have FIRST class', classlist.contains(el, 'SOMECLASS'));
    assertTrue('Should have second class', classlist.contains(el, 'second'));

    classlist.swap(el, 'second', 'FIRST');

    assertTrue('Should have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue('Should have FIRST class', classlist.contains(el, 'SOMECLASS'));
    assertFalse(
        'Should not have second class', classlist.contains(el, 'second'));
  },

  testToggle() {
    const el = dom.getElement('p1');
    classlist.set(el, 'SOMECLASS FIRST');

    assertTrue('Should have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue(
        'Should have SOMECLASS class', classlist.contains(el, 'SOMECLASS'));

    let ret = classlist.toggle(el, 'FIRST');

    assertFalse('Should not have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue(
        'Should have SOMECLASS class', classlist.contains(el, 'SOMECLASS'));
    assertFalse('Return value should have been false', ret);

    ret = classlist.toggle(el, 'FIRST');

    assertTrue('Should have FIRST class', classlist.contains(el, 'FIRST'));
    assertTrue(
        'Should have SOMECLASS class', classlist.contains(el, 'SOMECLASS'));
    assertTrue('Return value should have been true', ret);
  },

  testAddRemoveString() {
    const el = dom.createElement(TagName.DIV);
    el.className = 'A';

    classlist.addRemove(el, 'A', 'B');
    assertEquals('B', el.className);

    classlist.addRemove(el, 'Z', 'C');
    assertEquals('B C', el.className);

    classlist.addRemove(el, 'C', 'D');
    assertEquals('B D', el.className);

    classlist.addRemove(el, 'D', 'B');
    assertEquals('B', el.className);
  },
});
