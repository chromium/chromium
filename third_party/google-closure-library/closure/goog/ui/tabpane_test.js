/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.TabPaneTest');
goog.setTestOnly();

const TabPane = goog.require('goog.ui.TabPane');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let tabPane;
let page1;
let page2;
let page3;

testSuite({
  setUp() {
    dom.getElement('testBody').innerHTML = '<div id="tabpane"></div>' +
        '<div id="page1Content">' +
        '  Content for page 1' +
        '</div>' +
        '<div id="page2Content">' +
        '  Content for page 2' +
        '</div>' +
        '<div id="page3Content">' +
        '  Content for page 3' +
        '</div>';

    tabPane = new TabPane(dom.getElement('tabpane'));
    page1 = new TabPane.TabPage(dom.getElement('page1Content'), 'page1');
    page2 = new TabPane.TabPage(dom.getElement('page2Content'), 'page2');
    page3 = new TabPane.TabPage(dom.getElement('page3Content'), 'page3');

    tabPane.addPage(page1);
    tabPane.addPage(page2);
    tabPane.addPage(page3);
  },

  tearDown() {
    tabPane.dispose();
  },

  testAllPagesEnabledAndSelectable() {
    tabPane.setSelectedIndex(0);
    let selected = tabPane.getSelectedPage();
    assertEquals('page1 should be selected', 'page1', selected.getTitle());
    assertEquals(
        'goog-tabpane-tab-selected', selected.getTitleElement().className);

    tabPane.setSelectedIndex(1);
    selected = tabPane.getSelectedPage();
    assertEquals('page2 should be selected', 'page2', selected.getTitle());
    assertEquals(
        'goog-tabpane-tab-selected', selected.getTitleElement().className);

    tabPane.setSelectedIndex(2);
    selected = tabPane.getSelectedPage();
    assertEquals('page3 should be selected', 'page3', selected.getTitle());
    assertEquals(
        'goog-tabpane-tab-selected', selected.getTitleElement().className);
  },

  testDisabledPageIsNotSelectable() {
    page2.setEnabled(false);
    assertEquals(
        'goog-tabpane-tab-disabled', page2.getTitleElement().className);

    tabPane.setSelectedIndex(0);
    let selected = tabPane.getSelectedPage();
    assertEquals('page1 should be selected', 'page1', selected.getTitle());
    assertEquals(
        'goog-tabpane-tab-selected', selected.getTitleElement().className);

    tabPane.setSelectedIndex(1);
    selected = tabPane.getSelectedPage();
    assertEquals(
        'page1 should remain selected, as page2 is disabled', 'page1',
        selected.getTitle());
    assertEquals(
        'goog-tabpane-tab-selected', selected.getTitleElement().className);

    tabPane.setSelectedIndex(2);
    selected = tabPane.getSelectedPage();
    assertEquals('page3 should be selected', 'page3', selected.getTitle());
    assertEquals(
        'goog-tabpane-tab-selected', selected.getTitleElement().className);
  },
});
