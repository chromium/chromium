/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.LinkShortcutPluginTest');
goog.setTestOnly();

const BasicTextFormatter = goog.require('goog.editor.plugins.BasicTextFormatter');
const Field = goog.require('goog.editor.Field');
const KeyCodes = goog.require('goog.events.KeyCodes');
const LinkBubble = goog.require('goog.editor.plugins.LinkBubble');
const LinkShortcutPlugin = goog.require('goog.editor.plugins.LinkShortcutPlugin');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');

let propertyReplacer;

testSuite({
  setUp() {
    propertyReplacer = new PropertyReplacer();
  },

  tearDown() {
    propertyReplacer.reset();
    const field = document.getElementById('cleanup');
    dom.removeChildren(field);
    field.innerHTML = '<div id="field">http://www.google.com/</div>';
  },

  testShortcutCreatesALink() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    propertyReplacer.set(window, 'prompt', () => 'http://www.google.com/');
    const linkBubble = new LinkBubble();
    const formatter = new BasicTextFormatter();
    const plugin = new LinkShortcutPlugin();
    const fieldEl = document.getElementById('field');
    const field = new Field('field');
    field.registerPlugin(formatter);
    field.registerPlugin(linkBubble);
    field.registerPlugin(plugin);
    field.makeEditable();
    field.focusAndPlaceCursorAtStart();
    const textNode = testingDom.findTextNode('http://www.google.com/', fieldEl);
    events.fireKeySequence(field.getElement(), KeyCodes.K, {ctrlKey: true});

    /** @suppress {checkTypes} suppression added to enable type checking */
    const href = dom.getElementsByTagName(TagName.A, field.getElement())[0];
    assertEquals('http://www.google.com/', href.href);
    /** @suppress {visibility} suppression added to enable type checking */
    const bubbleLink = document.getElementById(LinkBubble.TEST_LINK_ID_);
    assertEquals('http://www.google.com/', bubbleLink.innerHTML);
  },
});
