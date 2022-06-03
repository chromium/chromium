/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.BlockquoteTest');
goog.setTestOnly();

const Blockquote = goog.require('goog.editor.plugins.Blockquote');
const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const Range = goog.require('goog.dom.Range');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

const SPLIT = '<span id="split-point"></span>';
let field;
let helper;
let plugin;
let root;

/**
 * @suppress {missingProperties,checkTypes} suppression added to enable type
 * checking
 */
function createPlugin(requireClassname, paragraphMode = undefined) {
  field.queryCommandValue('+defaultTag')
      .$anyTimes()
      .$returns(paragraphMode ? TagName.P : undefined);

  plugin = new Blockquote(requireClassname);
  plugin.registerFieldObject(field);
  plugin.enable(field);
}

/** @suppress {checkTypes} suppression added to enable type checking */
function execCommand() {
  field.$replay();

  // With splitPoint we try to mimic the behavior of EnterHandler's
  // deleteCursorSelection_.
  const splitPoint = dom.getElement('split-point');
  const position = BrowserFeature.HAS_W3C_RANGES ?
      {node: splitPoint.nextSibling, offset: 0} :
      splitPoint;
  if (BrowserFeature.HAS_W3C_RANGES) {
    dom.removeNode(splitPoint);
    Range.createCaret(position.node, 0).select();
  } else {
    Range.createCaret(position, 0).select();
  }

  const result = plugin.execCommand(Blockquote.SPLIT_COMMAND, position);
  if (!BrowserFeature.HAS_W3C_RANGES) {
    dom.removeNode(splitPoint);
  }

  return result;
}

testSuite({
  setUp() {
    root = dom.getElement('root');
    helper = new TestHelper(root);
    field = new FieldMock();

    helper.setUpEditableElement();
  },

  tearDown() {
    field.$verify();
    helper.tearDownEditableElement();
  },

  testSplitBlockquoteDoesNothingWhenNotInBlockquote() {
    root.innerHTML = `<div>Test${SPLIT}ing</div>`;

    createPlugin(false);
    assertFalse(execCommand());
    helper.assertHtmlMatches('<div>Testing</div>');
  },

  testSplitBlockquoteDoesNothingWhenNotInBlockquoteWithClass() {
    root.innerHTML = `<blockquote>Test${SPLIT}ing</blockquote>`;

    createPlugin(true);
    assertFalse(execCommand());
    helper.assertHtmlMatches('<blockquote>Testing</blockquote>');
  },

  testSplitBlockquoteInBlockquoteWithoutClass() {
    root.innerHTML = `<blockquote>Test${SPLIT}ing</blockquote>`;

    createPlugin(false);
    assertTrue(execCommand());
    helper.assertHtmlMatches(
        '<blockquote>Test</blockquote>' +
        '<div>' + (BrowserFeature.HAS_W3C_RANGES ? '&nbsp;' : '') + '</div>' +
        '<blockquote>ing</blockquote>');
  },

  testSplitBlockquoteInBlockquoteWithoutClassInParagraphMode() {
    root.innerHTML = `<blockquote>Test${SPLIT}ing</blockquote>`;

    createPlugin(false, true);
    assertTrue(execCommand());
    helper.assertHtmlMatches(
        '<blockquote>Test</blockquote>' +
        '<p>' + (BrowserFeature.HAS_W3C_RANGES ? '&nbsp;' : '') + '</p>' +
        '<blockquote>ing</blockquote>');
  },

  testSplitBlockquoteInBlockquoteWithClass() {
    root.innerHTML = `<blockquote class="tr_bq">Test${SPLIT}ing</blockquote>`;

    createPlugin(true);
    assertTrue(execCommand());

    helper.assertHtmlMatches(
        '<blockquote class="tr_bq">Test</blockquote>' +
        '<div>' + (BrowserFeature.HAS_W3C_RANGES ? '&nbsp;' : '') + '</div>' +
        '<blockquote class="tr_bq">ing</blockquote>');
  },

  testSplitBlockquoteInBlockquoteWithClassInParagraphMode() {
    root.innerHTML = `<blockquote class="tr_bq">Test${SPLIT}ing</blockquote>`;

    createPlugin(true, true);
    assertTrue(execCommand());
    helper.assertHtmlMatches(
        '<blockquote class="tr_bq">Test</blockquote>' +
        '<p>' + (BrowserFeature.HAS_W3C_RANGES ? '&nbsp;' : '') + '</p>' +
        '<blockquote class="tr_bq">ing</blockquote>');
  },

  testIsSplittableBlockquoteWhenRequiresClassNameToSplit() {
    createPlugin(true);

    const blockquoteWithClassName = dom.createDom(TagName.BLOCKQUOTE, 'tr_bq');
    assertTrue(
        'blockquote should be detected as splittable',
        plugin.isSplittableBlockquote(blockquoteWithClassName));

    const blockquoteWithoutClassName = dom.createDom(TagName.BLOCKQUOTE, 'foo');
    assertFalse(
        'blockquote should not be detected as splittable',
        plugin.isSplittableBlockquote(blockquoteWithoutClassName));

    const nonBlockquote = dom.createDom(TagName.SPAN, 'tr_bq');
    assertFalse(
        'element should not be detected as splittable',
        plugin.isSplittableBlockquote(nonBlockquote));
  },

  testIsSplittableBlockquoteWhenNotRequiresClassNameToSplit() {
    createPlugin(false);

    const blockquoteWithClassName = dom.createDom(TagName.BLOCKQUOTE, 'tr_bq');
    assertTrue(
        'blockquote should be detected as splittable',
        plugin.isSplittableBlockquote(blockquoteWithClassName));

    const blockquoteWithoutClassName = dom.createDom(TagName.BLOCKQUOTE, 'foo');
    assertTrue(
        'blockquote should be detected as splittable',
        plugin.isSplittableBlockquote(blockquoteWithoutClassName));

    const nonBlockquote = dom.createDom(TagName.SPAN, 'tr_bq');
    assertFalse(
        'element should not be detected as splittable',
        plugin.isSplittableBlockquote(nonBlockquote));
  },

  testIsSetupBlockquote() {
    createPlugin(false);

    const blockquoteWithClassName = dom.createDom(TagName.BLOCKQUOTE, 'tr_bq');
    assertTrue(
        'blockquote should be detected as setup',
        plugin.isSetupBlockquote(blockquoteWithClassName));

    const blockquoteWithoutClassName = dom.createDom(TagName.BLOCKQUOTE, 'foo');
    assertFalse(
        'blockquote should not be detected as setup',
        plugin.isSetupBlockquote(blockquoteWithoutClassName));

    const nonBlockquote = dom.createDom(TagName.SPAN, 'tr_bq');
    assertFalse(
        'element should not be detected as setup',
        plugin.isSetupBlockquote(nonBlockquote));
  },

  testIsUnsetupBlockquote() {
    createPlugin(false);

    const blockquoteWithClassName = dom.createDom(TagName.BLOCKQUOTE, 'tr_bq');
    assertFalse(
        'blockquote should not be detected as unsetup',
        plugin.isUnsetupBlockquote(blockquoteWithClassName));

    const blockquoteWithoutClassName = dom.createDom(TagName.BLOCKQUOTE, 'foo');
    assertTrue(
        'blockquote should be detected as unsetup',
        plugin.isUnsetupBlockquote(blockquoteWithoutClassName));

    const nonBlockquote = dom.createDom(TagName.SPAN, 'tr_bq');
    assertFalse(
        'element should not be detected as unsetup',
        plugin.isUnsetupBlockquote(nonBlockquote));
  },
});
