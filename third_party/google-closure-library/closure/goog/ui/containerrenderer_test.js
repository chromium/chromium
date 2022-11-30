/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ContainerRendererTest');
goog.setTestOnly();

const Container = goog.require('goog.ui.Container');
const ContainerRenderer = goog.require('goog.ui.ContainerRenderer');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const rendererasserts = goog.require('goog.testing.ui.rendererasserts');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let renderer;
let expectedFailures;
const stubs = new PropertyReplacer();

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    const sandbox = dom.getElement('sandbox');

    sandbox.appendChild(
        dom.createDom(TagName.SPAN, {id: 'noTabIndex'}, 'Test'));
    sandbox.appendChild(dom.createDom(
        TagName.DIV, {id: 'container', 'class': 'goog-container-horizontal'},
        dom.createDom(
            TagName.DIV, {id: 'control', 'class': 'goog-control'},
            'Hello, world!')));

    renderer = ContainerRenderer.getInstance();
  },

  tearDown() {
    dom.removeChildren(dom.getElement('sandbox'));
    stubs.reset();
    expectedFailures.handleTearDown();
  },

  testGetInstance() {
    assertTrue(
        'getInstance() must return a ContainerRenderer',
        renderer instanceof ContainerRenderer);
    assertEquals(
        'getInstance() must return the same object each time', renderer,
        ContainerRenderer.getInstance());
  },

  testGetCustomRenderer() {
    const cssClass = 'special-css-class';
    const containerRenderer =
        ContainerRenderer.getCustomRenderer(ContainerRenderer, cssClass);
    assertEquals(
        'Renderer should have returned the custom CSS class.', cssClass,
        containerRenderer.getCssClass());
  },

  testGetAriaRole() {
    assertUndefined('ARIA role must be undefined', renderer.getAriaRole());
  },

  /**
     @suppress {checkTypes,strictMissingProperties} suppression added to enable
     type checking
   */
  testEnableTabIndex() {
    const container = dom.getElement('container');
    assertFalse(
        'Container must not have any tab index',
        dom.isFocusableTabIndex(container));

    try {
      renderer.enableTabIndex(container, true);
      assertTrue(
          'Container must have a tab index',
          dom.isFocusableTabIndex(container));
      assertEquals('Container\'s tab index must be 0', 0, container.tabIndex);

      renderer.enableTabIndex(container, false);
      assertFalse(
          'Container must not have a tab index',
          dom.isFocusableTabIndex(container));
      assertEquals('Container\'s tab index must be -1', -1, container.tabIndex);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testCreateDom() {
    const horizontal = new Container(Container.Orientation.HORIZONTAL);
    const element1 = renderer.createDom(horizontal);
    assertEquals('Element must be a DIV', 'DIV', element1.tagName);
    assertEquals(
        'Element must have the expected class name',
        'goog-container goog-container-horizontal', element1.className);

    const vertical = new Container(Container.Orientation.VERTICAL);
    const element2 = renderer.createDom(vertical);
    assertEquals('Element must be a DIV', 'DIV', element2.tagName);
    assertEquals(
        'Element must have the expected class name',
        'goog-container goog-container-vertical', element2.className);
  },

  testGetContentElement() {
    assertNull(
        'getContentElement() must return null if element is null',
        renderer.getContentElement(null));
    const element = dom.getElement('container');
    assertEquals(
        'getContentElement() must return its argument', element,
        renderer.getContentElement(element));
  },

  testCanDecorate() {
    assertFalse(
        'canDecorate() must return false for a SPAN',
        renderer.canDecorate(dom.getElement('noTabIndex')));
    assertTrue(
        'canDecorate() must return true for a DIV',
        renderer.canDecorate(dom.getElement('container')));
  },

  testDecorate() {
    const container = new Container();
    const element = dom.getElement('container');

    assertFalse(
        'Container must not be in the document', container.isInDocument());
    container.decorate(element);
    assertTrue('Container must be in the document', container.isInDocument());

    assertEquals(
        'Container\'s ID must match the decorated element\'s ID', element.id,
        container.getId());
    assertEquals(
        'Element must have the expected class name',
        'goog-container-horizontal goog-container', element.className);
    assertEquals('Container must have one child', 1, container.getChildCount());
    assertEquals(
        'Child component\'s ID must be as expected', 'control',
        container.getChildAt(0).getId());

    assertThrows('Redecorating must throw error', () => {
      container.decorate(element);
    });
  },

  testDecorateWithCustomContainerElement() {
    const element = dom.getElement('container');
    const alternateContainerElement = dom.createElement(TagName.DIV);
    element.appendChild(alternateContainerElement);

    const container = new Container();
    stubs.set(renderer, 'getContentElement', () => alternateContainerElement);

    assertFalse(
        'Container must not be in the document', container.isInDocument());
    container.decorate(element);
    assertTrue('Container must be in the document', container.isInDocument());

    assertEquals(
        'Container\'s ID must match the decorated element\'s ID', element.id,
        container.getId());
    assertEquals(
        'Element must have the expected class name',
        'goog-container-horizontal goog-container', element.className);
    assertEquals(
        'Container must have 0 children', 0, container.getChildCount());

    assertThrows('Redecorating must throw error', () => {
      container.decorate(element);
    });
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetStateFromClassName() {
    const container = new Container();

    assertEquals(
        'Container must be vertical', Container.Orientation.VERTICAL,
        container.getOrientation());
    renderer.setStateFromClassName(
        container, 'goog-container-horizontal', 'goog-container');
    assertEquals(
        'Container must be horizontal', Container.Orientation.HORIZONTAL,
        container.getOrientation());
    renderer.setStateFromClassName(
        container, 'goog-container-vertical', 'goog-container');
    assertEquals(
        'Container must be vertical', Container.Orientation.VERTICAL,
        container.getOrientation());

    assertTrue('Container must be enabled', container.isEnabled());
    renderer.setStateFromClassName(
        container, 'goog-container-disabled', 'goog-container');
    assertFalse('Container must be disabled', container.isEnabled());
  },

  testInitializeDom() {
    const container = new Container();
    const element = dom.getElement('container');
    container.decorate(element);

    assertTrue(
        'Container\'s root element must be unselectable',
        style.isUnselectable(container.getElement()));

    assertEquals(
        'On IE, container\'s root element must have hideFocus=true',
        userAgent.IE, !!container.getElement().hideFocus);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDoesntCallGetCssClassInConstructor() {
    rendererasserts.assertNoGetCssClassCallsInConstructor(ContainerRenderer);
  },
});
