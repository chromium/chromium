/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.style.app.ButtonRendererTest');
goog.setTestOnly();

const Button = goog.require('goog.ui.Button');
const ButtonRenderer = goog.require('goog.ui.style.app.ButtonRenderer');
const Component = goog.require('goog.ui.Component');
const dom = goog.require('goog.dom');
const style = goog.require('goog.testing.ui.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

const renderer = ButtonRenderer.getInstance();
let button;

// Write iFrame tag to load reference FastUI markup. Then, our tests will
// compare the generated markup to the reference markup.
const refPath = '../../../../../../webutil/css/legacy/fastui/app/button_spec.html';
style.writeReferenceFrame(refPath);

function shouldRunTests() {
  // Disable tests when being run as a part of open-source repo as the button
  // specs are not included in closure-library.
  return !(/closure\/goog\/ui/.test(location.pathname));
}

/*
 * This test demonstrates what happens when you put whitespace in a
 * decorated button's content, and the decorated element 'hasBoxStructure'.
 * Note that this behavior is different than when the element does not
 * have box structure. Should this be fixed?
 */

testSuite({
  setUp() {
    button = new Button('Hello Generated', renderer);
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
    assertEquals('Hello Generated', button.getContentElement().innerHTML);
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
    assertEquals('Hello Decorated', button.getContentElement().innerHTML);
    assertEquals(
        'Click for Decorated', button.getElement().getAttribute('title'));
  },

  testDecoratedButtonBox() {
    button.decorate(dom.getElement('button-box'));
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    assertEquals('Hello Decorated Box', button.getContentElement().innerHTML);
    assertEquals(
        'Click for Decorated Box', button.getElement().getAttribute('title'));
  },

  testDecoratedButtonBoxWithSpaceInContent() {
    button.decorate(dom.getElement('button-box-with-space-in-content'));
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(9)) {
      assertEquals(
          'Hello Decorated Box with space ',
          button.getContentElement().innerHTML);
    } else {
      assertEquals(
          '\n    Hello Decorated Box with space\n  ',
          button.getContentElement().innerHTML);
    }
  },

  testExistingContentIsUsed() {
    button.decorate(dom.getElement('button-box-with-dom-content'));
    style.assertStructureMatchesReference(
        button.getElement(), 'normal-resting');
    // Safari 3 adds style="-webkit-user-select" to the strong tag, so we
    // can't simply look at the HTML.
    const content = button.getContentElement();
    assertEquals(
        'Unexpected number of child nodes', 3, content.childNodes.length);
    assertEquals('Unexpected tag', 'STRONG', content.childNodes[0].tagName);
    assertEquals(
        'Unexpected text content', 'Hello', content.childNodes[0].innerHTML);
    assertEquals('Unexpected tag', 'EM', content.childNodes[2].tagName);
    assertEquals(
        'Unexpected text content', 'Box', content.childNodes[2].innerHTML);
  },
});
