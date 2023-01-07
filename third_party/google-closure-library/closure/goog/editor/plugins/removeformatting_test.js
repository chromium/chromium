/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.RemoveFormattingTest');
goog.setTestOnly();

const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const Range = goog.require('goog.dom.Range');
const RemoveFormatting = goog.require('goog.editor.plugins.RemoveFormatting');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const googString = goog.require('goog.string');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const userAgent = goog.require('goog.userAgent');

let SAVED_HTML;
let FIELDMOCK;
let FORMATTER;
let testHelper;

let WEBKIT_AFTER_CHROME_21;
let insertImageBoldGarbage = '';
let insertImageFontGarbage = '';
let controlHtml;
let controlCleanHtml;
let expectedFailures;

function setUpTableTests() {
  const div = document.getElementById('html');
  div.innerHTML = '<table><tr> <th> head1</th><th id= "outerTh">' +
      '<span id="emptyTh">head2</span></th> </tr><tr> <td> one </td> <td>' +
      'two </td> </tr><tr><td> three</td><td id="outerTd"> ' +
      '<span id="emptyTd"><strong>four</strong></span></td></tr>' +
      '<tr id="outerTr"><td><span id="emptyTr"> five </span></td></tr>' +
      '<tr id="outerTr2"><td id="cell1"><b>seven</b></td><td id="cell2">' +
      '<u>eight</u><span id="cellspan2"> foo</span></td></tr></table>';
}

/**
 * A short formatting removal function for use with the RemoveFormatting
 * plugin. Does enough that we can tell this function was run over the
 * document.
 * @param {string} text The HTML in from the document.
 * @return {string} The "cleaned" HTML out.
 */
function replacementFormattingFunc(text) {
  // Really basic so that we can just see this is executing.
  return text.replace(/Foo/gi, 'Bar').replace(/<[\/]*span[^>]*>/gi, '');
}

// Runs tests knowing some browsers will fail, because the new
// table functionality hasn't been implemented in them yet.
function runExpectingFailuresForUnimplementedBrowsers(func) {
  if (userAgent.IE) {
    // IE returns an "unspecified error" which seems to be beyond
    // ExpectedFailures' ability to catch.
    return;
  }

  expectedFailures.expectFailureFor(
      userAgent.IE, 'Proper behavior not yet implemented for IE.');
  expectedFailures.expectFailureFor(
      userAgent.WEBKIT, 'Proper behavior not yet implemented for WebKit.');

  expectedFailures.run(func);
}

testSuite({
  shouldRunTests() {
    // This test has not yet been updated to run on IE8 and up. See
    // b/2997691.
    return !userAgent.IE;
  },

  setUpPage() {
    WEBKIT_AFTER_CHROME_21 = userAgent.WEBKIT;
    // On Chrome 16, execCommand('insertImage') inserts a garbage BR
    // after the image that we insert. We use this command to paste HTML
    // in-place, because it has better paragraph-preserving semantics.
    //
    // TODO(nicksantos): Figure out if there are better chrome APIs that we
    // should be using, or if insertImage should just be fixed.
    if (WEBKIT_AFTER_CHROME_21) {
      insertImageBoldGarbage = '<br>';
      insertImageFontGarbage = '<br>';

    } else if (userAgent.EDGE) {
      if (product.isVersion(14)) {
        insertImageFontGarbage = '<fontsize="-1"></fontsize="-1">';
      } else {
        insertImageFontGarbage =
            '<fontsize="-1"><font class="p" size="-1"></font></fontsize="-1">';
      }
    }
    // Extra html to add to test html to make sure removeformatting is
    // actually getting called when you're testing if it leaves certain
    // styles alone (instead of not even running at all due to some other
    // bug). However, adding this extra text into the node to be selected
    // screws up IE. (e.g. <a><img></a><b>t</b> --> <a></a><a><img></a>t )
    // TODO(user): Remove this special casing once http://b/3131117
    // is fixed.
    controlHtml = userAgent.IE ? '' : '<u>control</u>';
    controlCleanHtml = userAgent.IE ? '' : 'control';
    if (userAgent.EDGE) {
      controlCleanHtml = 'control<u></u>';
    }
    expectedFailures = new ExpectedFailures();
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  setUp() {
    testHelper = new TestHelper(document.getElementById('html'));
    testHelper.setUpEditableElement();

    FIELDMOCK = new FieldMock();
    FIELDMOCK.getElement();
    FIELDMOCK.$anyTimes();
    FIELDMOCK.$returns(document.getElementById('html'));

    FORMATTER = new RemoveFormatting();
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    FORMATTER.fieldObject = FIELDMOCK;

    FIELDMOCK.$replay();
  },

  tearDown() {
    expectedFailures.handleTearDown();
    testHelper.tearDownEditableElement();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testTableTagsAreNotRemoved() {
    setUpTableTests();
    let span;

    // TD
    span = document.getElementById('emptyTd');
    Range.createFromNodeContents(span).select();
    FORMATTER.removeFormatting_();

    let elem = document.getElementById('outerTd');
    assertTrue('TD should not be removed', !!elem);
    if (!userAgent.WEBKIT && !userAgent.EDGE) {
      // webkit seems to have an Apple-style-span
      assertEquals(
          'TD should be clean', 'four', googString.trim(elem.innerHTML));
    }

    // TR
    span = document.getElementById('outerTr');
    Range.createFromNodeContents(span).select();
    FORMATTER.removeFormatting_();

    elem = document.getElementById('outerTr');
    assertTrue('TR should not be removed', !!elem);

    // TH
    span = document.getElementById('emptyTh');
    Range.createFromNodeContents(span).select();
    FORMATTER.removeFormatting_();

    elem = document.getElementById('outerTh');
    assertTrue('TH should not be removed', !!elem);
    if (!userAgent.WEBKIT && !userAgent.EDGE) {
      // webkit seems to have an Apple-style-span
      assertEquals('TH should be clean', 'head2', elem.innerHTML);
    }
  },

  /**
   * We select two cells from the table and then make sure that there is no
   * data loss and basic formatting is removed from each cell.
   */
  testTableDataIsNotRemoved() {
    setUpTableTests();
    if (userAgent.IE) {
      // IE returns an "unspecified error" which seems to be beyond
      // ExpectedFailures' ability to catch.
      return;
    }

    expectedFailures.expectFailureFor(
        userAgent.WEBKIT || userAgent.EDGE,
        'The content moves out of the table in WebKit and Edge.');

    if (userAgent.IE) {
      // Not used since we bail out early for IE, but this is there so that
      // developers can easily reproduce IE error.
      Range.createFromNodeContents(document.getElementById('outerTr2'))
          .select();
    } else {
      const selection = window.getSelection();
      if (selection.rangeCount > 0) selection.removeAllRanges();
      let range = document.createRange();
      range.selectNode(document.getElementById('cell1'));
      selection.addRange(range);
      range = document.createRange();
      range.selectNode(document.getElementById('cell2'));
      selection.addRange(range);
    }

    expectedFailures
        .run(/**
                @suppress {visibility} suppression added to enable type
                checking
              */
             () => {
               FORMATTER.removeFormatting_();

               let span = document.getElementById('outerTr2');
               assertEquals(
                   'Table data should not be removed',
                   '<td id="cell1">seven</td><td id="cell2">eight foo</td>',
                   span.innerHTML);
             });
  },

  testLinksAreNotRemoved() {
    let anchor;
    const div = document.getElementById('html');
    div.innerHTML = 'Foo<span id="link">Pre<a href="http://www.google.com">' +
        'Outside Span<span style="font-size:15pt">Inside Span' +
        '</span></a></span>';

    anchor = document.getElementById('link');
    Range.createFromNodeContents(anchor).select();

    expectedFailures
        .run(/**
                @suppress {visibility} suppression added to enable type
                checking
              */
             () => {
               FORMATTER.removeFormatting_();
               assertHTMLEquals(
                   'link should not be removed',
                   'FooPre<a href="http://www.google.com/">Outside SpanInside Span</a>',
                   div.innerHTML);
             });
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAlternateRemoveFormattingFunction() {
    const div = document.getElementById('html');
    div.innerHTML = 'Start<span id="remFormat">Foo<pre>Bar</pre>Baz</span>';

    FORMATTER.setRemoveFormattingFunc(replacementFormattingFunc);
    const area = document.getElementById('remFormat');
    Range.createFromNodeContents(area).select();
    FORMATTER.removeFormatting_();
    // Webkit will change all tags to non-formatted ones anyway.
    // Make sure 'Foo' was changed to 'Bar'

    assertHTMLEquals(
        'regular cleaner should not have run', 'StartBar<pre>Bar</pre>Baz',
        div.innerHTML);
  },

  testGetValueForNode() {
    // Override getValueForNode to keep bold tags.
    const oldGetValue = RemoveFormatting.prototype.getValueForNode;
    /** @suppress {visibility} suppression added to enable type checking */
    RemoveFormatting.prototype.getValueForNode = function(node) {
      if (node.nodeName == TagName.B) {
        return '<b>' + this.removeFormattingWorker_(node.innerHTML) + '</b>';
      }
      return null;
    };

    /** @suppress {visibility} suppression added to enable type checking */
    let html = FORMATTER.removeFormattingWorker_('<div>foo<b>bar</b></div>');
    assertHTMLEquals('B tags should remain', 'foo<b>bar</b>', html);

    // Override getValueForNode to throw out bold tags, and their contents.
    RemoveFormatting.prototype.getValueForNode = (node) => {
      if (node.nodeName == TagName.B) {
        return '';
      }
      return null;
    };

    /** @suppress {visibility} suppression added to enable type checking */
    html = FORMATTER.removeFormattingWorker_('<div>foo<b>bar</b></div>');
    assertHTMLEquals('B tag and its contents should be removed', 'foo', html);

    FIELDMOCK.$verify();
    RemoveFormatting.prototype.getValueForNode = oldGetValue;
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRemoveFormattingAddsNoNbsps() {
    const div = document.getElementById('html');
    div.innerHTML = '"<span id="toStrip">Twin <b>Cinema</b></span>"';

    const span = document.getElementById('toStrip');
    Range.createFromNodeContents(span).select();

    FORMATTER.removeFormatting_();

    assertEquals(
        'Text should be the same, with no non-breaking spaces', '"Twin Cinema"',
        div.innerHTML);

    FIELDMOCK.$verify();
  },

  /** @bug 992795 */
  testRemoveFormattingNestedDivs() {
    /** @suppress {visibility} suppression added to enable type checking */
    const html = FORMATTER.removeFormattingWorker_(
        '<div>1</div><div><div>2</div></div>');

    testingDom.assertHtmlMatches('1<br>2', html);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testTheJavascriptReplaceMetacharacters() {
    const div = document.getElementById('html');
    div.innerHTML = '123 $< $> $" $& $$ $` $\' 456';
    const expected = '123 $&lt; $&gt; $" $&amp; $$ $` $\' 456' +
        (product.SAFARI ? '<br>' : '');
    // No idea why these <br> appear, but they're fairly insignificant
    // anyways.

    Range.createFromNodeContents(div).select();

    FORMATTER.removeFormatting_();
    assertHTMLEquals(
        'String.prototype.replace metacharacters should not trigger', expected,
        div.innerHTML);
  },

  /**
   * Test that when we perform remove formatting on an entire table,
   * that the visual look is similar to as if there was a table there.
   */
  testRemoveFormattingForTableFormatting() {
    // We preserve the table formatting as much as possible.
    // Spaces separate TD's, <br>'s separate TR's.
    // <br>'s separate the start and end of a table.
    let html = '<table><tr><td>cell00</td><td>cell01</td></tr>' +
        '<tr><td>cell10</td><td>cell11</td></tr></table>';
    /** @suppress {visibility} suppression added to enable type checking */
    html = FORMATTER.removeFormattingWorker_(html);
    assertHTMLEquals('<br>cell00 cell01<br>cell10 cell11<br>', html);
  },

  /**
   * @bug 1319715
   * @suppress {visibility} suppression added to enable type
   *      checking
   */
  testRemoveFormattingDoesNotShrinkSelection() {
    const div = document.getElementById('html');
    div.innerHTML = '<div>l </div><div><br><b>a</b>foo bar</div>';
    const div2 = div.lastChild;

    Range.createFromNodes(div2.firstChild, 0, div2.lastChild, 7).select();

    FORMATTER.removeFormatting_();

    const range = Range.createFromWindow();
    assertEquals(
        'Correct text should be selected', 'afoo bar', range.getText());

    // We have to trim out the leading BR in IE due to execCommand issues,
    // so it isn't sent off to the removeFormattingWorker.
    // Workaround for broken removeFormat in old webkit added an extra
    // <br> to the end of the html.
    let html = '<div>l </div><br class="GECKO WEBKIT">afoo bar' +
        (BrowserFeature.ADDS_NBSPS_IN_REMOVE_FORMAT ? '<br>' : '');
    if (userAgent.EDGE) {  // TODO(sdh): I have no idea where this comes from
      html = html.replace(' class="GECKO WEBKIT"', '');
    }

    testingDom.assertHtmlContentsMatch(html, div);
    FIELDMOCK.$verify();
  },

  /** @bug 1447374 */
  testInsideListRemoveFormat() {
    const div = document.getElementById('html');
    div.innerHTML = '<ul><li>one</li><li><b>two</b></li><li>three</li></ul>';

    const twoLi = div.firstChild.childNodes[1];
    Range.createFromNodeContents(twoLi).select();

    expectedFailures.expectFailureFor(
        userAgent.IE,
        'IE adds the "two" to the "three" li, and leaves empty B tags.');
    expectedFailures.expectFailureFor(
        userAgent.WEBKIT || userAgent.EDGE,
        'WebKit and Edge leave the "two" orphaned outside of an li but ' +
            'inside the ul (invalid HTML).');

    expectedFailures
        .run(/**
                @suppress {visibility} suppression added to enable type
                checking
              */
             () => {
               FORMATTER.removeFormatting_();
               // Test that we split the list.
               assertHTMLEquals(
                   '<ul><li>one</li></ul><br>two<ul><li>three</li></ul>',
                   div.innerHTML);
               FIELDMOCK.$verify();
             });
  },

  testFullListRemoveFormat() {
    const div = document.getElementById('html');
    div.innerHTML =
        '<ul><li>one</li><li><b>two</b></li><li>three</li></ul>after';

    Range.createFromNodeContents(div.firstChild).select();

    //  Note: This may just be a createFromNodeContents issue, as
    //  I can't ever make this happen with real user selection.
    expectedFailures.expectFailureFor(
        userAgent.IE,
        'IE combines everything into a single LI and leaves the UL.');

    expectedFailures.run(/**
                            @suppress {visibility} suppression added to enable
                            type checking
                          */
                         () => {
                           FORMATTER.removeFormatting_();
                           // Test that we completely remove the list.
                           assertHTMLEquals(
                               '<br>one<br>two<br>threeafter', div.innerHTML);
                           FIELDMOCK.$verify();
                         });
  },

  /** @bug 1440935 */
  testPartialListRemoveFormat() {
    const div = document.getElementById('html');
    div.innerHTML = '<ul><li>one</li><li>two</li><li>three</li></ul>after';

    // Select "two three after".
    Range.createFromNodes(div.firstChild.childNodes[1], 0, div.lastChild, 5)
        .select();

    expectedFailures.expectFailureFor(
        userAgent.IE, 'IE leaves behind an empty LI.');
    expectedFailures.expectFailureFor(
        userAgent.WEBKIT, 'WebKit completely loses the "one".');
    if (userAgent.EDGE) {
      // Edge leaves "two" and "threeafter" orphaned outside of an li but
      // inside the ul (invalid HTML). Skip this test instead of using
      // expectedFailures because this failure mode wrecks the DOM and
      // causes later tests to fail as well.
      return;
    }

    expectedFailures.run(/**
                            @suppress {visibility} suppression added to enable
                            type checking
                          */
                         () => {
                           FORMATTER.removeFormatting_();
                           // Test that we leave the list start alone.
                           assertHTMLEquals(
                               '<ul><li>one</li></ul><br>two<br>threeafter',
                               div.innerHTML);
                           FIELDMOCK.$verify();
                         });
  },

  testBasicRemoveFormatting() {
    // IE will clobber the editable div.
    // Note: I can't repro this using normal user selections.
    if (userAgent.IE) {
      return;
    }
    const div = document.getElementById('html');
    div.innerHTML = '<b>bold<i>italic</i></b>';

    Range.createFromNodeContents(div).select();

    expectedFailures.expectFailureFor(
        BrowserFeature.ADDS_NBSPS_IN_REMOVE_FORMAT,
        'The workaround for the nbsp bug adds an extra br at the end.');

    expectedFailures.run(/**
                            @suppress {visibility} suppression added to enable
                            type checking
                          */
                         () => {
                           FORMATTER.removeFormatting_();
                           assertHTMLEquals(
                               `bolditalic${insertImageBoldGarbage}`,
                               div.innerHTML);
                           FIELDMOCK.$verify();
                         });
  },

  /** @bug 1480260 */
  testPartialBasicRemoveFormatting() {
    const div = document.getElementById('html');
    div.innerHTML = '<b>bold<i>italic</i></b>';

    Range
        .createFromNodes(
            div.firstChild.firstChild, 2, div.firstChild.lastChild.firstChild,
            3)
        .select();


    expectedFailures.run(/**
                            @suppress {visibility} suppression added to enable
                            type checking
                          */
                         () => {
                           FORMATTER.removeFormatting_();
                           assertHTMLEquals(
                               '<b>bo</b>ldita<b><i>lic</i></b>',
                               div.innerHTML);
                           FIELDMOCK.$verify();
                         });
  },

  /**
   * @bug 3075557
   * @suppress {visibility} suppression added to enable type
   *      checking
   */
  testRemoveFormattingLinkedImageBorderZero() {
    const testHtml = '<a href="http://www.google.com/">' +
        '<img src="http://www.google.com/images/logo.gif" border="0"></a>';
    const div = document.getElementById('html');
    div.innerHTML = testHtml + controlHtml;
    Range.createFromNodeContents(div).select();
    FORMATTER.removeFormatting_();

    expectedFailures.expectFailureFor(
        userAgent.WEBKIT,
        'WebKit removes the image entirely, see ' +
            'https://bugs.webkit.org/show_bug.cgi?id=13125 .');

    expectedFailures.run(() => {
      assertHTMLEquals(
          'Image\'s border=0 should not be removed during remove formatting',
          testHtml + controlCleanHtml, div.innerHTML);
      FIELDMOCK.$verify();
    });
  },

  /**
   * @bug 3075557
   * @suppress {visibility} suppression added to enable type
   *      checking
   */
  testRemoveFormattingLinkedImageBorderNonzero() {
    const testHtml = '<a href="http://www.google.com/">' +
        '<img src="http://www.google.com/images/logo.gif" border="1"></a>';
    const div = document.getElementById('html');
    div.innerHTML = testHtml + controlHtml;
    Range.createFromNodeContents(div).select();
    FORMATTER.removeFormatting_();

    expectedFailures.expectFailureFor(
        userAgent.WEBKIT,
        'WebKit removes the image entirely, see ' +
            'https://bugs.webkit.org/show_bug.cgi?id=13125 .');

    expectedFailures.run(() => {
      assertHTMLEquals(
          'Image\'s border should be removed during remove formatting' +
              ' if non-zero',
          testHtml.replace(' border="1"', '') + controlCleanHtml,
          div.innerHTML);
      FIELDMOCK.$verify();
    });
  },

  /**
   * @bug 3075557
   * @suppress {visibility} suppression added to enable type
   *      checking
   */
  testRemoveFormattingUnlinkedImage() {
    const testHtml =
        '<img src="http://www.google.com/images/logo.gif" border="0">';
    const div = document.getElementById('html');
    div.innerHTML = testHtml + controlHtml;
    Range.createFromNodeContents(div).select();
    FORMATTER.removeFormatting_();

    expectedFailures.expectFailureFor(
        userAgent.WEBKIT,
        'WebKit removes the image entirely, see ' +
            'https://bugs.webkit.org/show_bug.cgi?id=13125 .');

    expectedFailures.run(() => {
      assertHTMLEquals(
          'Image\'s border=0 should not be removed during remove formatting' +
              ' even if not wrapped by a link',
          testHtml + controlCleanHtml, div.innerHTML);
      FIELDMOCK.$verify();
    });
  },

  /**
   * @bug 3075557
   * @suppress {visibility} suppression added to enable type
   *      checking
   */
  testRemoveFormattingLinkedImageDeep() {
    const testHtml = '<a href="http://www.google.com/"><b>hello' +
        '<img src="http://www.google.com/images/logo.gif" border="0">' +
        'world</b></a>';
    const div = document.getElementById('html');
    div.innerHTML = testHtml + controlHtml;
    Range.createFromNodeContents(div).select();
    FORMATTER.removeFormatting_();


    expectedFailures.run(() => {
      assertHTMLEquals(
          'Image\'s border=0 should not be removed during remove formatting' +
              ' even if deep inside anchor tag',
          testHtml.replace(/<\/?b>/g, '') + controlCleanHtml +
              insertImageBoldGarbage,
          div.innerHTML);
      FIELDMOCK.$verify();
    });
  },

  testFullTableRemoveFormatting() {
    // Something goes horrible wrong in case 1 below.  It was crashing all
    // WebKit browsers, and now seems to be giving errors as it is trying
    // to perform remove formatting on the little expected failures window
    // instead of the dom we select.  WTF.  Since I'm gutting this code,
    // I'm not going to look into this anymore right now.  For what its
    // worth, I can't repro any issues in standalone TrogEdit.
    if (userAgent.WEBKIT) {
      return;
    }

    const div = document.getElementById('html');

    // WebKit has an extra BR in case 2.
    expectedFailures.expectFailureFor(
        userAgent.IE,
        'IE clobbers the editable node in case 2 (can\'t repro with real ' +
            'user selections). IE doesn\'t remove the table in case 1.');

    expectedFailures
        .run(/**
                @suppress {visibility} suppression added to enable type
                checking
              */
             () => {
               // When a full table is selected, we remove it completely.
               div.innerHTML = 'foo<table><tr><td>bar</td></tr></table>baz1';
               Range.createFromNodeContents(div.childNodes[1]).select();
               FORMATTER.removeFormatting_();
               assertHTMLEquals('foo<br>bar<br>baz1', div.innerHTML);
               FIELDMOCK.$verify();

               // Remove the full table when it is selected with additional
               // contents too.
               div.innerHTML = 'foo<table><tr><td>bar</td></tr></table>baz2';
               Range.createFromNodes(div.firstChild, 0, div.lastChild, 1)
                   .select();
               FORMATTER.removeFormatting_();
               assertHTMLEquals('foo<br>bar<br>baz2', div.innerHTML);
               FIELDMOCK.$verify();

               // We should still remove the table, even if the selection is
               // inside the table and it is fully selected.
               div.innerHTML =
                   'foo<table><tr><td id=\'td\'>bar</td></tr></table>baz3';
               Range.createFromNodeContents(dom.getElement('td').firstChild)
                   .select();
               FORMATTER.removeFormatting_();
               assertHTMLEquals('foo<br>bar<br>baz3', div.innerHTML);
               FIELDMOCK.$verify();
             });
  },

  testInsideTableRemoveFormatting() {
    const div = document.getElementById('html');
    div.innerHTML =
        '<table><tr><td><b id="b">foo</b></td></tr><tr><td>ba</td></tr></table>';

    Range.createFromNodeContents(dom.getElement('b')).select();



    expectedFailures.run(/**
                            @suppress {visibility} suppression added to enable
                            type checking
                          */
                         () => {
                           FORMATTER.removeFormatting_();

                           // Only remove styling from inside tables.
                           assertHTMLEquals(
                               `<table><tr><td>foo${insertImageBoldGarbage}` +
                                   '</td></tr><tr><td>ba</td></tr></table>',
                               div.innerHTML);
                           FIELDMOCK.$verify();
                         });
  },

  testPartialTableRemoveFormatting() {
    if (userAgent.IE) {
      // IE returns an "unspecified error" which seems to be beyond
      // ExpectedFailures' ability to catch.
      return;
    }

    const div = document.getElementById('html');
    div.innerHTML = 'bar<table><tr><td><b id="b">foo</b></td></tr>' +
        '<tr><td><i>banana</i></td></tr></table><div id="baz">' +
        'baz</div>';

    // Select from the "oo" inside the b tag to the end of "baz".
    Range
        .createFromNodes(
            dom.getElement('b').firstChild, 1, dom.getElement('baz').firstChild,
            3)
        .select();

    // All browsers currently clobber the table cells that are selected.
    expectedFailures.expectFailureFor(userAgent.WEBKIT);

    expectedFailures.run(/**
                            @suppress {visibility} suppression added to enable
                            type checking
                          */
                         () => {
                           FORMATTER.removeFormatting_();
                           // Only remove styling from inside tables.
                           assertHTMLEquals(
                               'bar<table><tr><td><b id="b">f</b>oo</td></tr>' +
                                   '<tr><td>banana</td></tr></table>baz',
                               div.innerHTML);
                           FIELDMOCK.$verify();
                         });
  },

  testTwoTablesSelectedFullyRemoveFormatting() {
    runExpectingFailuresForUnimplementedBrowsers(/**
                                                    @suppress {visibility}
                                                    suppression added to
                                                    enable type checking
                                                  */
                                                 () => {
                                                   const div =
                                                       document.getElementById(
                                                           'html');
                                                   // When two tables are
                                                   // fully selected, we
                                                   // remove them
                                                   // completely.
                                                   div.innerHTML =
                                                       '<table><tr><td>foo</td></tr></table>' +
                                                       '<table><tr><td>bar</td></tr></table>';
                                                   Range
                                                       .createFromNodes(
                                                           div.firstChild, 0,
                                                           div.lastChild, 1)
                                                       .select();
                                                   FORMATTER
                                                       .removeFormatting_();
                                                   assertHTMLEquals(
                                                       '<br>foo<br><br>bar<br>',
                                                       div.innerHTML);
                                                   FIELDMOCK.$verify();
                                                 });
  },

  testTwoTablesSelectedFullyInsideRemoveFormatting() {
    if (userAgent.WEBKIT) {
      // Something goes very wrong here, but it did before
      // Julie started writing v2.  Will address when converting
      // safari to v2.
      return;
    }

    runExpectingFailuresForUnimplementedBrowsers(/**
                                                    @suppress {visibility}
                                                    suppression added to
                                                    enable type checking
                                                  */
                                                 () => {
                                                   const div =
                                                       document.getElementById(
                                                           'html');
                                                   // When two tables are
                                                   // selected from inside
                                                   // but fully, also remove
                                                   // them completely.
                                                   div.innerHTML =
                                                       '<table><tr><td id="td1">foo</td></tr></table>' +
                                                       '<table><tr><td id="td2">bar</td></tr></table>';
                                                   Range
                                                       .createFromNodes(
                                                           dom.getElement('td1')
                                                               .firstChild,
                                                           0,
                                                           dom.getElement('td2')
                                                               .firstChild,
                                                           3)
                                                       .select();
                                                   FORMATTER
                                                       .removeFormatting_();
                                                   assertHTMLEquals(
                                                       '<br>foo<br><br>bar<br>',
                                                       div.innerHTML);
                                                   FIELDMOCK.$verify();
                                                 });
  },

  testTwoTablesSelectedFullyAndPartiallyRemoveFormatting() {
    runExpectingFailuresForUnimplementedBrowsers(/**
                                                    @suppress {visibility}
                                                    suppression added to
                                                    enable type checking
                                                  */
                                                 () => {
                                                   const div =
                                                       document.getElementById(
                                                           'html');
                                                   // Two tables selected,
                                                   // one fully, one
                                                   // partially. Remove only
                                                   // the fully selected one
                                                   // and remove styles only
                                                   // from partially
                                                   // selected one.
                                                   div.innerHTML =
                                                       '<table><tr><td id="td1">foo</td></tr></table>' +
                                                       '<table><tr><td id="td2"><b>bar<b></td></tr></table>';
                                                   Range
                                                       .createFromNodes(
                                                           dom.getElement('td1')
                                                               .firstChild,
                                                           0,
                                                           dom.getElement('td2')
                                                               .firstChild
                                                               .firstChild,
                                                           2)
                                                       .select();
                                                   FORMATTER
                                                       .removeFormatting_();
                                                   let expectedHtml =
                                                       '<br>foo<br>' +
                                                       '<table><tr><td id="td2">ba<b>r</b></td></tr></table>';
                                                   if (userAgent.EDGE) {
                                                     // TODO(sdh): Edge
                                                     // inserts an extra
                                                     // empty <b> tag but is
                                                     // otherwise correct
                                                     expectedHtml =
                                                         expectedHtml.replace(
                                                             '</b>',
                                                             '<b></b></b>');
                                                   }
                                                   assertHTMLEquals(
                                                       expectedHtml,
                                                       div.innerHTML);
                                                   FIELDMOCK.$verify();
                                                 });
  },

  testTwoTablesSelectedPartiallyRemoveFormatting() {
    runExpectingFailuresForUnimplementedBrowsers(/**
                                                    @suppress {visibility}
                                                    suppression added to
                                                    enable type checking
                                                  */
                                                 () => {
                                                   const div =
                                                       document.getElementById(
                                                           'html');
                                                   // Two tables selected,
                                                   // both partially.  Don't
                                                   // remove tables, but
                                                   // remove styles.
                                                   div.innerHTML =
                                                       '<table><tr><td id="td1">f<i>o</i>o</td></tr></table>' +
                                                       '<table><tr><td id="td2">b<b>a</b>r</td></tr></table>';
                                                   Range
                                                       .createFromNodes(
                                                           dom.getElement('td1')
                                                               .firstChild,
                                                           1,
                                                           dom.getElement('td2')
                                                               .childNodes[1],
                                                           1)
                                                       .select();
                                                   FORMATTER
                                                       .removeFormatting_();
                                                   assertHTMLEquals(
                                                       '<table><tr><td id="td1">foo</td></tr></table>' +
                                                           '<table><tr><td id="td2">bar</td></tr></table>',
                                                       div.innerHTML);
                                                   FIELDMOCK.$verify();
                                                 });
  },

  /**
   * Test a random snippet from Google News (Google News has complicated
   * dom structure, including tables, links, images, etc).
   */
  testRandomGoogleNewsSnippetRemoveFormatting() {
    if (userAgent.IE) {
      // IE returns an "unspecified error" which seems to be beyond
      // ExpectedFailures' ability to catch.
      return;
    }

    const div = document.getElementById('html');
    div.innerHTML =
        '<font size="-3"><br></font><table align="right" border="0" ' +
        'cellpadding="0" cellspacing="0"><tbody><tr><td style="padding-left:' +
        '6px;" valign="top" width="80" align="center"><a href="http://www.wash' +
        'ingtonpost.com/wp-dyn/content/article/2008/11/11/AR2008111101090.htm' +
        'l" + id="s-skHRvWH7ryqkcA4caGv0QQ:u-AFQjCNG3vx1HJOxKxMQPzCvYOVRE0JUDe' +
        'Q:r-1-0i_1268233361_6_H0_MH20_PL60"><img src="http://news.google.com/' +
        'news?imgefp=4LFiNNP62TgJ&amp;imgurl=media3.washingtonpost.com/wp-dyn/' +
        'content/photo/2008/11/11/PH2008111101091.jpg" alt="" width="60" ' +
        'border="1" height="80"><br><font size="-2">Washington Post</font></a>' +
        '</td></tr></tbody></table><a href="http://www.nme.com/news/britney-' +
        'spears/40995" id="s-xZUO-t0c1IpsVjyJj0rgxw:u-AFQjCNEZAMQCseEW6uTgXI' +
        'iPvAMHe_0B4A:r-1-0_1268233361_6_H0_MH20_PL60"><b>Britney\'s son ' +
        'released from hospital</b></a><br><font size="-1"><b><font color=' +
        '"#6f6f6f">NME.com&nbsp;-</font> <nobr>53 minutes ago</nobr></b>' +
        '</font><br><font size="-1">Britney Spears� youngest son Jayden James ' +
        'has been released from hospital, having been admitted on Sunday after' +
        ' suffering a severe reaction to something he ingested.</font><br><fon' +
        'tsize="-1"><a href="http://www.celebrity-gossip.net/celebrities/holly' +
        'wood/britney-and-jamie-lynn-spears-alligator-alley-208944/" id="s-nM' +
        'PzHclcMG0J2WZkw9gnVQ:u-AFQjCNHal08usOQ5e5CAQsck2yGsTYeGVQ">Britney ' +
        'and Jamie Lynn Spears: Alligator Alley!</a> <font size="-1" color=' +
        '"#6f6f6f"><nobr>The Gossip Girls</nobr></font></font><br><font size=' +
        '"-1"><a href="http://foodconsumer.org/7777/8888/Other_N_ews_51/111101' +
        '362008_Allergy_incident_could_spell_custody_trouble_for_Britney_Spear' +
        's.shtml" id="s-2lMNDY4joOprVvkkY_b-6A:u-AFQjCNGAeFNutMEbSg5zAvrh5reBF' +
        'lqUmA">Allergy incident could spell trouble for Britney Spears</a> ' +
        '<font size="-1" color="#6f6f6f"><nobr>Food Consumer</nobr></font>' +
        '</font><br><font class="p" size="-1"><a href="http://www.people.com/' +
        'people/article/0,,20239458,00.html" id="s-x9thwVUYVET0ZJOnkkcsjw:u-A' +
        'FQjCNE99eijVIrezr9AFRjLkmo5j_Jr7A"><nobr>People Magazine</nobr></a>&nb' +
        'sp;- <a href="http://www.eonline.com/uberblog/b68226_hospital_run_cou' +
        'ld_cost_britney_custody.html" id="s-kYt5LHDhlDnhUL9kRLuuwA:u-AFQjCNF8' +
        '8eOy2utriYuF0icNrZQPzwK8gg"><nobr>E! Online</nobr></a>&nbsp;- <a href' +
        '="http://justjared.buzznet.com/2008/11/11/britney-spears-alligator-fa' +
        'rm/" id="s--VDy1fyacNvaRo_aXb02Dw:u-AFQjCNEn0Rz3wg0PMwDdzKTDug-9k5W6y' +
        'g"><nobr>Just Jared</nobr></a>&nbsp;- <a href="http://www.efluxmedia.' +
        'com/news_Britney_Spears_Son_Released_from_Hospital_28696.html" id="s-' +
        '8oX6hVDe4Qbcl1x5Rua_EA:u-AFQjCNEpn3nOHA8EB0pxJAPf6diOicMRDg"><nobr>eF' +
        'luxMedia</nobr></a></font><br><font class="p" size="-1"><a class="p" ' +
        'href="http://news.google.com/news?ncl=1268233361&amp;hl=en"><nobr><b>' +
        'all 950 news articles&nbsp;�</b></nobr></a></font>';
    // Select it all.
    Range.createFromNodeContents(div).select();

    expectedFailures
        .run(/**
                @suppress {visibility} suppression added to enable type
                checking
              */
             () => {
               FORMATTER.removeFormatting_();
               // Leave links and images alone, remove all other formatting.
               assertHTMLEquals(
                   '<br><br><a href="http://www.washingtonpost.com/wp-dyn/' +
                       'content/article/2008/11/11/AR2008111101090.html"><img src="http://n' +
                       'ews.google.com/news?imgefp=4LFiNNP62TgJ&amp;imgurl=media3.washingto' +
                       'npost.com/wp-dyn/content/photo/2008/11/11/PH2008111101091.jpg"><br>' +
                       'Washington Post</a><br><a href="http://www.nme.com/news/britney-spe' +
                       'ars/40995">Britney\'s son released from hospital</a><br>NME.com - 5' +
                       '3 minutes ago<br>Britney Spears� youngest son Jayden James has been' +
                       ' released from hospital, having been admitted on Sunday after suffe' +
                       'ring a severe reaction to something he ingested.<br><a href="http:/' +
                       '/www.celebrity-gossip.net/celebrities/hollywood/britney-and-jamie-l' +
                       'ynn-spears-alligator-alley-208944/">Britney and Jamie Lynn Spears: ' +
                       'Alligator Alley!</a> The Gossip Girls<br><a href="http://foodconsum' +
                       'er.org/7777/8888/Other_N_ews_51/111101362008_Allergy_incident_could' +
                       '_spell_custody_trouble_for_Britney_Spears.shtml">Allergy incident c' +
                       'ould spell trouble for Britney Spears</a> Food Consumer<br><a href=' +
                       '"http://www.people.com/people/article/0,,20239458,00.html">People M' +
                       'agazine</a> - <a href="http://www.eonline.com/uberblog/b68226_hospi' +
                       'tal_run_could_cost_britney_custody.html">E! Online</a> - <a href="h' +
                       'ttp://justjared.buzznet.com/2008/11/11/britney-spears-alligator-far' +
                       'm/">Just Jared</a> - <a href="http://www.efluxmedia.com/news_Britne' +
                       'y_Spears_Son_Released_from_Hospital_28696.html">eFluxMedia</a><br><' +
                       'a href="http://news.google.com/news?ncl=1268233361&amp;hl=en">all 9' +
                       '50 news articles �</a>' + insertImageFontGarbage,
                   div.innerHTML);
               FIELDMOCK.$verify();
             });
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRangeDelimitedByRanges() {
    const abcde = dom.getElement('abcde').firstChild;
    const start = Range.createFromNodes(abcde, 1, abcde, 2);
    const end = Range.createFromNodes(abcde, 3, abcde, 4);

    testingDom.assertRangeEquals(
        abcde, 1, abcde, 4,
        RemoveFormatting.createRangeDelimitedByRanges_(start, end));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetTableAncestor() {
    const div = document.getElementById('html');

    div.innerHTML = 'foo<table><tr><td>foo</td></tr></table>bar';
    assertTrue(
        'Full table is in table',
        !!FORMATTER.getTableAncestor_(div.childNodes[1]));

    assertFalse(
        'Outside of table', !!FORMATTER.getTableAncestor_(div.firstChild));

    assertTrue(
        'Table cell is in table',
        !!FORMATTER.getTableAncestor_(
            div.childNodes[1].firstChild.firstChild.firstChild));

    dom.setTextContent(div, 'foo');
    assertNull(
        'No table inside field.',
        FORMATTER.getTableAncestor_(div.childNodes[0]));
  },

  /**
   * @bug 1272905
   * @suppress {visibility} suppression added to enable type
   *      checking
   */
  testHardReturnsInHeadersPreserved() {
    const div = document.getElementById('html');
    div.innerHTML = '<h1>abcd</h1><h2>efgh</h2><h3>ijkl</h3>';

    // Select efgh.
    Range.createFromNodeContents(div.childNodes[1]).select();
    FORMATTER.removeFormatting_();

    expectedFailures.expectFailureFor(
        userAgent.IE, 'Proper behavior not yet implemented for IE.');
    expectedFailures.expectFailureFor(
        userAgent.WEBKIT, 'Proper behavior not yet implemented for WebKit.');
    expectedFailures.run(() => {
      assertHTMLEquals('<h1>abcd</h1><br>efgh<h3>ijkl</h3>', div.innerHTML);
    });

    // Select ijkl.
    Range.createFromNodeContents(div.lastChild).select();
    FORMATTER.removeFormatting_();

    expectedFailures.expectFailureFor(
        userAgent.IE, 'Proper behavior not yet implemented for IE.');
    expectedFailures.expectFailureFor(
        userAgent.WEBKIT, 'Proper behavior not yet implemented for WebKit.');
    expectedFailures.run(() => {
      assertHTMLEquals('<h1>abcd</h1><br>efgh<br>ijkl', div.innerHTML);
    });

    // Select abcd.
    Range.createFromNodeContents(div.firstChild).select();
    FORMATTER.removeFormatting_();

    expectedFailures.expectFailureFor(
        userAgent.IE, 'Proper behavior not yet implemented for IE.');
    expectedFailures.expectFailureFor(
        userAgent.WEBKIT, 'Proper behavior not yet implemented for WebKit.');
    expectedFailures.run(() => {
      assertHTMLEquals('<br>abcd<br>efgh<br>ijkl', div.innerHTML);
    });
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testKeyboardShortcut_space() {
    FIELDMOCK.$reset();

    if (!userAgent.MAC) {
      FIELDMOCK.execCommand(RemoveFormatting.REMOVE_FORMATTING_COMMAND);
    }

    FIELDMOCK.$replay();

    const e = {};
    const key = ' ';
    const result = FORMATTER.handleKeyboardShortcut(e, key, true);
    assertEquals(!userAgent.MAC, result);

    FIELDMOCK.$verify();
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testKeyboardShortcut_backslash() {
    FIELDMOCK.$reset();

    FIELDMOCK.execCommand(RemoveFormatting.REMOVE_FORMATTING_COMMAND);

    FIELDMOCK.$replay();

    const e = {};
    const key = '\\';
    const result = FORMATTER.handleKeyboardShortcut(e, key, true);
    assertTrue(result);

    FIELDMOCK.$verify();
  },

  testKeyboardShortcut_other() {
    FIELDMOCK.$reset();
    FIELDMOCK.$replay();

    const e = {};
    const key = 'x';
    const result = FORMATTER.handleKeyboardShortcut(e, key, true);
    assertFalse(result);

    FIELDMOCK.$verify();
  },

  testKeyboardShortcut_withBothModifierKeys() {
    FIELDMOCK.$reset();
    FIELDMOCK.$replay();

    const e = {};
    e.metaKey = true;
    e.ctrlKey = true;
    const key = ' ';
    const result = FORMATTER.handleKeyboardShortcut(e, key, true);
    assertFalse(result);

    FIELDMOCK.$verify();
  },

  testKeyboardShortcut_withMetaKeyAndShiftKey() {
    FIELDMOCK.$reset();
    FIELDMOCK.$replay();

    const e = {};
    e.metaKey = true;
    e.shiftKey = true;
    const key = ' ';
    const result = FORMATTER.handleKeyboardShortcut(e, key, true);
    assertFalse(result);

    FIELDMOCK.$verify();
  },

  testKeyboardShortcut_withCtrlKeyAndShiftKey() {
    FIELDMOCK.$reset();
    FIELDMOCK.$replay();

    const e = {};
    e.ctrlKey = true;
    e.shiftKey = true;
    const key = ' ';
    const result = FORMATTER.handleKeyboardShortcut(e, key, true);
    assertFalse(result);

    FIELDMOCK.$verify();
  },
});
