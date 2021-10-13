/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.style.app.MenuButtonRendererTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const MenuButton = goog.require('goog.ui.MenuButton');
const MenuButtonRenderer = goog.require('goog.ui.style.app.MenuButtonRenderer');
const dom = goog.require('goog.dom');
const style = goog.require('goog.testing.ui.style');
const testSuite = goog.require('goog.testing.testSuite');

const renderer = MenuButtonRenderer.getInstance();
let button;

// Write iFrame tag to load reference FastUI markup. Then, our tests will
// compare the generated markup to the reference markup.
const refPath =
    '../../../../../../webutil/css/legacy/fastui/app/menubutton_spec.html';
style.writeReferenceFrame(refPath);

function shouldRunTests() {
  // Disable tests when being run as a part of open-source repo as the button
  // specs are not included in closure-library.
  return !(/closure\/goog\/ui/.test(location.pathname));
}

testSuite({
  setUp() {
    button = new MenuButton('Hello Generated', null, renderer);
    button.setTooltip('Click for Generated');
  },

  tearDown() {
    if (button) {
      button.dispose();
    }
    dom.removeChildren(dom.getElement('sandbox'));
  },

  testGeneratedButton() {
    button.render(dom.getElement('sandbox'));
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    assertEquals(
        'Hello Generated', button.getContentElement().firstChild.nodeValue);
    assertEquals(
        'Click for Generated', button.getElement().getAttribute('title'));
  },

  testButtonStates() {
    button.render(dom.getElement('sandbox'));
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    button.setState(Component.State.HOVER, true);
    style.assertStructureMatchesReference(button.getElement(), 'normal-hover');
    button.setState(Component.State.HOVER, false);
    button.setState(Component.State.FOCUSED, true);
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-focused');
    button.setState(Component.State.FOCUSED, false);
    button.setState(Component.State.ACTIVE, true);
    style.assertStructureMatchesReference(button.getElement(), 'normal-active');
    button.setState(Component.State.ACTIVE, false);
    button.setState(Component.State.DISABLED, true);
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-disabled');
  },

  testDecoratedButton() {
    button.decorate(dom.getElement('button'));
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    assertEquals(
        'Hello Decorated', button.getContentElement().firstChild.nodeValue);
    assertEquals(
        'Click for Decorated', button.getElement().getAttribute('title'));
  },

  testDecoratedButtonBox() {
    button.decorate(dom.getElement('button-box'));
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    assertEquals(
        'Hello Decorated Box', button.getContentElement().firstChild.nodeValue);
    assertEquals(
        'Click for Decorated Box', button.getElement().getAttribute('title'));
  },

  testExistingContentIsUsed() {
    button.decorate(dom.getElement('button-with-dom-content'));
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    // Safari 3 adds style="-webkit-user-select" to the strong tag, so we
    // can't simply look at the HTML.
    const content = button.getContentElement();
    assertEquals(
        'Unexpected number of child nodes; expected existing number ' +
            'plus one for the dropdown element',
        4, content.childNodes.length);
    assertEquals('Unexpected tag', 'STRONG', content.childNodes[0].tagName);
    assertEquals(
        'Unexpected text content', 'Hello Strong',
        content.childNodes[0].innerHTML);
    assertEquals('Unexpected tag', 'EM', content.childNodes[2].tagName);
    assertEquals(
        'Unexpected text content', 'Box', content.childNodes[2].innerHTML);
  },

  testDecoratedButtonWithMenu() {
    button.decorate(dom.getElement('button-with-menu'));
    assertEquals('Unexpected number of menu items', 2, button.getItemCount());
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    assertFalse(
        'Expected menu element to not be contained by button',
        dom.contains(button.getElement(), button.getMenu().getElement()));
  },

  testDropDownExistsAfterButtonRename() {
    button.decorate(dom.getElement('button-2'));
    button.setContent('New title');
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    assertEquals(
        'Unexpected number of child nodes; expected text element ' +
            'and the dropdown element',
        2, button.getContentElement().childNodes.length);
    assertEquals('New title', button.getContentElement().firstChild.nodeValue);
    assertEquals(
        'Click for Decorated', button.getElement().getAttribute('title'));
  },
});
