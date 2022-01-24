/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.BasicTextFormatterTest');
goog.setTestOnly();

const BasicTextFormatter = goog.require('goog.editor.plugins.BasicTextFormatter');
const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const Command = goog.require('goog.editor.Command');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const Field = goog.require('goog.editor.Field');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const LooseMock = goog.require('goog.testing.LooseMock');
const Plugin = goog.require('goog.editor.Plugin');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Range = goog.require('goog.dom.Range');
const SafeHtml = goog.require('goog.html.SafeHtml');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const product = goog.require('goog.userAgent.product');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let stubs;

let SAVED_HTML;
let FIELDMOCK;
let FORMATTER;
let ROOT;
let HELPER;
let OPEN_SUB;
let CLOSE_SUB;
let OPEN_SUPER;
let CLOSE_SUPER;
const MOCK_BLOCKQUOTE_STYLE = 'border-left: 1px solid gray;';
class MOCK_GET_BLOCKQUOTE_STYLES {
  /** @suppress {checkTypes} suppression added to enable type checking */
  constructor() {
    return MOCK_BLOCKQUOTE_STYLE;
  }
}

let REAL_FIELD;
let REAL_PLUGIN;

let expectedFailures;

function setUpRealField() {
  REAL_FIELD = new Field('real-field');
  REAL_PLUGIN = new BasicTextFormatter();
  REAL_FIELD.registerPlugin(REAL_PLUGIN);
  REAL_FIELD.makeEditable();
}

function setUpRealFieldIframe() {
  /** @suppress {const} suppression added to enable type checking */
  REAL_FIELD = new Field('iframe');
  FORMATTER = new BasicTextFormatter();
  REAL_FIELD.registerPlugin(FORMATTER);
  REAL_FIELD.makeEditable();
}

function selectRealField() {
  Range.createFromNodeContents(REAL_FIELD.getElement()).select();
  REAL_FIELD.dispatchSelectionChangeEvent();
}

/** @suppress {missingProperties} suppression added to enable type checking */
function setUpListAndBlockquoteTests() {
  const htmlDiv = document.getElementById('html');
  HELPER = new TestHelper(htmlDiv);
  HELPER.setUpEditableElement();

  FIELDMOCK.getElement();
  FIELDMOCK.$anyTimes();
  FIELDMOCK.$returns(htmlDiv);
}

function tearDownHelper() {
  HELPER.tearDownEditableElement();
  HELPER.dispose();
  /** @suppress {const} suppression added to enable type checking */
  HELPER = null;
}

function tearDownListAndBlockquoteTests() {
  tearDownHelper();
}

function setUpSubSuperTests() {
  dom.setTextContent(ROOT, '12345');
  /** @suppress {const} suppression added to enable type checking */
  HELPER = new TestHelper(ROOT);
  HELPER.setUpEditableElement();
}

function tearDownSubSuperTests() {
  tearDownHelper();
}

/** @suppress {missingProperties} suppression added to enable type checking */
function setUpLinkTests(text, url, isEditable) {
  stubs.set(window, 'prompt', () => url);

  ROOT.innerHTML = text;
  /** @suppress {const} suppression added to enable type checking */
  HELPER = new TestHelper(ROOT);
  if (isEditable) {
    HELPER.setUpEditableElement();
    FIELDMOCK.execCommand(Command.MODAL_LINK_EDITOR, mockmatchers.isObject)
        .$returns(undefined);
    let focusField = () => {
      throw 'Field should not be re-focused';
    };
  }

  FIELDMOCK.getElement().$anyTimes().$returns(ROOT);

  FIELDMOCK.setModalMode(true);

  FIELDMOCK.isSelectionEditable().$anyTimes().$returns(isEditable);
}

function tearDownLinkTests() {
  tearDownHelper();
}

/**
 * @suppress {checkTypes,missingProperties} suppression added to enable type
 * checking
 */
function setUpJustifyTests(html) {
  ROOT.innerHTML = html;
  /** @suppress {const} suppression added to enable type checking */
  HELPER = new TestHelper(ROOT);
  HELPER.setUpEditableElement(ROOT);

  FIELDMOCK.getElement();
  FIELDMOCK.$anyTimes();
  FIELDMOCK.$returns(ROOT);

  FIELDMOCK.getElement();
  FIELDMOCK.$anyTimes();
  FIELDMOCK.$returns(ROOT);
}

function tearDownJustifyTests() {
  tearDownHelper();
}

let isFontSizeTest = false;
let defaultFontSizeMap;

/** @suppress {missingProperties} suppression added to enable type checking */
function setUpFontSizeTests() {
  isFontSizeTest = true;
  ROOT.innerHTML = '1<span style="font-size:2px">23</span>4' +
      '<span style="font-size:5px; white-space:pre">56</span>7';
  /** @suppress {const} suppression added to enable type checking */
  HELPER = new TestHelper(ROOT);
  HELPER.setUpEditableElement();
  FIELDMOCK.getElement().$returns(ROOT).$anyTimes();

  // Map representing the sizes of the text in the HTML snippet used in these
  // tests. The key is the exact text content of each text node, and the value
  // is the initial size of the font in pixels. Since tests may cause a text
  // node to be split in two, this also contains keys that initially don't
  // match any text node, but may match one later if an existing node is
  // split. The value for these keys is null, signifying no text node should
  // exist with that content.
  defaultFontSizeMap = {
    '1': 16,
    '2': null,
    '23': 2,
    '3': null,
    '4': 16,
    '5': null,
    '56': 5,
    '6': null,
    '7': 16,
  };
  assertFontSizes('Assertion failed on default font sizes!', {});
}

function tearDownFontSizeTests() {
  if (isFontSizeTest) {
    tearDownHelper();
    isFontSizeTest = false;
  }
}

/**
 * Asserts that the text nodes set up by setUpFontSizeTests() have had their
 * font sizes changed as described by sizeChangesMap.
 * @param {string} msg Assertion error message.
 * @param {Object<string,?number>} sizeChangesMap Maps the text content of a
 *     text node to be measured to its expected font size in pixels, or null if
 *     that text node should not be present in the document (i.e. because it was
 *     split into two). Only the text nodes that have changed from their default
 *     need to be specified.
 */
function assertFontSizes(msg, sizeChangesMap) {
  googObject.extend(defaultFontSizeMap, sizeChangesMap);
  for (let k in defaultFontSizeMap) {
    const node = HELPER.findTextNode(k);
    const expected = defaultFontSizeMap[k];
    if (expected) {
      assertNotNull(`${msg} [couldn't find text node "${k}"]`, node);
      assertEquals(
          `${msg} [incorrect font size for "${k}"]`, expected,
          style.getFontSize(node.parentNode));
    } else {
      assertNull(`${msg} [unexpected text node "${k}"]`, node);
    }
  }
}

/**
 * Helper to make sure the precondition that executing the font size command
 * wraps the content in font tags instead of modifying the style attribute is
 * maintained by the browser even if the selection is already text that is
 * wrapped in a tag with a font size style. We test this with several
 * permutations of how the selection looks: selecting the text in the text
 * node, selecting the whole text node as a unit, or selecting the whole span
 * node as a unit. Sometimes the browser wraps the text node with the font
 * tag, sometimes it wraps the span with the font tag. Either one is ok as
 * long as a font tag is actually being used instead of just modifying the
 * span's style, because our fix for {@bug 1286408} would remove that style.
 * @param {function()} doSelect Function to select the "23" text in the test
 *     content.
 * @suppress {visibility} suppression added to enable type checking
 */
function doTestFontSizeStyledSpan(doSelect) {
  // Make sure no new browsers start getting this bad behavior. If they do,
  // this test will unexpectedly pass.
  expectedFailures.expectFailureFor(
      !BrowserFeature.DOESNT_OVERRIDE_FONT_SIZE_IN_STYLE_ATTR);

  try {
    setUpFontSizeTests();
    FIELDMOCK.$replay();

    doSelect();
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.FONT_SIZE, 7);
    const parentNode = HELPER.findTextNode('23').parentNode;
    const grandparentNode = parentNode.parentNode;
    const fontNode =
        dom.getElementsByTagNameAndClass(TagName.FONT, undefined, ROOT)[0];
    const spanNode =
        dom.getElementsByTagNameAndClass(TagName.SPAN, undefined, ROOT)[0];
    assertTrue(
        'A font tag should have been added either outside or inside' +
            ' the existing span',
        parentNode == spanNode && grandparentNode == fontNode ||
            parentNode == fontNode && grandparentNode == spanNode);

    FIELDMOCK.$verify();
  } catch (e) {
    expectedFailures.handleException(e);
  }
}

/** @suppress {missingProperties} suppression added to enable type checking */
function setUpIframeField(content) {
  const ifr = document.getElementById('iframe');
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  const body = ifr.contentWindow.document.body;
  body.innerHTML = content;

  /** @suppress {const} suppression added to enable type checking */
  HELPER = new TestHelper(body);
  HELPER.setUpEditableElement();
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  FIELDMOCK = new FieldMock(ifr.contentWindow);
  FIELDMOCK.getElement();
  FIELDMOCK.$anyTimes();
  FIELDMOCK.$returns(body);
  FIELDMOCK.queryCommandValue('rtl');
  FIELDMOCK.$anyTimes();
  FIELDMOCK.$returns(null);
  /**
   * @suppress {visibility,checkTypes} suppression added to enable type
   * checking
   */
  FORMATTER.fieldObject = FIELDMOCK;
}

function tearDownIframeField() {
  tearDownHelper();
}

/** @suppress {missingProperties} suppression added to enable type checking */
function setUpConvertBreaksToDivTests() {
  ROOT.innerHTML = '<p>paragraph</p>one<br id="br1">two<br><br><br>three';
  /** @suppress {const} suppression added to enable type checking */
  HELPER = new TestHelper(ROOT);
  HELPER.setUpEditableElement();

  FIELDMOCK.getElement();
  FIELDMOCK.$anyTimes();
  FIELDMOCK.$returns(ROOT);
}

function tearDownConvertBreaksToDivTests() {
  tearDownHelper();
}

/** @bug 2420054 */
const JUSTIFICATION_COMMANDS = [
  BasicTextFormatter.COMMAND.JUSTIFY_LEFT,
  BasicTextFormatter.COMMAND.JUSTIFY_RIGHT,
  BasicTextFormatter.COMMAND.JUSTIFY_CENTER,
  BasicTextFormatter.COMMAND.JUSTIFY_FULL,
];
function doTestIsJustification(command) {
  setUpRealField();
  REAL_FIELD.setSafeHtml(false, SafeHtml.htmlEscape('foo'));
  selectRealField();
  REAL_FIELD.execCommand(command);

  for (let i = 0; i < JUSTIFICATION_COMMANDS.length; i++) {
    if (JUSTIFICATION_COMMANDS[i] == command) {
      assertTrue(
          'queryCommandValue(' + JUSTIFICATION_COMMANDS[i] +
              ') should be true after execCommand(' + command + ')',
          REAL_FIELD.queryCommandValue(JUSTIFICATION_COMMANDS[i]));
    } else {
      assertFalse(
          'queryCommandValue(' + JUSTIFICATION_COMMANDS[i] +
              ') should be false after execCommand(' + command + ')',
          REAL_FIELD.queryCommandValue(JUSTIFICATION_COMMANDS[i]));
    }
  }
}

/** @bug 2472589 */
function doTestIsJustificationPInDiv(useCss, align, command) {
  setUpRealField();
  const attrs = {};
  if (useCss) {
    attrs['style'] = {'text-align': align};
  } else {
    attrs['align'] = align;
  }
  const html = SafeHtml.create('div', attrs, SafeHtml.create('p', {}, 'foo'));

  REAL_FIELD.setSafeHtml(false, html);
  selectRealField();
  assertTrue(
      `P inside ${align} aligned` + (useCss ? ' (using CSS)' : '') +
          ' DIV should be considered ' + align + ' aligned',
      REAL_FIELD.queryCommandValue(command));
}

/**
 * Assert that the prepared contents matches the expected.
 * @suppress {visibility} suppression added to enable type checking
 */
function assertPreparedContents(expected, original) {
  assertEquals(
      expected,
      REAL_FIELD.reduceOp_(Plugin.Op.PREPARE_CONTENTS_HTML, original));
}

/** Assert that sanitization doesn't affect the given text. */
function assertNotPreparedContents(text) {
  assertPreparedContents(text, text);
}

/**
 * Assert that only BR elements expected to persist after convertBreaksToDivs_
 * are in the HTML.
 */
function assertNotBadBrElements(html) {
  if (userAgent.IE) {
    assertNotContains('There should not be <br> elements', '<br', html);
  } else {
    assertFalse(
        'There should not be <br> elements, except ones to prevent ' +
            '<div>s from collapsing' + html,
        /(?!<div>)<br>(?!<\/div>)/.test(html));
  }
}
testSuite({
  /** @suppress {uselessCode} suppression added to enable type checking */
  setUpPage() {
    stubs = new PropertyReplacer();
    SAVED_HTML = dom.getElement('html').innerHTML;
    FIELDMOCK;
    FORMATTER;
    ROOT = dom.getElement('root');
    HELPER;
    OPEN_SUB = '<sub>';
    CLOSE_SUB = '</sub>';
    OPEN_SUPER = '<sup>';
    CLOSE_SUPER = '</sup>';
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    /** @suppress {const} suppression added to enable type checking */
    FIELDMOCK = new FieldMock();

    /** @suppress {const} suppression added to enable type checking */
    FORMATTER = new BasicTextFormatter();
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    FORMATTER.fieldObject = FIELDMOCK;
  },

  tearDown() {
    tearDownFontSizeTests();

    if (REAL_FIELD) {
      REAL_FIELD.makeUneditable();
      REAL_FIELD.dispose();
      /** @suppress {const} suppression added to enable type checking */
      REAL_FIELD = null;
    }

    expectedFailures.handleTearDown();
    stubs.reset();

    dom.getElement('html').innerHTML = SAVED_HTML;
  },

  /**
     @suppress {missingProperties,visibility,strictMissingProperties}
     suppression added to enable type checking
   */
  testIEList() {
    if (userAgent.IE) {
      setUpListAndBlockquoteTests();

      FIELDMOCK.queryCommandValue('rtl').$returns(null);

      FIELDMOCK.$replay();
      const ul = dom.getElement('outerUL');
      Range.createFromNodeContents(dom.getFirstElementChild(ul).firstChild)
          .select();
      FORMATTER.fixIELists_();
      assertFalse('Unordered list must not have ordered type', ul.type == '1');
      const ol = dom.getElement('ol');
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      ol.type = 'disc';
      Range.createFromNodeContents(dom.getFirstElementChild(ul).firstChild)
          .select();
      FORMATTER.fixIELists_();
      assertFalse(
          'Ordered list must not have unordered type', ol.type == 'disc');
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      ol.type = '1';
      Range.createFromNodeContents(dom.getFirstElementChild(ul).firstChild)
          .select();
      FORMATTER.fixIELists_();
      assertTrue('Ordered list must retain ordered list type', ol.type == '1');
      tearDownListAndBlockquoteTests();
    }
  },

  /**
     @suppress {missingProperties,visibility} suppression added to enable type
     checking
   */
  testWebKitList() {
    if (userAgent.WEBKIT) {
      setUpListAndBlockquoteTests();

      FIELDMOCK.queryCommandValue('rtl').$returns(null);

      FIELDMOCK.$replay();
      let ul = document.getElementById('outerUL');
      const html = ul.innerHTML;
      Range.createFromNodeContents(ul).select();

      FORMATTER.fixSafariLists_();
      assertEquals('Contents of UL shouldn\'t change', html, ul.innerHTML);

      ul = document.getElementById('outerUL2');
      Range.createFromNodeContents(ul).select();

      FORMATTER.fixSafariLists_();
      /** @suppress {checkTypes} suppression added to enable type checking */
      const childULs = dom.getElementsByTagName(TagName.UL, ul);
      assertEquals('UL should have one child UL', 1, childULs.length);
      tearDownListAndBlockquoteTests();
    }
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testGeckoListFont() {
    if (userAgent.GECKO) {
      setUpListAndBlockquoteTests();
      FIELDMOCK.queryCommandValue(Command.DEFAULT_TAG).$returns('BR').$times(2);

      FIELDMOCK.$replay();
      const p = dom.getElement('geckolist');
      const font = p.firstChild;
      Range.createFromNodeContents(font).select();
      /** @suppress {visibility} suppression added to enable type checking */
      let retVal = FORMATTER.beforeInsertListGecko_();
      assertFalse('Workaround shouldn\'t be applied when not needed', retVal);

      dom.removeChildren(font);
      Range.createFromNodeContents(font).select();
      /** @suppress {visibility} suppression added to enable type checking */
      retVal = FORMATTER.beforeInsertListGecko_();
      assertTrue('Workaround should be applied when needed', retVal);
      document.execCommand('insertorderedlist', false, true);
      assertTrue(
          'Font should be Courier',
          /courier/i.test(document.queryCommandValue('fontname')));
      tearDownListAndBlockquoteTests();
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSwitchListType() {
    if (!userAgent.WEBKIT) {
      return;
    }
    // Test that we're not seeing https://bugs.webkit.org/show_bug.cgi?id=19539,
    // the type of multi-item lists.
    setUpListAndBlockquoteTests();

    FIELDMOCK.$replay();
    let list = dom.getElement('switchListType');
    const parent = dom.getParentElement(list);

    Range.createFromNodeContents(list).select();
    FORMATTER.execCommandInternal('insertunorderedlist');
    list = dom.getFirstElementChild(parent);
    assertEquals(String(TagName.UL), list.tagName);
    assertEquals(
        3, dom.getElementsByTagNameAndClass(TagName.LI, null, list).length);

    Range.createFromNodeContents(list).select();
    FORMATTER.execCommandInternal('insertorderedlist');
    list = dom.getFirstElementChild(parent);
    assertEquals(String(TagName.OL), list.tagName);
    assertEquals(
        3, dom.getElementsByTagNameAndClass(TagName.LI, null, list).length);

    tearDownListAndBlockquoteTests();
  },

  testIsSilentCommand() {
    const commands = googObject.getValues(BasicTextFormatter.COMMAND);
    const silentCommands = [BasicTextFormatter.COMMAND.CREATE_LINK];

    for (let i = 0; i < commands.length; i += 1) {
      const command = commands[i];
      const shouldBeSilent = googArray.contains(silentCommands, command);
      const isSilent =
          BasicTextFormatter.prototype.isSilentCommand.call(null, command);
      assertEquals(shouldBeSilent, isSilent);
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSubscriptRemovesSuperscript() {
    setUpSubSuperTests();
    FIELDMOCK.$replay();

    HELPER.select('12345', 1, '12345', 4);  // Selects '234'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.SUPERSCRIPT);
    HELPER.assertHtmlMatches(`1${OPEN_SUPER}234${CLOSE_SUPER}5`);
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.SUBSCRIPT);
    HELPER.assertHtmlMatches(`1${OPEN_SUB}234${CLOSE_SUB}5`);

    FIELDMOCK.$verify();
    tearDownSubSuperTests();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSuperscriptRemovesSubscript() {
    setUpSubSuperTests();
    FIELDMOCK.$replay();

    HELPER.select('12345', 1, '12345', 4);  // Selects '234'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.SUBSCRIPT);
    HELPER.assertHtmlMatches(`1${OPEN_SUB}234${CLOSE_SUB}5`);
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.SUPERSCRIPT);
    HELPER.assertHtmlMatches(`1${OPEN_SUPER}234${CLOSE_SUPER}5`);

    FIELDMOCK.$verify();
    tearDownSubSuperTests();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSubscriptRemovesSuperscriptIntersecting() {
    // Tests: 12345 , sup(23) , sub(34) ==> 1+sup(2)+sub(34)+5
    // This is more complex because the sub and sup calls are made on separate
    // fields which intersect each other and queryCommandValue seems to return
    // false if the command is only applied to part of the field.
    setUpSubSuperTests();
    FIELDMOCK.$replay();

    HELPER.select('12345', 1, '12345', 3);  // Selects '23'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.SUPERSCRIPT);
    HELPER.assertHtmlMatches(`1${OPEN_SUPER}23${CLOSE_SUPER}45`);
    HELPER.select('23', 1, '45', 1);  // Selects '34'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.SUBSCRIPT);
    HELPER.assertHtmlMatches(
        `1${OPEN_SUPER}2${CLOSE_SUPER}${OPEN_SUB}34${CLOSE_SUB}5`);

    FIELDMOCK.$verify();
    tearDownSubSuperTests();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSuperscriptRemovesSubscriptIntersecting() {
    // Tests: 12345 , sub(23) , sup(34) ==> 1+sub(2)+sup(34)+5
    setUpSubSuperTests();
    FIELDMOCK.$replay();

    HELPER.select('12345', 1, '12345', 3);  // Selects '23'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.SUBSCRIPT);
    HELPER.assertHtmlMatches(`1${OPEN_SUB}23${CLOSE_SUB}45`);
    HELPER.select('23', 1, '45', 1);  // Selects '34'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.SUPERSCRIPT);
    HELPER.assertHtmlMatches(
        `1${OPEN_SUB}2${CLOSE_SUB}${OPEN_SUPER}34${CLOSE_SUPER}5`);

    FIELDMOCK.$verify();
    tearDownSubSuperTests();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testLink() {
    setUpLinkTests('12345', 'http://www.x.com/', true);
    FIELDMOCK.$replay();

    HELPER.select('12345', 3);
    FORMATTER.execCommandInternal(Command.LINK);
    HELPER.assertHtmlMatches(
        BrowserFeature.GETS_STUCK_IN_LINKS ?
            '123<a href="http://www.x.com/">http://www.x.com/</a>&nbsp;45' :
            '123<a href="http://www.x.com/">http://www.x.com/</a>45');

    FIELDMOCK.$verify();
    tearDownLinkTests();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testLinks() {
    const url1 = 'http://google.com/1';
    const url2 = 'http://google.com/2';
    const dialogUrl = 'http://google.com/3';
    const html = `<p>${url1}</p><p>${url2}</p>`;
    setUpLinkTests(html, dialogUrl, true);
    FIELDMOCK.$replay();

    HELPER.select(url1, 0, url2, url2.length);
    FORMATTER.execCommandInternal(Command.LINK);
    let expectDialogUrl = false;
    if (userAgent.IE || (userAgent.EDGE && !product.isVersion(14))) {
      expectDialogUrl = true;
    }
    HELPER.assertHtmlMatches(
        `<p><a href="${url1}">${url1}</a></p><p>` +
        '<a href="' + dialogUrl + '">' + (expectDialogUrl ? dialogUrl : url2) +
        '</a></p>');
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSelectedLink() {
    setUpLinkTests('12345', 'http://www.x.com/', true);
    FIELDMOCK.$replay();

    HELPER.select('12345', 1, '12345', 4);
    FORMATTER.execCommandInternal(Command.LINK);
    HELPER.assertHtmlMatches(
        BrowserFeature.GETS_STUCK_IN_LINKS ?
            '1<a href="http://www.x.com/">234</a>&nbsp;5' :
            '1<a href="http://www.x.com/">234</a>5');

    FIELDMOCK.$verify();
    tearDownLinkTests();
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testCanceledLink() {
    setUpLinkTests('12345', undefined, true);
    FIELDMOCK.$replay();

    HELPER.select('12345', 1, '12345', 4);
    FORMATTER.execCommandInternal(Command.LINK);
    HELPER.assertHtmlMatches('12345');

    assertEquals('234', FIELDMOCK.getRange().getText());

    FIELDMOCK.$verify();
    tearDownLinkTests();
  },

  /**
     @suppress {missingProperties,visibility} suppression added to enable type
     checking
   */
  testUnfocusedLink() {
    FIELDMOCK.$reset();
    FIELDMOCK.getEditableDomHelper().$anyTimes().$returns(
        dom.getDomHelper(window.document));
    setUpLinkTests('12345', undefined, false);
    FIELDMOCK.getRange().$anyTimes().$returns(null);
    FIELDMOCK.$replay();

    FORMATTER.execCommandInternal(Command.LINK);
    HELPER.assertHtmlMatches('12345');

    FIELDMOCK.$verify();
    tearDownLinkTests();
  },

  /**
     @suppress {missingProperties,visibility} suppression added to enable type
     checking
   */
  testCreateLink() {
    const text = 'some text here';
    const url = 'http://google.com';

    ROOT.innerHTML = text;
    /** @suppress {const} suppression added to enable type checking */
    HELPER = new TestHelper(ROOT);
    HELPER.setUpEditableElement();
    FIELDMOCK.isSelectionEditable().$anyTimes().$returns(true);
    FIELDMOCK.getElement().$anyTimes().$returns(ROOT);
    FIELDMOCK.$replay();

    HELPER.select(text, 0, text, text.length);
    FORMATTER.execCommandInternal(
        BasicTextFormatter.COMMAND.CREATE_LINK, FIELDMOCK.getRange(), url);
    HELPER.assertHtmlMatches(`<a href="${url}">${text}</a>`);

    FIELDMOCK.$verify();
    tearDownLinkTests();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testJustify() {
    setUpJustifyTests('<div>abc</div><p>def</p><div>ghi</div>');
    FIELDMOCK.$replay();

    HELPER.select('abc', 1, 'def', 1);  // Selects 'bcd'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.JUSTIFY_CENTER);
    HELPER.assertHtmlMatches(
        '<div style="text-align: center">abc</div>' +
        '<p style="text-align: center">def</p>' +
        '<div>ghi</div>');

    FIELDMOCK.$verify();
    tearDownJustifyTests();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testJustifyInInline() {
    setUpJustifyTests('<div>a<i>b</i>c</div><div>d</div>');
    FIELDMOCK.$replay();

    HELPER.select('b', 0, 'b', 1);  // Selects 'b'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.JUSTIFY_CENTER);
    HELPER.assertHtmlMatches(
        '<div style="text-align: center">a<i>b</i>c</div><div>d</div>');

    FIELDMOCK.$verify();
    tearDownJustifyTests();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testJustifyInBlock() {
    setUpJustifyTests('<div>a<div>b</div>c</div>');
    FIELDMOCK.$replay();

    HELPER.select('b', 0, 'b', 1);  // Selects 'h'.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.JUSTIFY_CENTER);
    HELPER.assertHtmlMatches(
        '<div>a<div style="text-align: center">b</div>c</div>');

    FIELDMOCK.$verify();
    tearDownJustifyTests();
  },

  /**
   * Regression test for {@bug 1286408}. Tests that when you change the font
   * size of a selection, any font size styles that were nested inside are
   * removed.
   * @suppress {visibility} suppression added to enable type checking
   */
  testFontSizeOverridesStyleAttr() {
    setUpFontSizeTests();
    FIELDMOCK.$replay();

    HELPER.select('1', 0, '4', 1);  // Selects 1234.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.FONT_SIZE, 6);

    assertFontSizes(
        'New font size should override existing font size',
        {'1': 32, '23': 32, '4': 32});

    if (BrowserFeature.DOESNT_OVERRIDE_FONT_SIZE_IN_STYLE_ATTR) {
      const span = HELPER.findTextNode('23').parentNode;
      assertFalse(
          'Style attribute should be gone',
          span.getAttributeNode('style') != null &&
              span.getAttributeNode('style').specified);
    }

    FIELDMOCK.$verify();
  },

  /**
   * Make sure the style stripping works when the selection starts and stops in
   * different nodes that both contain font size styles.
   * @suppress {visibility} suppression added to enable type checking
   */
  testFontSizeOverridesStyleAttrMultiNode() {
    setUpFontSizeTests();
    FIELDMOCK.$replay();

    HELPER.select('23', 0, '56', 2);  // Selects 23456.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.FONT_SIZE, 6);
    const span = HELPER.findTextNode('23').parentNode;
    const span2 = HELPER.findTextNode('56').parentNode;

    assertFontSizes(
        'New font size should override existing font size in all spans',
        {'23': 32, '4': 32, '56': 32});
    const whiteSpace = userAgent.IE ?
        style.getCascadedStyle(span2, 'whiteSpace') :
        style.getComputedStyle(span2, 'whiteSpace');
    assertEquals(
        'Whitespace style in last span should have been left', 'pre',
        whiteSpace);

    if (BrowserFeature.DOESNT_OVERRIDE_FONT_SIZE_IN_STYLE_ATTR) {
      assertFalse(
          'Style attribute should be gone from first span',
          span.getAttributeNode('style') != null &&
              span.getAttributeNode('style').specified);
      assertTrue(
          'Style attribute should not be gone from last span',
          span2.getAttributeNode('style').specified);
    }

    FIELDMOCK.$verify();
  },

  /**
   * Makes sure the font size style is not removed when only a part of the
   * element with font size style is selected during the font size command.
   * @suppress {visibility} suppression added to enable type checking
   */
  testFontSizeDoesntOverrideStyleAttr() {
    setUpFontSizeTests();
    FIELDMOCK.$replay();

    HELPER.select(
        '23', 1, '4', 1);  // Selects 34 (half of span with font size).
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.FONT_SIZE, 6);

    assertFontSizes(
        'New font size shouldn\'t override existing font size before selection',
        {'2': 2, '23': null, '3': 32, '4': 32});

    FIELDMOCK.$verify();
  },

  /**
   * Makes sure the font size style is not removed when only a part of the
   * element with font size style is selected during the font size command, but
   * is removed for another element that is fully selected.
   * @suppress {visibility} suppression added to enable type checking
   */
  testFontSizeDoesntOverrideStyleAttrMultiNode() {
    setUpFontSizeTests();
    FIELDMOCK.$replay();

    HELPER.select('23', 1, '56', 2);  // Selects 3456.
    FORMATTER.execCommandInternal(BasicTextFormatter.COMMAND.FONT_SIZE, 6);

    assertFontSizes(
        'New font size shouldn\'t override existing font size before ' +
            'selection, but still override existing font size in last span',
        {'2': 2, '23': null, '3': 32, '4': 32, '56': 32});

    FIELDMOCK.$verify();
  },

  testFontSizeStyledSpanSelectingText() {
    doTestFontSizeStyledSpan(() => {
      HELPER.select('23', 0, '23', 2);
    });
  },

  testFontSizeStyledSpanSelectingTextNode() {
    doTestFontSizeStyledSpan(() => {
      const textNode = HELPER.findTextNode('23');
      HELPER.select(textNode.parentNode, 0, textNode.parentNode, 1);
    });
  },

  testFontSizeStyledSpanSelectingSpanNode() {
    doTestFontSizeStyledSpan(() => {
      const spanNode = HELPER.findTextNode('23').parentNode;
      HELPER.select(spanNode.parentNode, 1, spanNode.parentNode, 2);
    });
  },

  /**
   * @bug 1414941
   * @suppress {visibility,missingProperties} suppression added to
   *      enable type checking
   */
  testConvertBreaksToDivsKeepsP() {
    if (BrowserFeature.CAN_LISTIFY_BR) {
      return;
    }
    setUpConvertBreaksToDivTests();
    FIELDMOCK.$replay();

    HELPER.select('three', 0);
    FORMATTER.convertBreaksToDivs_();
    assertEquals(
        'There should still be a <p> tag', 1,
        dom.getElementsByTagName(TagName.P, FIELDMOCK.getElement()).length);
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const html = FIELDMOCK.getElement().innerHTML.toLowerCase();
    assertNotBadBrElements(html);
    assertNotContains(
        'There should not be empty <div> elements', '<div><\/div>', html);

    FIELDMOCK.$verify();
    tearDownConvertBreaksToDivTests();
  },

  /**
   * @bug 1414937
   * @bug 934535
   * @suppress {visibility} suppression added to enable type checking
   */
  testConvertBreaksToDivsDoesntCollapseBR() {
    if (BrowserFeature.CAN_LISTIFY_BR) {
      return;
    }
    setUpConvertBreaksToDivTests();
    FIELDMOCK.$replay();

    HELPER.select('three', 0);
    FORMATTER.convertBreaksToDivs_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const html = FIELDMOCK.getElement().innerHTML.toLowerCase();
    assertNotBadBrElements(html);
    assertNotContains(
        'There should not be empty <div> elements', '<div><\/div>', html);
    if (userAgent.IE) {
      // <div><br></div> misbehaves in IE
      assertNotContains(
          '<br> should not be used to prevent <div> from collapsing',
          '<div><br><\/div>', html);
    }

    FIELDMOCK.$verify();
    tearDownConvertBreaksToDivTests();
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testConvertBreaksToDivsSelection() {
    if (BrowserFeature.CAN_LISTIFY_BR) {
      return;
    }
    setUpConvertBreaksToDivTests();
    FIELDMOCK.$replay();

    HELPER.select('two', 1, 'three', 3);
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const before = FIELDMOCK.getRange().getText().replace(/\s/g, '');
    FORMATTER.convertBreaksToDivs_();
    assertEquals(
        'Selection must not be changed', before,
        FIELDMOCK.getRange().getText().replace(/\s/g, ''));

    FIELDMOCK.$verify();
    tearDownConvertBreaksToDivTests();
  },

  /**
   * @bug 1414937
   * @suppress {visibility,missingProperties} suppression added to
   *      enable type checking
   */
  testConvertBreaksToDivsInsertList() {
    setUpConvertBreaksToDivTests();
    FIELDMOCK.$replay();

    HELPER.select('three', 0);
    FORMATTER.execCommandInternal('insertorderedlist');
    assertTrue(
        'Ordered list must be inserted',
        FIELDMOCK.getEditableDomHelper().getDocument().queryCommandState(
            'insertorderedlist'));
    tearDownConvertBreaksToDivTests();
  },

  /**
   * Regression test for {@bug 1939883}, where if a br has an id, it causes
   * the convert br code to throw a js error. This goes a step further and
   * ensures that the id is preserved in the resulting div element.
   * @suppress {visibility} suppression added to enable type checking
   */
  testConvertBreaksToDivsKeepsId() {
    if (BrowserFeature.CAN_LISTIFY_BR) {
      return;
    }
    setUpConvertBreaksToDivTests();
    FIELDMOCK.$replay();

    HELPER.select('one', 0, 'two', 0);
    FORMATTER.convertBreaksToDivs_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const html = FIELDMOCK.getElement().innerHTML.toLowerCase();
    assertNotBadBrElements(html);
    const idBr = document.getElementById('br1');
    assertNotNull('There should still be a tag with id="br1"', idBr);
    assertEquals(
        'The tag with id="br1" should be a <div> now', String(TagName.DIV),
        idBr.tagName);
    assertNull(
        'There should not be any tag with id="temp_br"',
        document.getElementById('temp_br'));

    FIELDMOCK.$verify();
    tearDownConvertBreaksToDivTests();
  },

  testIsJustificationLeft() {
    doTestIsJustification(BasicTextFormatter.COMMAND.JUSTIFY_LEFT);
  },

  testIsJustificationRight() {
    doTestIsJustification(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT);
  },

  testIsJustificationCenter() {
    doTestIsJustification(BasicTextFormatter.COMMAND.JUSTIFY_CENTER);
  },

  testIsJustificationFull() {
    doTestIsJustification(BasicTextFormatter.COMMAND.JUSTIFY_FULL);
  },

  /**
   * Regression test for {@bug 1414813}, where all 3 justification buttons are
   * considered "on" when you first tab into the editable field. In this
   * situation, when lorem ipsum is the only node in the editable field iframe
   * body, mockField.getRange() returns an empty range.
   * @suppress {missingProperties,visibility} suppression added to enable type
   * checking
   */
  testIsJustificationEmptySelection() {
    const mockField = new LooseMock(Field);

    mockField.getRange();
    mockField.$anyTimes();
    mockField.$returns(null);
    mockField.getPluginByClassId('Bidi');
    mockField.$anyTimes();
    mockField.$returns(null);
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    FORMATTER.fieldObject = mockField;

    mockField.$replay();

    assertFalse(
        'Empty selection should not be justified',
        FORMATTER.isJustification_(BasicTextFormatter.COMMAND.JUSTIFY_CENTER));
    assertFalse(
        'Empty selection should not be justified',
        FORMATTER.isJustification_(BasicTextFormatter.COMMAND.JUSTIFY_FULL));
    assertFalse(
        'Empty selection should not be justified',
        FORMATTER.isJustification_(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT));
    assertFalse(
        'Empty selection should not be justified',
        FORMATTER.isJustification_(BasicTextFormatter.COMMAND.JUSTIFY_LEFT));

    mockField.$verify();
  },

  testIsJustificationSimple1() {
    setUpRealField();
    REAL_FIELD.setSafeHtml(
        false, SafeHtml.create('div', {'align': 'right'}, 'foo'));
    selectRealField();

    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_LEFT));
    assertTrue(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT));
  },

  testIsJustificationSimple2() {
    setUpRealField();
    REAL_FIELD.setSafeHtml(
        false,
        SafeHtml.create('div', {'style': {'text-align': 'right'}}, 'foo'));
    selectRealField();

    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_LEFT));
    assertTrue(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT));
  },

  testIsJustificationComplete1() {
    setUpRealField();
    REAL_FIELD.setSafeHtml(
        false,
        SafeHtml.concat(
            SafeHtml.create('div', {'align': 'left'}, 'a'),
            SafeHtml.create('div', {'align': 'right'}, 'b')));
    selectRealField();

    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_LEFT));
    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT));
  },

  testIsJustificationComplete2() {
    setUpRealField();
    REAL_FIELD.setSafeHtml(
        false,
        SafeHtml.concat(
            SafeHtml.create('div', {'align': 'left'}, 'a'),
            SafeHtml.create('div', {'align': 'left'}, 'b')));
    selectRealField();

    assertTrue(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_LEFT));
    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT));
  },

  testIsJustificationComplete3() {
    setUpRealField();
    REAL_FIELD.setSafeHtml(
        false,
        SafeHtml.concat(
            SafeHtml.create('div', {'align': 'right'}, 'a'),
            SafeHtml.create('div', {'align': 'right'}, 'b')));
    selectRealField();

    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_LEFT));
    assertTrue(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT));
  },

  testIsJustificationComplete4() {
    setUpRealField();
    REAL_FIELD.setSafeHtml(
        false,
        SafeHtml.concat(
            SafeHtml.create(
                'div', {'align': 'right'},
                SafeHtml.create('div', {'align': 'left'}, 'a')),
            SafeHtml.create('div', {'align': 'right'}, 'b')));
    selectRealField();

    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_LEFT));
    assertTrue(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT));
  },

  testIsJustificationComplete5() {
    setUpRealField();
    REAL_FIELD.setSafeHtml(
        false,
        SafeHtml.concat(
            SafeHtml.create('div', {'align': 'right'}, 'a'), 'b',
            SafeHtml.create('div', {'align': 'right'}, 'c')));
    selectRealField();

    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_LEFT));
    assertFalse(
        REAL_FIELD.queryCommandValue(BasicTextFormatter.COMMAND.JUSTIFY_RIGHT));
  },

  testIsJustificationPInDivLeft() {
    doTestIsJustificationPInDiv(
        false, 'left', BasicTextFormatter.COMMAND.JUSTIFY_LEFT);
  },

  testIsJustificationPInDivRight() {
    doTestIsJustificationPInDiv(
        false, 'right', BasicTextFormatter.COMMAND.JUSTIFY_RIGHT);
  },

  testIsJustificationPInDivCenter() {
    doTestIsJustificationPInDiv(
        false, 'center', BasicTextFormatter.COMMAND.JUSTIFY_CENTER);
  },

  testIsJustificationPInDivJustify() {
    doTestIsJustificationPInDiv(
        false, 'justify', BasicTextFormatter.COMMAND.JUSTIFY_FULL);
  },

  testIsJustificationPInDivLeftCss() {
    doTestIsJustificationPInDiv(
        true, 'left', BasicTextFormatter.COMMAND.JUSTIFY_LEFT);
  },

  testIsJustificationPInDivRightCss() {
    doTestIsJustificationPInDiv(
        true, 'right', BasicTextFormatter.COMMAND.JUSTIFY_RIGHT);
  },

  testIsJustificationPInDivCenterCss() {
    doTestIsJustificationPInDiv(
        true, 'center', BasicTextFormatter.COMMAND.JUSTIFY_CENTER);
  },

  testIsJustificationPInDivJustifyCss() {
    doTestIsJustificationPInDiv(
        true, 'justify', BasicTextFormatter.COMMAND.JUSTIFY_FULL);
  },

  testPrepareContent() {
    setUpRealField();
    assertPreparedContents('\n', '\n');

    if (BrowserFeature.COLLAPSES_EMPTY_NODES) {
      assertPreparedContents(
          '&nbsp;<script>alert(\'hi\')<' +
              '/script>',
          '<script>alert(\'hi\')<' +
              '/script>');
    } else {
      assertNotPreparedContents(
          '<script>alert(\'hi\')<' +
          '/script>');
    }

    if (BrowserFeature.CONVERT_TO_B_AND_I_TAGS) {
      assertPreparedContents(
          '<b id=\'foo\'>hi</b>', '<strong id=\'foo\'>hi</strong>');
      assertPreparedContents('<i>hi</i>', '<em>hi</em>');
      assertNotPreparedContents('<embed>');
    } else {
      assertNotPreparedContents('<em>hi</em>');
      assertNotPreparedContents('<strong id=\'foo\'>hi</strong>');
    }
  },

  testScrubImagesRemovesCustomAttributes() {
    const fieldElem = dom.getElement('real-field');
    dom.removeChildren(fieldElem);
    const attrs = {
      'src': 'http://www.google.com/foo.jpg',
      'tabIndex': '0',
      'tabIndexSet': '0',
    };
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    attrs[goog.HASH_CODE_PROPERTY_] = '0';
    dom.appendChild(fieldElem, dom.createDom(TagName.IMG, attrs));

    setUpRealField();

    const html = REAL_FIELD.getCleanContents();
    assert(
        `Images must not have forbidden attributes: ${html}`,
        -1 == html.indexOf('tabIndex') && -1 == html.indexOf('closure'));
    assert(
        `Image URLs must not be made relative by default: ${html}`,
        -1 != html.indexOf('/foo.jpg'));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGeckoSelectionChange() {
    if (!userAgent.GECKO) {
      return;
    }

    setUpRealFieldIframe();
    // Use native selection for this test because goog.dom.Range will
    // change selections of <br>
    const ifr = document.getElementById('iframe');
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    ifr.contentWindow.document.body.innerHTML =
        'hello<br id="br1"><br id="br2">';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const body = ifr.contentWindow.document.body;
    const range = REAL_FIELD.getRange();
    const browserRange = range.getBrowserRangeObject();
    browserRange.setStart(body, 2);
    browserRange.setEnd(body, 2);
    FORMATTER.applyExecCommandGeckoFixes_('formatblock');
    const updatedRange = REAL_FIELD.getRange().getBrowserRangeObject();
    assertEquals(
        'Range should have been updated to deep range', 'br2',
        updatedRange.startContainer.id);
    assertEquals(
        'Range should have been updated to deep range', 0,
        updatedRange.startOffset);
  },

  testIEExecCommandFixes() {
    if (!userAgent.IE) {
      return;
    }

    setUpRealField();
    REAL_FIELD.setSafeHtml(false, SafeHtml.create('blockquote', {}, 'hi'));
    Range.createFromNodeContents(REAL_FIELD.getElement()).select();

    /** @suppress {visibility} suppression added to enable type checking */
    const nodes = REAL_PLUGIN.applyExecCommandIEFixes_('insertOrderedList');
    assertHTMLEquals(
        '<blockquote>hi<div style="height:0px"></div></blockquote>',
        REAL_FIELD.getCleanContents());
  },
});
