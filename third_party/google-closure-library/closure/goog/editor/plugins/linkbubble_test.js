/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.LinkBubbleTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Command = goog.require('goog.editor.Command');
const EventType = goog.require('goog.events.EventType');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const FunctionMock = goog.require('goog.testing.FunctionMock');
const GoogEvent = goog.require('goog.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const Link = goog.require('goog.editor.Link');
const LinkBubble = goog.require('goog.editor.plugins.LinkBubble');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Range = goog.require('goog.dom.Range');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const googString = goog.require('goog.string');
const googWindow = goog.require('goog.window');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let fieldDiv;
let FIELDMOCK;
let linkBubble;
let link;
let linkChild;
let mockWindowOpen;
let stubs;
let testHelper;

function closeBox() {
  const closeBox =
      dom.getElementsByTagNameAndClass(TagName.DIV, 'tr_bubble_closebox');
  assertEquals('Should find only one close box', 1, closeBox.length);
  assertNotNull('Found close box', closeBox[0]);
  events.fireClickSequence(closeBox[0]);
}

/** @suppress {visibility} suppression added to enable type checking */
function assertBubble() {
  assertTrue('Link bubble visible', linkBubble.isVisible());
  assertNotNull('Link bubble created', dom.$(LinkBubble.LINK_DIV_ID_));
}

/** @suppress {visibility} suppression added to enable type checking */
function assertNoBubble() {
  assertFalse('Link bubble not visible', linkBubble.isVisible());
  assertNull('Link bubble not created', dom.$(LinkBubble.LINK_DIV_ID_));
}

/** @suppress {checkTypes} suppression added to enable type checking */
function createMouseEvent(target) {
  const eventObj = new GoogEvent(EventType.MOUSEUP, target);
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  eventObj.button = BrowserEvent.MouseButton.LEFT;

  return new BrowserEvent(eventObj, target);
}

testSuite({
  setUpPage() {
    fieldDiv = dom.$('field');
    stubs = new PropertyReplacer();
    testHelper = new TestHelper(dom.getElement('field'));
  },

  setUp() {
    testHelper.setUpEditableElement();
    FIELDMOCK = new FieldMock();

    linkBubble = new LinkBubble();
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    linkBubble.fieldObject = FIELDMOCK;

    link = fieldDiv.firstChild;
    linkChild = link.lastChild;

    /** @suppress {checkTypes} suppression added to enable type checking */
    mockWindowOpen = new FunctionMock('open');
    stubs.set(googWindow, 'open', mockWindowOpen);
  },

  tearDown() {
    link.removeAttribute('data-dlb');
    linkBubble.closeBubble();
    testHelper.tearDownEditableElement();
    stubs.reset();
  },

  testLinkSelected() {
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);
    Range.createFromNodeContents(link).select();
    linkBubble.handleSelectionChange();
    assertBubble();
    FIELDMOCK.$verify();
  },

  testLinkClicked() {
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);
    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();
    FIELDMOCK.$verify();
  },

  testDisabledLinkClicked() {
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);
    link.setAttribute('data-dlb', '');
    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertNoBubble();
    link.removeAttribute('data-dlb');
    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();
    FIELDMOCK.$verify();
  },

  testImageLink() {
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);
    link.setAttribute('imageanchor', 1);
    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();
    FIELDMOCK.$verify();
  },

  testCloseBox() {
    this.testLinkClicked();
    closeBox();
    assertNoBubble();
    FIELDMOCK.$verify();
  },

  /**
     @suppress {missingProperties,visibility} suppression added to enable type
     checking
   */
  testChangeClicked() {
    FIELDMOCK.execCommand(Command.MODAL_LINK_EDITOR, new Link(link, false));
    FIELDMOCK.$registerArgumentListVerifier(
        'execCommand',
        (arr1, arr2) => arr1.length == arr2.length && arr1.length == 2 &&
            arr1[0] == Command.MODAL_LINK_EDITOR &&
            arr2[0] == Command.MODAL_LINK_EDITOR && arr1[1] instanceof Link &&
            arr2[1] instanceof Link);
    FIELDMOCK.$times(1);
    FIELDMOCK.$returns(true);
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();

    events.fireClickSequence(dom.$(LinkBubble.CHANGE_LINK_ID_));
    assertNoBubble();
    FIELDMOCK.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testChangePressed() {
    FIELDMOCK.execCommand(Command.MODAL_LINK_EDITOR, new Link(link, false));
    FIELDMOCK.$registerArgumentListVerifier(
        'execCommand',
        (arr1, arr2) => arr1.length == arr2.length && arr1.length == 2 &&
            arr1[0] == Command.MODAL_LINK_EDITOR &&
            arr2[0] == Command.MODAL_LINK_EDITOR && arr1[1] instanceof Link &&
            arr2[1] instanceof Link);
    FIELDMOCK.$times(1);
    FIELDMOCK.$returns(true);
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();

    /** @suppress {visibility} suppression added to enable type checking */
    const defaultPrevented = !events.fireKeySequence(
        dom.$(LinkBubble.CHANGE_LINK_ID_), KeyCodes.ENTER);
    assertTrue(defaultPrevented);
    assertNoBubble();
    FIELDMOCK.$verify();
  },

  /**
     @suppress {missingProperties,visibility} suppression added to enable type
     checking
   */
  testDeleteClicked() {
    FIELDMOCK.dispatchBeforeChange();
    FIELDMOCK.$times(1);
    FIELDMOCK.dispatchChange();
    FIELDMOCK.$times(1);
    FIELDMOCK.focus();
    FIELDMOCK.$times(1);
    FIELDMOCK.$replay();

    linkBubble.enable(FIELDMOCK);

    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();

    events.fireClickSequence(dom.$(LinkBubble.DELETE_LINK_ID_));
    const element = userAgent.GECKO ? document.body : fieldDiv;
    assertNotEquals(
        'Link removed', element.firstChild.nodeName, String(TagName.A));
    assertNoBubble();
    const range = Range.createFromWindow();
    assertEquals('Link selection on link text', linkChild, range.getEndNode());
    assertEquals(
        'Link selection on link text end',
        dom.getRawTextContent(linkChild).length, range.getEndOffset());
    FIELDMOCK.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testDeletePressed() {
    FIELDMOCK.dispatchBeforeChange();
    FIELDMOCK.$times(1);
    FIELDMOCK.dispatchChange();
    FIELDMOCK.$times(1);
    FIELDMOCK.focus();
    FIELDMOCK.$times(1);
    FIELDMOCK.$replay();

    linkBubble.enable(FIELDMOCK);

    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();

    /** @suppress {visibility} suppression added to enable type checking */
    const defaultPrevented = !events.fireKeySequence(
        dom.$(LinkBubble.DELETE_LINK_ID_), KeyCodes.ENTER);
    assertTrue(defaultPrevented);
    const element = userAgent.GECKO ? document.body : fieldDiv;
    assertNotEquals(
        'Link removed', element.firstChild.nodeName, String(TagName.A));
    assertNoBubble();
    const range = Range.createFromWindow();
    assertEquals('Link selection on link text', linkChild, range.getEndNode());
    assertEquals(
        'Link selection on link text end',
        dom.getRawTextContent(linkChild).length, range.getEndOffset());
    FIELDMOCK.$verify();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testActionClicked() {
    const SPAN = 'actionSpanId';
    const LINK = 'actionLinkId';
    let toShowCount = 0;
    let actionCount = 0;

    const linkAction = new LinkBubble.Action(
        SPAN, LINK, 'message',
        () => {
          toShowCount++;
          return toShowCount == 1;  // Show it the first time.
        },
        () => {
          actionCount++;
        });

    linkBubble = new LinkBubble(linkAction);
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    linkBubble.fieldObject = FIELDMOCK;
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    // The first time the bubble is shown, show our custom action.
    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();
    assertEquals('Should check showing the action', 1, toShowCount);
    assertEquals('Action should not have fired yet', 0, actionCount);

    assertTrue(
        'Action should be visible 1st time', style.isElementShown(dom.$(SPAN)));
    events.fireClickSequence(dom.$(LINK));

    assertEquals('Should not check showing again yet', 1, toShowCount);
    assertEquals('Action should be fired', 1, actionCount);

    closeBox();
    assertNoBubble();

    // The action won't be shown the second time around.
    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();
    assertEquals('Should check showing again', 2, toShowCount);
    assertEquals('Action should not fire again', 1, actionCount);
    assertFalse(
        'Action should not be shown 2nd time',
        style.isElementShown(dom.$(SPAN)));

    FIELDMOCK.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testLinkTextClicked() {
    mockWindowOpen(
        'http://www.google.com/', {'target': '_blank', 'noreferrer': false},
        window);
    mockWindowOpen.$replay();
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();

    events.fireClickSequence(dom.$(LinkBubble.TEST_LINK_ID_));

    assertBubble();
    mockWindowOpen.$verify();
    FIELDMOCK.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testLinkTextClickedCustomUrlFn() {
    mockWindowOpen(
        'http://images.google.com/', {'target': '_blank', 'noreferrer': false},
        window);
    mockWindowOpen.$replay();
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    linkBubble.setTestLinkUrlFn((url) => url.replace('www', 'images'));

    linkBubble.handleSelectionChange(createMouseEvent(link));
    assertBubble();

    events.fireClickSequence(dom.$(LinkBubble.TEST_LINK_ID_));

    assertBubble();
    mockWindowOpen.$verify();
    FIELDMOCK.$verify();
  },

  /**
   * Urls with invalid schemes shouldn't be linkified.
   * @bug 2585360
   * @suppress {visibility} suppression added to enable type checking
   */
  testDontLinkifyInvalidScheme() {
    mockWindowOpen.$replay();
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    const badLink = dom.createElement(TagName.A);
    badLink.href = 'javascript:alert(1)';
    dom.setTextContent(badLink, 'bad link');

    linkBubble.handleSelectionChange(createMouseEvent(badLink));
    assertBubble();

    // The link shouldn't exist at all
    assertNull(dom.$(LinkBubble.TEST_LINK_ID_));

    assertBubble();
    mockWindowOpen.$verify();
    FIELDMOCK.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testIsSafeSchemeToOpen() {
    // Urls with no scheme at all are ok too since 'http://' will be prepended.
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
          linkBubble.isSafeSchemeToOpen_(good[i]));
    }

    for (i = 0; i < bad.length; i++) {
      assertFalse(
          bad[i] + ' should have an unsafe scheme',
          linkBubble.isSafeSchemeToOpen_(bad[i]));
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testShouldOpenWithWhitelist() {
    linkBubble.setSafeToOpenSchemes(['abc']);

    assertTrue(
        'Scheme should be safe', linkBubble.shouldOpenUrl('abc://google.com'));
    assertFalse(
        'Scheme should be unsafe',
        linkBubble.shouldOpenUrl('http://google.com'));

    linkBubble.setBlockOpeningUnsafeSchemes(false);
    assertTrue(
        'Non-whitelisted should now be safe after disabling blocking',
        linkBubble.shouldOpenUrl('http://google.com'));
  },

  /**
   * @bug 763211
   * @bug 2182147
   */
  testLongUrlTestLinkAnchorTextCorrect() {
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    const longUrl = 'http://www.reallylonglinkthatshouldbetruncated' +
        'becauseitistoolong.com';
    const truncatedLongUrl = googString.truncateMiddle(longUrl, 48);

    const longLink = dom.createElement(TagName.A);
    longLink.href = longUrl;
    dom.setTextContent(longLink, 'Google');
    fieldDiv.appendChild(longLink);

    linkBubble.handleSelectionChange(createMouseEvent(longLink));
    assertBubble();

    /** @suppress {visibility} suppression added to enable type checking */
    const testLinkEl = dom.$(LinkBubble.TEST_LINK_ID_);
    assertEquals(
        'The test link\'s anchor text should be the truncated URL.',
        truncatedLongUrl, testLinkEl.innerHTML);

    fieldDiv.removeChild(longLink);
    FIELDMOCK.$verify();
  },

  /** @bug 2416024 */
  testOverridingCreateBubbleContentsDoesntNpeGetTargetUrl() {
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    stubs.set(
        linkBubble,
        'createBubbleContents', /**
                                   @suppress {visibility} suppression added to
                                   enable type checking
                                 */
        (elem) => {
          // getTargetUrl would cause an NPE if urlUtil_ wasn't defined yet.
          linkBubble.getTargetUrl();
        });
    assertNotThrows(
        'Accessing this.urlUtil_ should not NPE',
        goog.bind(
            linkBubble.handleSelectionChange, linkBubble,
            createMouseEvent(link)));

    FIELDMOCK.$verify();
  },

  /**
   * @bug 15379294
   * @suppress {visibility} suppression added to enable type
   *      checking
   */
  testUpdateLinkCommandDoesNotTriggerAnException() {
    FIELDMOCK.$replay();
    linkBubble.enable(FIELDMOCK);

    // At this point, the bubble was not created yet using its createBubble
    // public method.
    assertNotThrows(
        'Executing goog.editor.Command.UPDATE_LINK_BUBBLE should not trigger ' +
            'an exception even if the bubble was not created yet using its ' +
            'createBubble method.',
        goog.bind(
            linkBubble.execCommandInternal, linkBubble,
            Command.UPDATE_LINK_BUBBLE));

    FIELDMOCK.$verify();
  },
});
