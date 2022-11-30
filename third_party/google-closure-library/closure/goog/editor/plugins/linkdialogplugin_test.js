/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.editor.plugins.LinkDialogTest');
goog.setTestOnly();

const AbstractDialog = goog.require('goog.ui.editor.AbstractDialog');
const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const Command = goog.require('goog.editor.Command');
const DomHelper = goog.require('goog.dom.DomHelper');
const Field = goog.require('goog.editor.Field');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const Link = goog.require('goog.editor.Link');
const LinkDialog = goog.require('goog.ui.editor.LinkDialog');
const LinkDialogPlugin = goog.require('goog.editor.plugins.LinkDialogPlugin');
const MockControl = goog.require('goog.testing.MockControl');
const NodeType = goog.require('goog.dom.NodeType');
const SafeHtml = goog.require('goog.html.SafeHtml');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const Unicode = goog.require('goog.string.Unicode');
const dom = goog.require('goog.dom');
const editorDom = goog.require('goog.testing.editor.dom');
const events = goog.require('goog.testing.events');
const googString = goog.require('goog.string');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let plugin;
let anchorElem;
let extraAnchors;
let isNew;
let testDiv;

let mockCtrl;
let mockField;
let mockLink;
let mockAlert;

const OLD_LINK_TEXT = 'old text';
const OLD_LINK_URL = 'http://old.url/';
const NEW_LINK_TEXT = 'My Link Text';
const NEW_LINK_URL = 'http://my.link/url/';

let fieldElem;
let fieldObj;
let linkObj;

function setUpAnchor(
    text, href, isNew = undefined, target = undefined, rel = undefined) {
  setUpGivenAnchor(anchorElem, text, href, isNew, target, rel);
}

function setUpGivenAnchor(anchor, text, href, opt_isNew, opt_target, opt_rel) {
  anchor.innerHTML = text;
  anchor.href = href;
  isNew = !!opt_isNew;
  if (opt_target) {
    anchor.target = opt_target;
  }
  if (opt_rel) {
    anchor.rel = opt_rel;
  }
}

/**
 * @suppress {missingProperties,checkTypes} suppression added to enable type
 * checking
 */
function verifyRelNoFollow(noFollow, originalRel, expectedRel) {
  mockLink.placeCursorRightOf();
  mockField.dispatchSelectionChangeEvent();
  mockField.dispatchChange();
  mockField.focus();
  mockCtrl.$replayAll();

  plugin = new LinkDialogPlugin();
  plugin.registerFieldObject(mockField);
  plugin.showRelNoFollow();
  /**
   * @suppress {visibility,checkTypes} suppression added to enable type
   * checking
   */
  plugin.currentLink_ = mockLink;

  setUpAnchor(OLD_LINK_TEXT, OLD_LINK_URL, true, null, originalRel);
  /** @suppress {visibility} suppression added to enable type checking */
  const dialog = plugin.createDialog(new DomHelper(), mockLink);
  dialog.dispatchEvent(
      new LinkDialog.OkEvent(NEW_LINK_TEXT, NEW_LINK_URL, false, noFollow));
  assertEquals(expectedRel, anchorElem.rel);

  mockCtrl.$verifyAll();
}

/**
 * Setup a real editable field (instead of a mock) and register the plugin to
 * it.
 */
function setUpRealEditableField() {
  fieldElem = dom.createElement(TagName.DIV);
  fieldElem.id = 'myField';
  document.body.appendChild(fieldElem);
  fieldElem.appendChild(anchorElem);
  fieldObj = new Field('myField', document);
  fieldObj.makeEditable();
  /** @suppress {checkTypes} suppression added to enable type checking */
  linkObj = new Link(fieldObj.getElement().firstChild, isNew);
  // Register the plugin to that field.
  plugin = new LinkDialogPlugin();
  fieldObj.registerPlugin(plugin);
}

/** Tear down the real editable field. */
function tearDownRealEditableField() {
  if (fieldObj) {
    fieldObj.makeUneditable();
    fieldObj.dispose();
    fieldObj = null;
  }
  dom.removeNode(fieldElem);
}
testSuite({
  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  setUp() {
    testDiv = dom.getDocument().getElementById('test');
    dom.setTextContent(testDiv, 'Some preceding text');

    anchorElem = dom.createElement(TagName.A);
    anchorElem.href = 'http://www.google.com/';
    dom.setTextContent(anchorElem, 'anchor text');
    dom.appendChild(testDiv, anchorElem);
    extraAnchors = [];

    mockCtrl = new MockControl();
    mockField = new FieldMock();
    mockCtrl.addMock(mockField);
    mockLink = mockCtrl.createLooseMock(Link);
    mockAlert = mockCtrl.createGlobalFunctionMock('alert');

    isNew = false;
    mockLink.isNew().$anyTimes().$does(() => isNew);
    mockLink.setTextAndUrl(mockmatchers.isString, mockmatchers.isString)
        .$anyTimes()
        .$does((text, url) => {
          anchorElem.innerHTML = text;
          anchorElem.href = url;
        });
    mockLink.getAnchor().$anyTimes().$returns(anchorElem);
    mockLink.getExtraAnchors().$anyTimes().$returns(extraAnchors);
  },

  tearDown() {
    plugin.dispose();
    tearDownRealEditableField();
    dom.removeChildren(testDiv);
    mockCtrl.$tearDown();
  },

  /**
     Tests that the plugin's dialog is properly created.
     @suppress {checkTypes} suppression added to enable type checking
   */
  testCreateDialog() {
    // Note: this tests simply creating the dialog because that's the only
    // functionality added to this class. Opening or closing effects
    // (editing the actual link) is tested in linkdialog_test.html, but
    // should be moved here if that functionality gets refactored from the
    // dialog to the plugin.
    mockCtrl.$replayAll();

    plugin = new LinkDialogPlugin();
    plugin.registerFieldObject(mockField);

    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(new DomHelper(), mockLink);
    assertTrue(
        'Dialog should be of type goog.ui.editor.LinkDialog',
        dialog instanceof LinkDialog);

    mockCtrl.$verifyAll();
  },

  /**
     Tests that when the OK event fires the link is properly updated.
     @suppress {missingProperties,checkTypes} suppression added to enable
     type checking
   */
  testOk() {
    mockLink.placeCursorRightOf();
    mockField.dispatchSelectionChangeEvent();
    mockField.dispatchChange();
    mockField.focus();
    mockCtrl.$replayAll();

    setUpAnchor(OLD_LINK_TEXT, OLD_LINK_URL);
    plugin = new LinkDialogPlugin();
    plugin.registerFieldObject(mockField);
    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(new DomHelper(), mockLink);

    // Mock of execCommand + clicking OK without actually opening the
    // dialog.
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    plugin.currentLink_ = mockLink;
    dialog.dispatchEvent(new LinkDialog.OkEvent(NEW_LINK_TEXT, NEW_LINK_URL));

    assertEquals('Display text incorrect', NEW_LINK_TEXT, anchorElem.innerHTML);
    assertEquals(
        'Anchor element href incorrect', NEW_LINK_URL, anchorElem.href);

    mockCtrl.$verifyAll();
  },

  /**
     Tests that when the Cancel event fires the link is unchanged.
     @suppress {checkTypes} suppression added to enable type checking
   */
  testCancel() {
    mockCtrl.$replayAll();

    setUpAnchor(OLD_LINK_TEXT, OLD_LINK_URL);
    plugin = new LinkDialogPlugin();
    plugin.registerFieldObject(mockField);
    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(new DomHelper(), mockLink);

    // Mock of execCommand + cancel without actually opening the dialog.
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    plugin.currentLink_ = mockLink;
    dialog.dispatchEvent(AbstractDialog.EventType.CANCEL);

    assertEquals(
        'Display text should not be changed', OLD_LINK_TEXT,
        anchorElem.innerHTML);
    assertEquals(
        'Anchor element href should not be changed', OLD_LINK_URL,
        anchorElem.href);

    mockCtrl.$verifyAll();
  },

  /**
     Tests that when the Cancel event fires for a new link it gets removed.
     @suppress {missingProperties,checkTypes} suppression added to enable
     type checking
   */
  testCancelNew() {
    mockField.dispatchChange();  // Should be fired because link was removed.
    mockCtrl.$replayAll();

    setUpAnchor(OLD_LINK_TEXT, OLD_LINK_URL, true);
    const prevSib = anchorElem.previousSibling;
    plugin = new LinkDialogPlugin();
    plugin.registerFieldObject(mockField);
    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(new DomHelper(), mockLink);

    // Mock of execCommand + cancel without actually opening the dialog.
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    plugin.currentLink_ = mockLink;
    dialog.dispatchEvent(AbstractDialog.EventType.CANCEL);

    assertNotEquals(
        'Anchor element should be removed from document body', testDiv,
        anchorElem.parentNode);
    const newElem = prevSib.nextSibling;
    assertEquals(
        'Link should be replaced by text node', NodeType.TEXT,
        newElem.nodeType);
    assertEquals(
        'Original text should be left behind', OLD_LINK_TEXT,
        newElem.nodeValue);

    mockCtrl.$verifyAll();
  },

  /**
     Tests that when the Cancel event fires for a new link it gets removed.
     @suppress {missingProperties,checkTypes} suppression added to enable
     type checking
   */
  testCancelNewMultiple() {
    mockField.dispatchChange();  // Should be fired because link was removed.
    mockCtrl.$replayAll();

    const anchorElem1 = anchorElem;
    const parent1 = dom.createDom(TagName.DIV, null, anchorElem1);
    dom.appendChild(testDiv, parent1);
    setUpGivenAnchor(
        anchorElem1, `${OLD_LINK_TEXT}1`, `${OLD_LINK_URL}1`, true);

    let anchorElem2 = dom.createDom(TagName.A);
    const parent2 = dom.createDom(TagName.DIV, null, anchorElem2);
    dom.appendChild(testDiv, parent2);
    setUpGivenAnchor(
        anchorElem2, `${OLD_LINK_TEXT}2`, `${OLD_LINK_URL}2`, true);
    extraAnchors.push(anchorElem2);

    let anchorElem3 = dom.createDom(TagName.A);
    const parent3 = dom.createDom(TagName.DIV, null, anchorElem3);
    dom.appendChild(testDiv, parent3);
    setUpGivenAnchor(
        anchorElem3, `${OLD_LINK_TEXT}3`, `${OLD_LINK_URL}3`, true);
    extraAnchors.push(anchorElem3);

    plugin = new LinkDialogPlugin();
    plugin.registerFieldObject(mockField);
    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(new DomHelper(), mockLink);

    // Mock of execCommand + cancel without actually opening the dialog.
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    plugin.currentLink_ = mockLink;
    dialog.dispatchEvent(AbstractDialog.EventType.CANCEL);

    assertNotEquals(
        'Anchor 1 element should be removed from document body', parent1,
        anchorElem1.parentNode);
    assertNotEquals(
        'Anchor 2 element should be removed from document body', parent2,
        anchorElem2.parentNode);
    assertNotEquals(
        'Anchor 3 element should be removed from document body', parent3,
        anchorElem3.parentNode);

    assertEquals(
        'Link 1 should be replaced by text node', NodeType.TEXT,
        parent1.firstChild.nodeType);
    assertEquals(
        'Link 2 should be replaced by text node', NodeType.TEXT,
        parent2.firstChild.nodeType);
    assertEquals(
        'Link 3 should be replaced by text node', NodeType.TEXT,
        parent3.firstChild.nodeType);

    assertEquals(
        'Original text 1 should be left behind', `${OLD_LINK_TEXT}1`,
        parent1.firstChild.nodeValue);
    assertEquals(
        'Original text 2 should be left behind', `${OLD_LINK_TEXT}2`,
        parent2.firstChild.nodeValue);
    assertEquals(
        'Original text 3 should be left behind', `${OLD_LINK_TEXT}3`,
        parent3.firstChild.nodeValue);

    mockCtrl.$verifyAll();
  },

  /**
     Tests that when the Cancel event fires for a new link it gets removed.
     @suppress {missingProperties,checkTypes} suppression added to enable
     type checking
   */
  testOkNewMultiple() {
    mockLink.placeCursorRightOf();
    mockField.dispatchSelectionChangeEvent();
    mockField.dispatchChange();
    mockField.focus();
    mockCtrl.$replayAll();

    const anchorElem1 = anchorElem;
    setUpGivenAnchor(
        anchorElem1, `${OLD_LINK_TEXT}1`, `${OLD_LINK_URL}1`, true);

    let anchorElem2 = dom.createElement(TagName.A);
    dom.appendChild(testDiv, anchorElem2);
    setUpGivenAnchor(
        anchorElem2, `${OLD_LINK_TEXT}2`, `${OLD_LINK_URL}2`, true);
    extraAnchors.push(anchorElem2);

    let anchorElem3 = dom.createElement(TagName.A);
    dom.appendChild(testDiv, anchorElem3);
    setUpGivenAnchor(
        anchorElem3, `${OLD_LINK_TEXT}3`, `${OLD_LINK_URL}3`, true);
    extraAnchors.push(anchorElem3);

    const prevSib = anchorElem1.previousSibling;
    plugin = new LinkDialogPlugin();
    plugin.registerFieldObject(mockField);
    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(new DomHelper(), mockLink);

    // Mock of execCommand + clicking OK without actually opening the
    // dialog.
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    plugin.currentLink_ = mockLink;
    dialog.dispatchEvent(new LinkDialog.OkEvent(NEW_LINK_TEXT, NEW_LINK_URL));

    assertEquals(
        'Display text 1 must update', NEW_LINK_TEXT, anchorElem1.innerHTML);
    assertEquals(
        'Display text 2 must not update', `${OLD_LINK_TEXT}2`,
        anchorElem2.innerHTML);
    assertEquals(
        'Display text 3 must not update', `${OLD_LINK_TEXT}3`,
        anchorElem3.innerHTML);

    assertEquals(
        'Anchor element 1 href must update', NEW_LINK_URL, anchorElem1.href);
    assertEquals(
        'Anchor element 2 href must update', NEW_LINK_URL, anchorElem2.href);
    assertEquals(
        'Anchor element 3 href must update', NEW_LINK_URL, anchorElem3.href);

    mockCtrl.$verifyAll();
  },

  /**
   * Tests the anchor's target is correctly modified with the "open in new
   * window" feature on.
   * @suppress {missingProperties,checkTypes} suppression added to enable
   * type checking
   */
  testOkOpenInNewWindow() {
    mockLink.placeCursorRightOf().$anyTimes();
    mockField.dispatchSelectionChangeEvent().$anyTimes();
    mockField.dispatchChange().$anyTimes();
    mockField.focus().$anyTimes();
    mockCtrl.$replayAll();

    plugin = new LinkDialogPlugin();
    plugin.registerFieldObject(mockField);
    plugin.showOpenLinkInNewWindow(false);
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    plugin.currentLink_ = mockLink;

    // Edit a link that doesn't open in a new window and leave it as such.
    setUpAnchor(OLD_LINK_TEXT, OLD_LINK_URL);
    /** @suppress {visibility} suppression added to enable type checking */
    let dialog = plugin.createDialog(new DomHelper(), mockLink);
    dialog.dispatchEvent(
        new LinkDialog.OkEvent(NEW_LINK_TEXT, NEW_LINK_URL, false, false));
    assertEquals(
        'Target should not be set for link that doesn\'t open in new window',
        '', anchorElem.target);
    assertFalse(
        'Checked state should stay false',
        plugin.getOpenLinkInNewWindowCheckedState());

    // Edit a link that doesn't open in a new window and toggle it on.
    setUpAnchor(OLD_LINK_TEXT, OLD_LINK_URL);
    /** @suppress {visibility} suppression added to enable type checking */
    dialog = plugin.createDialog(new DomHelper(), mockLink);
    dialog.dispatchEvent(
        new LinkDialog.OkEvent(NEW_LINK_TEXT, NEW_LINK_URL, true));
    assertEquals(
        'Target should be set to _blank for link that opens in new window',
        '_blank', anchorElem.target);
    assertTrue(
        'Checked state should be true after toggling a link on',
        plugin.getOpenLinkInNewWindowCheckedState());

    // Edit a link that doesn't open in a named window and don't touch it.
    setUpAnchor(OLD_LINK_TEXT, OLD_LINK_URL, false, 'named');
    /** @suppress {visibility} suppression added to enable type checking */
    dialog = plugin.createDialog(new DomHelper(), mockLink);
    dialog.dispatchEvent(
        new LinkDialog.OkEvent(NEW_LINK_TEXT, NEW_LINK_URL, false));
    assertEquals(
        'Target should keep its original value', 'named', anchorElem.target);
    assertFalse(
        'Checked state should be false again',
        plugin.getOpenLinkInNewWindowCheckedState());

    // Edit a link that opens in a new window and toggle it off.
    setUpAnchor(OLD_LINK_TEXT, OLD_LINK_URL, false, '_blank');
    /** @suppress {visibility} suppression added to enable type checking */
    dialog = plugin.createDialog(new DomHelper(), mockLink);
    dialog.dispatchEvent(
        new LinkDialog.OkEvent(NEW_LINK_TEXT, NEW_LINK_URL, false));
    assertEquals(
        'Target should not be set for link that doesn\'t open in new window',
        '', anchorElem.target);

    mockCtrl.$verifyAll();
  },

  testOkNoFollowEnabled() {
    verifyRelNoFollow(true, null, 'nofollow');
  },

  testOkNoFollowInUppercase() {
    verifyRelNoFollow(true, 'NOFOLLOW', 'NOFOLLOW');
  },

  testOkNoFollowEnabledHasMoreRelValues() {
    verifyRelNoFollow(true, 'author', 'author nofollow');
  },

  testOkNoFollowDisabled() {
    verifyRelNoFollow(false, null, '');
  },

  testOkNoFollowDisabledHasMoreRelValues1() {
    verifyRelNoFollow(false, 'author', 'author');
  },

  testOkNoFollowDisabledHasMoreRelValues2() {
    verifyRelNoFollow(false, 'author nofollow', 'author ');
  },

  testOkNoFollowInUppercaseWithMoreValues() {
    verifyRelNoFollow(true, 'NOFOLLOW author', 'NOFOLLOW author');
  },

  /**
   * Tests that the selection is cleared when the dialog opens and is
   * correctly restored after cancel is clicked.
   * @suppress {visibility} suppression added to enable type checking
   */
  testRestoreSelectionOnOk() {
    setUpAnchor('12345', '/');
    setUpRealEditableField();

    const elem = fieldObj.getElement();
    const helper = new TestHelper(elem);
    helper.select('12345', 1, '12345', 4);  // Selects '234'.

    assertEquals(
        'Incorrect text selected before dialog is opened', '234',
        fieldObj.getRange().getText());
    plugin.execCommand(Command.MODAL_LINK_EDITOR, linkObj);
    if (!userAgent.IE) {
      // IE returns some bogus range when field doesn't have selection.
      // You can't remove the selection from a whitebox field in Opera.
      assertNull(
          'There should be no selection while dialog is open',
          fieldObj.getRange());
    }
    events.fireClickSequence(plugin.dialog_.getOkButtonElement());
    assertEquals(
        'No text should be selected after clicking ok', '',
        fieldObj.getRange().getText());

    // Test that the caret is placed at the end of the link text.
    editorDom.assertRangeBetweenText(
        // If the browser gets stuck in links, an nbsp was added after the
        // link to avoid that, otherwise we just look for the 5.
        BrowserFeature.GETS_STUCK_IN_LINKS ? Unicode.NBSP : '5', '',
        fieldObj.getRange());

    // NOTE(user): The functionality to avoid getting stuck in
    // links is tested in editablelink_test.html::testPlaceCursorRightOf().
  },

  /**
   * Tests that the selection is cleared when the dialog opens and is
   * correctly restored after cancel is clicked.
   * @param {boolean=} isNew Whether to test behavior when creating a new
   *     link (cancelling will flatten it).
   * @suppress {visibility} suppression added to enable type checking
   */
  testRestoreSelectionOnCancel(isNew = undefined) {
    setUpAnchor('12345', '/', isNew);
    setUpRealEditableField();

    const elem = fieldObj.getElement();
    const helper = new TestHelper(elem);
    helper.select('12345', 1, '12345', 4);  // Selects '234'.

    assertEquals(
        'Incorrect text selected before dialog is opened', '234',
        fieldObj.getRange().getText());
    plugin.execCommand(Command.MODAL_LINK_EDITOR, linkObj);
    if (!userAgent.IE) {
      // IE returns some bogus range when field doesn't have selection.
      // You can't remove the selection from a whitebox field in Opera.
      assertNull(
          'There should be no selection while dialog is open',
          fieldObj.getRange());
    }
    events.fireClickSequence(plugin.dialog_.getCancelButtonElement());
    assertEquals(
        'Incorrect text selected after clicking cancel', '234',
        fieldObj.getRange().getText());
  },

  /**
   * Tests that the selection is cleared when the dialog opens and is
   * correctly restored after cancel is clicked and the new link is removed.
   */
  testRestoreSelectionOnCancelNew() {
    this.testRestoreSelectionOnCancel(true);
  },

  /**
   * Tests that the BeforeTestLink event is suppressed for invalid url
   * schemes.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testTestLinkDisabledForInvalidScheme() {
    mockAlert(mockmatchers.isString);
    mockCtrl.$replayAll();

    const invalidUrl = 'javascript:document.write(\'hello\');';

    plugin = new LinkDialogPlugin();
    /** @suppress {visibility} suppression added to enable type checking */
    const dialog = plugin.createDialog(new DomHelper(), mockLink);

    // Mock of execCommand + clicking test without actually opening the
    // dialog.
    const dispatched =
        dialog.dispatchEvent(new LinkDialog.BeforeTestLinkEvent(invalidUrl));

    assertFalse(dispatched);
    mockCtrl.$verifyAll();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testIsSafeSchemeToOpen() {
    plugin = new LinkDialogPlugin();
    // Urls with no scheme at all are ok too since 'http://' will be
    // prepended.
    const good = [
      'http://google.com',
      'http://google.com/',
      'https://google.com',
      'null@google.com',
      'http://www.google.com',
      'http://site.com',
      'google.com',
      'google',
      'http://google',
      'HTTP://GOOGLE.COM',
      'HtTp://www.google.com',
    ];

    const bad = [
      'javascript:google.com',
      'httpp://google.com',
      'data:foo',
      'javascript:alert(\'hi\');',
      'abc:def',
    ];

    let i;
    for (i = 0; i < good.length; i++) {
      assertTrue(
          good[i] + ' should have a safe scheme',
          plugin.isSafeSchemeToOpen_(good[i]));
    }

    for (i = 0; i < bad.length; i++) {
      assertFalse(
          bad[i] + ' should have an unsafe scheme',
          plugin.isSafeSchemeToOpen_(bad[i]));
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testShouldOpenWithWhitelist() {
    plugin.setSafeToOpenSchemes(['abc']);

    assertTrue(
        'Scheme should be safe', plugin.shouldOpenUrl('abc://google.com'));
    assertFalse(
        'Scheme should be unsafe', plugin.shouldOpenUrl('http://google.com'));

    plugin.setBlockOpeningUnsafeSchemes(false);
    assertTrue(
        'Non-whitelisted should now be safe after disabling blocking',
        plugin.shouldOpenUrl('http://google.com'));
  },

  /**
   * Regression test for http://b/issue?id=1607766 . Without the fix, this
   * should give an Invalid Argument error in IE, because the editable field
   * caches a selection util that has a reference to the node of the link
   * text before it is edited (which gets replaced by a new node for the new
   * text after editing).
   * @suppress {visibility} suppression added to enable type checking
   */
  testBug1607766() {
    setUpAnchor('abc', 'def');
    setUpRealEditableField();

    const elem = fieldObj.getElement();
    const helper = new TestHelper(elem);
    helper.select('abc', 1, 'abc', 2);  // Selects 'b'.
    // Dispatching a selection event causes the field to cache a selection
    // util, which is the root of the bug.
    plugin.fieldObject.dispatchSelectionChangeEvent();

    plugin.execCommand(Command.MODAL_LINK_EDITOR, linkObj);
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    dom.getElement(LinkDialog.Id_.TEXT_TO_DISPLAY).value = 'Abc';
    events.fireClickSequence(plugin.dialog_.getOkButtonElement());

    // In IE the unit test somehow doesn't cause a browser focus event, so
    // we need to manually invoke this, which is where the bug happens.
    plugin.fieldObject.dispatchFocus_();
  },

  /** Regression test for http://b/issue?id=2215546 . */
  testBug2215546() {
    setUpRealEditableField();

    const elem = fieldObj.getElement();
    fieldObj.setSafeHtml(
        false,
        SafeHtml.create('div', {}, SafeHtml.create('a', {'href': '/'}, '')));
    anchorElem = elem.firstChild.firstChild;
    linkObj = new Link(anchorElem, true);

    const helper = new TestHelper(elem);
    // Select "</a>" in a way, simulating what IE does if you hit enter
    // twice, arrow up into the blank line and open the link dialog.
    helper.select(anchorElem, 0, elem.firstChild, 1);

    plugin.execCommand(Command.MODAL_LINK_EDITOR, linkObj);
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    dom.getElement(LinkDialog.Id_.TEXT_TO_DISPLAY).value = 'foo';
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    dom.getElement(LinkDialog.Id_.ON_WEB_INPUT).value = 'foo';
    /** @suppress {visibility} suppression added to enable type checking */
    const okButton = plugin.dialog_.getOkButtonElement();
    okButton.disabled = false;
    events.fireClickSequence(okButton);

    assertEquals(
        'Link text should have been inserted', 'foo', anchorElem.innerHTML);
  },

  /**
   * Test that link insertion doesn't scroll the field to the top
   * after clicking Cancel or OK.
   */
  testBug7279077ScrollOnFocus() {
    if (userAgent.IE) {
      return;  // TODO(user): take this out once b/7279077 fixed for IE
               // too.
    }
    setUpAnchor('12345', '/');
    setUpRealEditableField();

    // Make the field scrollable and kinda small.
    const elem = fieldObj.getElement();
    elem.style.overflow = 'auto';
    elem.style.height = '40px';
    elem.style.width = '200px';
    elem.style.contenteditable = 'true';

    // Add a bunch of text before the anchor tag.
    const longTextElem = dom.createElement(TagName.SPAN);
    longTextElem.innerHTML = googString.repeat('All work and no play.<p>', 20);
    elem.insertBefore(longTextElem, elem.firstChild);

    const helper = new TestHelper(elem);
    helper.select('12345', 1, '12345', 4);  // Selects '234'.

    // Scroll down.
    elem.scrollTop = 60;

    // Bring up the link insertion dialog, then cancel.
    plugin.execCommand(Command.MODAL_LINK_EDITOR, linkObj);
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    dom.getElement(LinkDialog.Id_.TEXT_TO_DISPLAY).value = 'foo';
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    dom.getElement(LinkDialog.Id_.ON_WEB_INPUT).value = 'foo';
    /** @suppress {visibility} suppression added to enable type checking */
    const cancelButton = plugin.dialog_.getCancelButtonElement();
    events.fireClickSequence(cancelButton);

    assertEquals(
        'Field should not have scrolled after cancel', 60, elem.scrollTop);

    // Now let's try it with clicking the OK button.
    plugin.execCommand(Command.MODAL_LINK_EDITOR, linkObj);
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    dom.getElement(LinkDialog.Id_.TEXT_TO_DISPLAY).value = 'foo';
    /**
     * @suppress {visibility,strictMissingProperties} suppression added to
     * enable type checking
     */
    dom.getElement(LinkDialog.Id_.ON_WEB_INPUT).value = 'foo';
    /** @suppress {visibility} suppression added to enable type checking */
    const okButton = plugin.dialog_.getOkButtonElement();
    events.fireClickSequence(okButton);

    assertEquals('Field should not have scrolled after OK', 60, elem.scrollTop);
  },
});
