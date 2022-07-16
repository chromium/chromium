/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.TabBarRendererTest');
goog.setTestOnly();

const Container = goog.require('goog.ui.Container');
const Role = goog.require('goog.a11y.aria.Role');
const TabBar = goog.require('goog.ui.TabBar');
const TabBarRenderer = goog.require('goog.ui.TabBarRenderer');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const rendererasserts = goog.require('goog.testing.ui.rendererasserts');
const testSuite = goog.require('goog.testing.testSuite');

let sandbox;
let renderer;
let tabBar;

testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
    renderer = TabBarRenderer.getInstance();
    tabBar = new TabBar();
  },

  tearDown() {
    tabBar.dispose();
    dom.removeChildren(sandbox);
  },

  testConstructor() {
    assertNotNull('Renderer must not be null', renderer);
  },

  testGetCssClass() {
    assertEquals(
        'getCssClass() must return expected value', TabBarRenderer.CSS_CLASS,
        renderer.getCssClass());
  },

  testGetAriaRole() {
    assertEquals(
        'getAriaRole() must return expected value', Role.TAB_LIST,
        renderer.getAriaRole());
  },

  testCreateDom() {
    const element = renderer.createDom(tabBar);
    assertNotNull('Created element must not be null', element);
    assertEquals(
        'Created element must be a DIV', String(TagName.DIV), element.tagName);
    assertSameElements(
        'Created element must have expected class names',
        ['goog-tab-bar', 'goog-tab-bar-horizontal', 'goog-tab-bar-top'],
        classlist.get(element));
  },

  testDecorate() {
    sandbox.innerHTML = '<div id="start" class="goog-tab-bar-start"></div>';
    const element = renderer.decorate(tabBar, dom.getElement('start'));
    assertNotNull('Decorated element must not be null', element);
    assertEquals(
        'Decorated element must be as expected', dom.getElement('start'),
        element);
    // Due to a bug in ContainerRenderer, the "-vertical" class isn't applied.
    // TODO(attila): Fix this!
    assertSameElements(
        'Decorated element must have expected class names',
        ['goog-tab-bar', 'goog-tab-bar-start'], classlist.get(element));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetStateFromClassName() {
    renderer.setStateFromClassName(
        tabBar, 'goog-tab-bar-bottom', renderer.getCssClass());
    assertEquals(
        'Location must be BOTTOM', TabBar.Location.BOTTOM,
        tabBar.getLocation());
    assertEquals(
        'Orientation must be HORIZONTAL', Container.Orientation.HORIZONTAL,
        tabBar.getOrientation());

    renderer.setStateFromClassName(
        tabBar, 'goog-tab-bar-end', renderer.getCssClass());
    assertEquals(
        'Location must be END', TabBar.Location.END, tabBar.getLocation());
    assertEquals(
        'Orientation must be VERTICAL', Container.Orientation.VERTICAL,
        tabBar.getOrientation());

    renderer.setStateFromClassName(
        tabBar, 'goog-tab-bar-top', renderer.getCssClass());
    assertEquals(
        'Location must be TOP', TabBar.Location.TOP, tabBar.getLocation());
    assertEquals(
        'Orientation must be HORIZONTAL', Container.Orientation.HORIZONTAL,
        tabBar.getOrientation());

    renderer.setStateFromClassName(
        tabBar, 'goog-tab-bar-start', renderer.getCssClass());
    assertEquals(
        'Location must be START', TabBar.Location.START, tabBar.getLocation());
    assertEquals(
        'Orientation must be VERTICAL', Container.Orientation.VERTICAL,
        tabBar.getOrientation());
  },

  testGetClassNames() {
    assertSameElements(
        'Class names for TOP location must be as expected',
        ['goog-tab-bar', 'goog-tab-bar-horizontal', 'goog-tab-bar-top'],
        renderer.getClassNames(tabBar));

    tabBar.setLocation(TabBar.Location.START);
    assertSameElements(
        'Class names for START location must be as expected',
        ['goog-tab-bar', 'goog-tab-bar-vertical', 'goog-tab-bar-start'],
        renderer.getClassNames(tabBar));

    tabBar.setLocation(TabBar.Location.BOTTOM);
    assertSameElements(
        'Class names for BOTTOM location must be as expected',
        ['goog-tab-bar', 'goog-tab-bar-horizontal', 'goog-tab-bar-bottom'],
        renderer.getClassNames(tabBar));

    tabBar.setLocation(TabBar.Location.END);
    assertSameElements(
        'Class names for END location must be as expected',
        ['goog-tab-bar', 'goog-tab-bar-vertical', 'goog-tab-bar-end'],
        renderer.getClassNames(tabBar));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDoesntCallGetCssClassInConstructor() {
    rendererasserts.assertNoGetCssClassCallsInConstructor(TabBarRenderer);
  },
});
