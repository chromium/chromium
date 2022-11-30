/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.editor.LinkDialogTest');
goog.setTestOnly();

const AbstractDialog = goog.require('goog.ui.editor.AbstractDialog');
const ArgumentMatcher = goog.require('goog.testing.mockmatchers.ArgumentMatcher');
const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const DomHelper = goog.require('goog.dom.DomHelper');
const EventHandler = goog.require('goog.events.EventHandler');
const EventType = goog.require('goog.events.EventType');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const Link = goog.require('goog.editor.Link');
const LinkDialog = goog.require('goog.ui.editor.LinkDialog');
const MockControl = goog.require('goog.testing.MockControl');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const asserts = goog.require('goog.testing.asserts');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googWindow = goog.require('goog.window');
const messages = goog.require('goog.ui.editor.messages');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

let dialog;
let mockCtrl;
let mockLink;
let mockOkHandler;
let mockGetViewportSize;
let mockWindowOpen;
let isNew;
let anchorElem;
const stubs = new PropertyReplacer();

const ANCHOR_TEXT = 'anchor text';
const ANCHOR_URL = 'http://www.google.com/';
const ANCHOR_EMAIL = 'm@r.cos';
const ANCHOR_MAILTO = `mailto:${ANCHOR_EMAIL}`;

function setUpAnchor(href, text, opt_isNew, opt_target, opt_rel) {
  anchorElem.href = href;
  anchorElem.innerHTML = text;
  isNew = !!opt_isNew;
  if (opt_target) {
    anchorElem.target = opt_target;
  }
  if (opt_rel) {
    anchorElem.rel = opt_rel;
  }
}

/**
 * Creates and shows the dialog to be tested.
 * @param {Document=} document Document to render the dialog into. Defaults to
 *     the main window's document.
 * @param {boolean=} openInNewWindow Whether the open in new window checkbox
 *     should be shown.
 * @param {boolean=} noFollow Whether rel=nofollow checkbox should be shown.
 * @param {boolean=} opt_focusTextToDisplayOnOpenIfEmpty If passed, will call
 *     focusTextToDisplayOnOpenIfEmpty on the dialog.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createAndShow(
    document = undefined, openInNewWindow = undefined, noFollow = undefined,
    opt_focusTextToDisplayOnOpenIfEmpty) {
  /** @suppress {checkTypes} suppression added to enable type checking */
  dialog = new LinkDialog(new DomHelper(document), mockLink);
  if (openInNewWindow) {
    dialog.showOpenLinkInNewWindow(false);
  }
  if (noFollow) {
    dialog.showRelNoFollow();
  }
  if (opt_focusTextToDisplayOnOpenIfEmpty) {
    dialog.focusTextToDisplayOnOpenIfEmpty();
  }
  dialog.addEventListener(AbstractDialog.EventType.OK, mockOkHandler);
  dialog.show();
}

/**
 * Sets up the mock event handler to expect an OK event with the given text
 * and url.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function expectOk(linkText, linkUrl, opt_openInNewWindow, opt_noFollow) {
  mockOkHandler.handleEvent(new ArgumentMatcher(
      (arg) => arg.type == AbstractDialog.EventType.OK &&
          arg.linkText == linkText && arg.linkUrl == linkUrl &&
          arg.openInNewWindow == !!opt_openInNewWindow &&
          arg.noFollow == !!opt_noFollow,
      `{linkText: ${linkText}, linkUrl: ${linkUrl}` +
          ', openInNewWindow: ' + opt_openInNewWindow +
          ', noFollow: ' + opt_noFollow + '}'));
}

/**
 * Return true if we should use active element in our tests.
 * @return {boolean} .
 * @suppress {checkTypes} suppression added to enable type checking
 */
function useActiveElement() {
  return BrowserFeature.HAS_ACTIVE_ELEMENT ||
      userAgent.WEBKIT && userAgent.isVersionOrHigher(9);
}

/** @suppress {visibility} suppression added to enable type checking */
function getDisplayInput() {
  return dialog.dom.getElement(LinkDialog.Id_.TEXT_TO_DISPLAY);
}

function getDisplayInputText() {
  return getDisplayInput().value;
}

function setDisplayInputText(text) {
  const textInput = getDisplayInput();
  textInput.value = text;
  // Fire event so that dialog behaves like when user types.
  fireInputEvent(textInput, KeyCodes.M);
}

function getUrlInput() {
  /** @suppress {visibility} suppression added to enable type checking */
  const elt = dialog.dom.getElement(LinkDialog.Id_.ON_WEB_INPUT);
  assertNotNullNorUndefined('UrlInput must be found', elt);
  return elt;
}

function getUrlInputText() {
  return getUrlInput().value;
}

/** @suppress {visibility} suppression added to enable type checking */
function setUrlInputText(text) {
  const urlInput = getUrlInput();
  urlInput.value = text;
  // Fire event so that dialog behaves like when user types.
  fireInputEvent(dialog.urlInputHandler_, KeyCodes.M);
}

function getEmailInput() {
  /** @suppress {visibility} suppression added to enable type checking */
  const elt = dialog.dom.getElement(LinkDialog.Id_.EMAIL_ADDRESS_INPUT);
  assertNotNullNorUndefined('EmailInput must be found', elt);
  return elt;
}

function getEmailInputText() {
  return getEmailInput().value;
}

/** @suppress {visibility} suppression added to enable type checking */
function setEmailInputText(text) {
  const emailInput = getEmailInput();
  emailInput.value = text;
  // Fire event so that dialog behaves like when user types.
  fireInputEvent(dialog.emailInputHandler_, KeyCodes.M);
}

/** @suppress {visibility} suppression added to enable type checking */
function getOpenInNewWindowCheckboxChecked() {
  return dialog.openInNewWindowCheckbox_.checked;
}

function setOpenInNewWindowCheckboxChecked(checked) {
  /** @suppress {visibility} suppression added to enable type checking */
  dialog.openInNewWindowCheckbox_.checked = checked;
}

function fireInputEvent(input, keyCode) {
  const inputEvent = new GoogTestingEvent(EventType.INPUT, input);
  inputEvent.keyCode = keyCode;
  inputEvent.charCode = keyCode;
  testingEvents.fireBrowserEvent(inputEvent);
}
testSuite({
  /** @suppress {missingProperties} suppression added to enable type checking */
  setUp() {
    anchorElem = dom.createElement(TagName.A);
    dom.appendChild(dom.getDocument().body, anchorElem);

    mockCtrl = new MockControl();
    mockLink = mockCtrl.createLooseMock(Link);
    mockOkHandler = mockCtrl.createLooseMock(EventHandler);

    isNew = false;
    mockLink.isNew();
    mockLink.$anyTimes();
    mockLink.$does(() => isNew);
    mockLink.getCurrentText();
    mockLink.$anyTimes();
    mockLink.$does(() => anchorElem.innerHTML);
    mockLink.setTextAndUrl(mockmatchers.isString, mockmatchers.isString);
    mockLink.$anyTimes();
    mockLink.$does((text, url) => {
      anchorElem.innerHTML = text;
      anchorElem.href = url;
    });
    mockLink.$registerArgumentListVerifier('placeCursorRightOf', () => true);
    mockLink.placeCursorRightOf(mockmatchers.iBoolean);
    mockLink.$anyTimes();
    mockLink.getAnchor();
    mockLink.$anyTimes();
    mockLink.$returns(anchorElem);

    mockWindowOpen = mockCtrl.createFunctionMock('open');
    stubs.set(googWindow, 'open', mockWindowOpen);
  },

  tearDown() {
    dialog.dispose();
    dom.removeNode(anchorElem);
    stubs.reset();
  },

  /**
   * Tests that when you show the dialog for a new link, you can switch
   * to the URL view.
   * @param {Document=} document Document to render the dialog into. Defaults to
   *     the main window's document.
   * @suppress {visibility} suppression added to enable type checking
   */
  testShowNewLinkSwitchToUrl(document = undefined) {
    mockCtrl.$replayAll();
    setUpAnchor('', '', true);  // Must be done before creating the dialog.
    createAndShow(document);

    /** @suppress {visibility} suppression added to enable type checking */
    const webRadio =
        dialog.dom.getElement(LinkDialog.Id_.ON_WEB_TAB).firstChild;
    /** @suppress {visibility} suppression added to enable type checking */
    const emailRadio =
        dialog.dom.getElement(LinkDialog.Id_.EMAIL_ADDRESS_TAB).firstChild;
    assertTrue('Web Radio Button selected', webRadio.checked);
    assertFalse('Email Radio Button selected', emailRadio.checked);
    if (useActiveElement()) {
      assertEquals(
          'Focus should be on url input', getUrlInput(),
          dialog.dom.getActiveElement());
    }

    emailRadio.click();
    assertFalse('Web Radio Button selected', webRadio.checked);
    assertTrue('Email Radio Button selected', emailRadio.checked);
    if (useActiveElement()) {
      assertEquals(
          'Focus should be on url input', getEmailInput(),
          dialog.dom.getActiveElement());
    }

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that when you show the dialog for a new link, the input fields are
   * empty, the web tab is selected and focus is in the url input field.
   * @param {Document=} document Document to render the dialog into. Defaults to
   *     the main window's document.
   * @suppress {visibility,checkTypes,missingProperties} suppression added to
   * enable type checking
   */
  testShowForNewLink(document = undefined) {
    mockCtrl.$replayAll();
    setUpAnchor('', '', true);  // Must be done before creating the dialog.
    createAndShow(document);

    assertEquals(
        'Display text input field should be empty', '', getDisplayInputText());
    assertEquals('Url input field should be empty', '', getUrlInputText());
    assertEquals(
        'On the web tab should be selected', LinkDialog.Id_.ON_WEB,
        dialog.curTabId_);
    if (useActiveElement()) {
      assertEquals(
          'Focus should be on url input', getUrlInput(),
          dialog.dom.getActiveElement());
    }

    mockCtrl.$verifyAll();
  },

  /**
   * Fakes that the mock field is using an iframe and does the same test as
   * testShowForNewLink().
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  testShowForNewLinkWithDiffAppWindow() {
    this.testShowForNewLink(dom.getElement('appWindowIframe').contentDocument);
  },

  /**
   * Tests that when you show the dialog for a url link, the input fields are
   * filled in, the web tab is selected and focus is in the url input field.
   * @suppress {visibility,checkTypes,missingProperties} suppression added to
   * enable type checking
   */
  testShowForUrlLink() {
    mockCtrl.$replayAll();
    setUpAnchor(ANCHOR_URL, ANCHOR_TEXT);
    createAndShow();

    assertEquals(
        'Display text input field should be filled in', ANCHOR_TEXT,
        getDisplayInputText());
    assertEquals(
        'Url input field should be filled in', ANCHOR_URL, getUrlInputText());
    assertEquals(
        'On the web tab should be selected', LinkDialog.Id_.ON_WEB,
        dialog.curTabId_);
    if (useActiveElement()) {
      assertEquals(
          'Focus should be on url input', getUrlInput(),
          dialog.dom.getActiveElement());
    }

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that when you show the dialog for a mailto link, the input fields are
   * filled in, the email tab is selected and focus is in the email input field.
   * @suppress {visibility,checkTypes,missingProperties} suppression added to
   * enable type checking
   */
  testShowForMailtoLink() {
    mockCtrl.$replayAll();
    setUpAnchor(ANCHOR_MAILTO, ANCHOR_TEXT);
    createAndShow();

    assertEquals(
        'Display text input field should be filled in', ANCHOR_TEXT,
        getDisplayInputText());
    assertEquals(
        'Email input field should be filled in',
        ANCHOR_EMAIL,  // The 'mailto:' is not in the input!
        getEmailInputText());
    assertEquals(
        'Email tab should be selected', LinkDialog.Id_.EMAIL_ADDRESS,
        dialog.curTabId_);
    if (useActiveElement()) {
      assertEquals(
          'Focus should be on email input', getEmailInput(),
          dialog.dom.getActiveElement());
    }

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that when you show the dialog for a new link, if the text to display
   * is empty and focusTextToDisplayOnOpenIfEmpty is set, the input fields are
   * empty, the web tab is selected and focus is in the text to display input
   * field.
   * @param {Document=} document Document to render the dialog into. Defaults to
   *     the main window's document.
   * @suppress {visibility,checkTypes,missingProperties} suppression added to
   * enable type checking
   */
  testShowForNewLink_focusTextToDisplayOnOpenIfEmpty(document = undefined) {
    mockCtrl.$replayAll();
    setUpAnchor('', '', true);  // Must be done before creating the dialog.
    createAndShow(
        document, undefined /* opt_openInNewWindow */,
        undefined /*  opt_noFollow */,
        true /* opt_focusTextToDisplayOnOpenIfEmpty */);

    assertEquals(
        'Display text input field should be empty', '', getDisplayInputText());
    assertEquals('Url input field should be empty', '', getUrlInputText());
    assertEquals(
        'On the web tab should be selected', LinkDialog.Id_.ON_WEB,
        dialog.curTabId_);
    if (useActiveElement()) {
      assertEquals(
          'Focus should be on text to display input', getDisplayInput(),
          dialog.dom.getActiveElement());
    }

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that when focusTextToDisplayOnOpenIfEmpty is set, if the display text
   * is not empty when you show the dialog for a url link, the display text
   * input is filled in, the web tab is selected and focus is in the url input
   * field.
   * @param {Document=} document Document to render the dialog into. Defaults to
   *     the main window's document.
   * @suppress {visibility,checkTypes,missingProperties} suppression added to
   * enable type checking
   */
  testShowForUrlLink_focusTextToDisplayOnOpenIfEmpty(document = undefined) {
    mockCtrl.$replayAll();
    setUpAnchor('', ANCHOR_TEXT);
    createAndShow(
        document /* opt_document */, undefined /* opt_openInNewWindow */,
        undefined /*  opt_noFollow */,
        true /* opt_focusTextToDisplayOnOpenIfEmpty */);

    assertEquals(
        'Display text input field should be filled in', ANCHOR_TEXT,
        getDisplayInputText());
    assertEquals(
        'On the web tab should be selected', LinkDialog.Id_.ON_WEB,
        dialog.curTabId_);
    if (useActiveElement()) {
      assertEquals(
          'Focus should be on url input', getUrlInput(),
          dialog.dom.getActiveElement());
    }

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that when focusTextToDisplayOnOpenIfEmpty is set, if the display text
   * is not empty when you show the dialog for a mailto link, the display text
   * input is filled in, the email tab is selected and focus is in the email
   * input field.
   * @param {Document=} document Document to render the dialog into. Defaults to
   *     the main window's document.
   * @suppress {visibility,checkTypes,missingProperties} suppression added to
   * enable type checking
   */
  testShowForMailtoLink_focusTextToDisplayOnOpenIfEmpty(document = undefined) {
    mockCtrl.$replayAll();
    setUpAnchor(ANCHOR_MAILTO, ANCHOR_TEXT);
    createAndShow(
        document /* opt_document */, undefined /* opt_openInNewWindow */,
        undefined /*  opt_noFollow */,
        true /* opt_focusTextToDisplayOnOpenIfEmpty */);

    assertEquals(
        'Display text input field should be filled in', ANCHOR_TEXT,
        getDisplayInputText());
    assertEquals(
        'Email tab should be selected', LinkDialog.Id_.EMAIL_ADDRESS,
        dialog.curTabId_);
    if (useActiveElement()) {
      assertEquals(
          'Focus should be on email input', getEmailInput(),
          dialog.dom.getActiveElement());
    }

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that the display text is autogenerated from the url input in the
   * right situations (and not generated when appropriate too).
   */
  testAutogeneration() {
    mockCtrl.$replayAll();
    setUpAnchor('', '', true);
    createAndShow();

    // Simulate typing a url when everything is empty, should autogen.
    setUrlInputText(ANCHOR_URL);
    assertEquals(
        'Display text should have been autogenerated', ANCHOR_URL,
        getDisplayInputText());

    // Simulate typing text when url is set, afterwards should not autogen.
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_MAILTO);
    assertNotEquals(
        'Display text should not have been autogenerated', ANCHOR_MAILTO,
        getDisplayInputText());
    assertEquals(
        'Display text should have remained the same', ANCHOR_TEXT,
        getDisplayInputText());

    // Simulate typing text equal to existing url, afterwards should autogen.
    setDisplayInputText(ANCHOR_MAILTO);
    setUrlInputText(ANCHOR_URL);
    assertEquals(
        'Display text should have been autogenerated', ANCHOR_URL,
        getDisplayInputText());

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that the display text is not autogenerated from the url input in all
   * situations when the autogeneration feature is turned off.
   */
  testAutogenerationOff() {
    mockCtrl.$replayAll();

    setUpAnchor('', '', true);
    createAndShow();

    // Disable the autogen feature
    dialog.setAutogenFeatureEnabled(false);

    // Simulate typing a url when everything is empty, should not autogen.
    setUrlInputText(ANCHOR_URL);
    assertEquals(
        'Display text should not have been autogenerated', '',
        getDisplayInputText());

    // Simulate typing text when url is set, afterwards should not autogen.
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_MAILTO);
    assertNotEquals(
        'Display text should not have been autogenerated', ANCHOR_MAILTO,
        getDisplayInputText());
    assertEquals(
        'Display text should have remained the same', ANCHOR_TEXT,
        getDisplayInputText());

    // Simulate typing text equal to existing url, afterwards should not
    // autogen.
    setDisplayInputText(ANCHOR_MAILTO);
    setUrlInputText(ANCHOR_URL);
    assertEquals(
        'Display text should not have been autogenerated', ANCHOR_MAILTO,
        getDisplayInputText());

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that clicking OK with the url tab selected dispatches an event with
   * the proper link data.
   * @suppress {visibility} suppression added to enable type checking
   */
  testOkForUrl() {
    expectOk(ANCHOR_TEXT, ANCHOR_URL);
    mockCtrl.$replayAll();
    setUpAnchor('', '', true);
    createAndShow();

    dialog.tabPane_.setSelectedTabId(LinkDialog.Id_.ON_WEB_TAB);
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_URL);
    testingEvents.fireClickSequence(dialog.getOkButtonElement());

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that clicking OK with the url tab selected but with an email address
   * in the url field dispatches an event with the proper link data.
   * @suppress {visibility} suppression added to enable type checking
   */
  testOkForUrlWithEmail() {
    expectOk(ANCHOR_TEXT, ANCHOR_MAILTO);
    mockCtrl.$replayAll();
    setUpAnchor('', '', true);
    createAndShow();

    dialog.tabPane_.setSelectedTabId(LinkDialog.Id_.ON_WEB_TAB);
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_EMAIL);
    testingEvents.fireClickSequence(dialog.getOkButtonElement());

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that clicking OK with the email tab selected dispatches an event with
   * the proper link data.
   * @suppress {visibility} suppression added to enable type checking
   */
  testOkForEmail() {
    expectOk(ANCHOR_TEXT, ANCHOR_MAILTO);
    mockCtrl.$replayAll();
    setUpAnchor('', '', true);
    createAndShow();

    dialog.tabPane_.setSelectedTabId(LinkDialog.Id_.EMAIL_ADDRESS_TAB);

    setDisplayInputText(ANCHOR_TEXT);
    setEmailInputText(ANCHOR_EMAIL);
    testingEvents.fireClickSequence(dialog.getOkButtonElement());

    mockCtrl.$verifyAll();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testOpenLinkInNewWindowNewLink() {
    expectOk(ANCHOR_TEXT, ANCHOR_URL, true);
    expectOk(ANCHOR_TEXT, ANCHOR_URL, false);
    mockCtrl.$replayAll();

    setUpAnchor('', '', true);
    createAndShow(undefined, true);
    dialog.tabPane_.setSelectedTabId(LinkDialog.Id_.ON_WEB_TAB);
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_URL);

    assertFalse(
        '"Open in new window" should start unchecked',
        getOpenInNewWindowCheckboxChecked());
    setOpenInNewWindowCheckboxChecked(true);
    assertTrue(
        '"Open in new window" should have gotten checked',
        getOpenInNewWindowCheckboxChecked());
    testingEvents.fireClickSequence(dialog.getOkButtonElement());

    // Reopen same dialog
    dialog.show();
    dialog.tabPane_.setSelectedTabId(LinkDialog.Id_.ON_WEB_TAB);
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_URL);

    assertTrue(
        '"Open in new window" should remember it was checked',
        getOpenInNewWindowCheckboxChecked());
    setOpenInNewWindowCheckboxChecked(false);
    assertFalse(
        '"Open in new window" should have gotten unchecked',
        getOpenInNewWindowCheckboxChecked());
    testingEvents.fireClickSequence(dialog.getOkButtonElement());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testOpenLinkInNewWindowExistingLink() {
    mockCtrl.$replayAll();

    // Edit an existing link that already opens in a new window.
    setUpAnchor('', '', false, '_blank');
    createAndShow(undefined, true);
    dialog.tabPane_.setSelectedTabId(LinkDialog.Id_.ON_WEB_TAB);
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_URL);

    assertTrue(
        '"Open in new window" should start checked for existing link',
        getOpenInNewWindowCheckboxChecked());

    mockCtrl.$verifyAll();
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testRelNoFollowNewLink() {
    expectOk(ANCHOR_TEXT, ANCHOR_URL, null, true);
    expectOk(ANCHOR_TEXT, ANCHOR_URL, null, false);
    mockCtrl.$replayAll();

    setUpAnchor('', '', true, true);
    createAndShow(null, null, true);
    dialog.tabPane_.setSelectedTabId(LinkDialog.Id_.ON_WEB_TAB);
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_URL);
    assertFalse(
        'rel=nofollow should start unchecked',
        dialog.relNoFollowCheckbox_.checked);

    // Check rel=nofollow and close the dialog.
    /** @suppress {visibility} suppression added to enable type checking */
    dialog.relNoFollowCheckbox_.checked = true;
    testingEvents.fireClickSequence(dialog.getOkButtonElement());

    // Reopen the same dialog.
    anchorElem.rel = 'foo nofollow bar';
    dialog.show();
    dialog.tabPane_.setSelectedTabId(LinkDialog.Id_.ON_WEB_TAB);
    setDisplayInputText(ANCHOR_TEXT);
    setUrlInputText(ANCHOR_URL);
    assertTrue(
        'rel=nofollow should start checked when reopening the dialog',
        dialog.relNoFollowCheckbox_.checked);
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testRelNoFollowExistingLink() {
    mockCtrl.$replayAll();

    setUpAnchor('', '', null, null, 'foo nofollow bar');
    createAndShow(null, null, true);
    assertTrue(
        'rel=nofollow should start checked for existing link',
        dialog.relNoFollowCheckbox_.checked);

    mockCtrl.$verifyAll();
  },

  /**
   * Test that clicking on the test button opens a new window with the correct
   * options.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testWebTestButton() {
    if (userAgent.GECKO) {
      // TODO(robbyw): Figure out why this is flaky and fix it.
      return;
    }

    let height;
    let width;

    mockWindowOpen(
        ANCHOR_URL,
        new ArgumentMatcher(
            (options) => !asserts.findDifferences(options, {
              target: '_blank',
              width: width,
              height: height,
              toolbar: true,
              scrollbars: true,
              location: true,
              statusbar: false,
              menubar: true,
              resizable: true,
              noreferrer: false,
              noopener: false,
            }),
            '2nd arg: window.open() options'),
        window);

    mockCtrl.$replayAll();
    setUpAnchor(ANCHOR_URL, ANCHOR_TEXT);
    createAndShow();

    // Measure viewport after opening dialog because that might cause scrollbars
    // to appear and reduce the viewport size.
    const size = dom.getViewportSize(window);
    width = Math.max(size.width - 50, 50);
    height = Math.max(size.height - 50, 50);

    /** @suppress {visibility} suppression added to enable type checking */
    const testLink = testingDom.findTextNode(
        messages.MSG_TEST_THIS_LINK, dialog.dialogInternal_.getElement());
    testingEvents.fireClickSequence(testLink.parentNode);

    mockCtrl.$verifyAll();
  },

  /**
   * Test that clicking on the test button does not open a new window when
   * the event is canceled.
   */
  testWebTestButtonPreventDefault() {
    mockCtrl.$replayAll();
    setUpAnchor(ANCHOR_URL, ANCHOR_TEXT);
    createAndShow();

    events.listen(dialog, LinkDialog.EventType.BEFORE_TEST_LINK, (e) => {
      assertEquals(e.url, ANCHOR_URL);
      e.preventDefault();
    });
    /** @suppress {visibility} suppression added to enable type checking */
    const testLink = testingDom.findTextNode(
        messages.MSG_TEST_THIS_LINK, dialog.dialogInternal_.getElement());
    testingEvents.fireClickSequence(testLink.parentNode);

    mockCtrl.$verifyAll();
  },

  /**
   * Test that the setTextToDisplayVisible() correctly works.
   * options.
   * @suppress {visibility} suppression added to enable type checking
   */
  testSetTextToDisplayVisible() {
    mockCtrl.$replayAll();
    setUpAnchor('', '', true);
    createAndShow();

    assertNotEquals(
        'none', style.getStyle(dialog.textToDisplayDiv_, 'display'));
    dialog.setTextToDisplayVisible(false);
    assertEquals('none', style.getStyle(dialog.textToDisplayDiv_, 'display'));
    dialog.setTextToDisplayVisible(true);
    assertNotEquals(
        'none', style.getStyle(dialog.textToDisplayDiv_, 'display'));

    mockCtrl.$verifyAll();
  },
});
