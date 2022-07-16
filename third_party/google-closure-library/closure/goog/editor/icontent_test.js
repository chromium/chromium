/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.icontentTest');
goog.setTestOnly();

const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const FieldFormatInfo = goog.require('goog.editor.icontent.FieldFormatInfo');
const FieldStyleInfo = goog.require('goog.editor.icontent.FieldStyleInfo');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const icontent = goog.require('goog.editor.icontent');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let wrapperDiv;
let realIframe;
let realIframeDoc;
let propertyReplacer;

/**
 * Check a given body for the most basic properties that all iframes must have.
 * @param {Element} body The actual body element
 * @param {string} id The expected id
 * @param {string} bodyHTML The expected innerHTML
 * @param {boolean=} rtl If true, expect RTL directionality
 */
function assertBodyCorrect(body, id, bodyHTML, rtl = undefined) {
  assertEquals(bodyHTML, body.innerHTML.toString());
  // We can't just check
  // assert(HAS_CONTENTE_EDITABLE, !!body.contentEditable) since in
  // FF 3 we don't currently use contentEditable, but body.contentEditable
  // = 'inherit' and !!'inherit' = true.
  if (BrowserFeature.HAS_CONTENT_EDITABLE) {
    assertEquals('true', String(body.contentEditable));
  } else {
    assertNotEquals('true', String(body.contentEditable));
  }
  assertContains('editable', body.className.match(/\S+/g));
  assertEquals('true', String(body.getAttribute('g_editable')));
  assertEquals(
      'true',
      // IE has bugs with getAttribute('hideFocus'), and
      // Webkit has bugs with normal .hideFocus access.
      String(userAgent.IE ? body.hideFocus : body.getAttribute('hideFocus')));
  assertEquals(id, body.id);
}

/** @return {!Object} A mock document */
function createMockDocument() {
  return {
    body: {
      tagName: 'BODY',
      setAttribute: function(key, val) {
        /** @suppress {globalThis} suppression added to enable type checking */
        this[key] = val;
      },
      getAttribute: /**
                       @suppress {globalThis} suppression added to enable type
                       checking
                     */
          function(key) {
            return this[key];
          },
      style: {direction: ''},
    },
  };
}
testSuite({
  setUp() {
    wrapperDiv = dom.createDom(
        TagName.DIV, null, realIframe = dom.createDom(TagName.IFRAME));
    dom.appendChild(document.body, wrapperDiv);
    realIframeDoc = realIframe.contentWindow.document;
    propertyReplacer = new PropertyReplacer();
  },

  tearDown() {
    dom.removeNode(wrapperDiv);
    propertyReplacer.reset();
  },

  /**
     @suppress {checkTypes,strictMissingProperties} suppression added to enable
     type checking
   */
  testWriteHttpsInitialIframeContent() {
    // This is not a particularly useful test; it's just a sanity check to make
    // sure nothing explodes
    const info = new FieldFormatInfo('id', false, false, false);
    const doc = createMockDocument();
    icontent.writeHttpsInitialIframe(info, doc, 'some html');
    assertBodyCorrect(doc.body, 'id', 'some html');
  },

  /**
     @suppress {checkTypes,strictMissingProperties} suppression added to enable
     type checking
   */
  testWriteHttpsInitialIframeContentRtl() {
    const info = new FieldFormatInfo('id', false, false, true);
    const doc = createMockDocument();
    icontent.writeHttpsInitialIframe(info, doc, 'some html');
    assertBodyCorrect(doc.body, 'id', 'some html', true);
  },

  testWriteInitialIframeContentBlendedStandardsGrowing() {
    if (BrowserFeature.HAS_CONTENT_EDITABLE) {
      return;  // only executes when using an iframe
    }

    const info = new FieldFormatInfo('id', true, true, false);
    const styleInfo = new FieldStyleInfo(
        wrapperDiv, '.MyClass { position: absolute; top: 500px; }');
    const doc = realIframeDoc;
    const html = '<div class="MyClass">Some Html</div>';
    icontent.writeNormalInitialBlendedIframe(info, html, styleInfo, realIframe);

    assertBodyCorrect(doc.body, 'id', html);
    assertEquals('CSS1Compat', doc.compatMode);              // standards
    assertEquals('auto', doc.documentElement.style.height);  // growing
    assertEquals('100%', doc.body.style.height);             // standards
    assertEquals('hidden', doc.body.style.overflowY);        // growing
    assertEquals('', realIframe.style.position);  // no padding on wrapper

    assertEquals(500, doc.body.firstChild.offsetTop);
    assert(
        dom.getElementsByTagName(TagName.STYLE, doc)[0].innerHTML.indexOf(
            '-moz-force-broken-image-icon') != -1);  // standards
  },

  testWriteInitialIframeContentBlendedQuirksFixedRtl() {
    if (BrowserFeature.HAS_CONTENT_EDITABLE) {
      return;  // only executes when using an iframe
    }

    const info = new FieldFormatInfo('id', false, true, true);
    const styleInfo = new FieldStyleInfo(wrapperDiv, '');
    wrapperDiv.style.padding = '2px 5px';
    const doc = realIframeDoc;
    const html = 'Some Html';
    icontent.writeNormalInitialBlendedIframe(info, html, styleInfo, realIframe);

    assertBodyCorrect(doc.body, 'id', html, true);
    assertEquals('BackCompat', doc.compatMode);              // quirks
    assertEquals('100%', doc.documentElement.style.height);  // fixed height
    assertEquals('auto', doc.body.style.height);             // quirks
    assertEquals('auto', doc.body.style.overflow);           // fixed height

    assertEquals('-2px', realIframe.style.marginTop);
    assertEquals('-5px', realIframe.style.marginLeft);
    assert(
        dom.getElementsByTagName(TagName.STYLE, doc)[0].innerHTML.indexOf(
            '-moz-force-broken-image-icon') == -1);  // quirks
  },

  testWhiteboxStandardsFixedRtl() {
    const info = new FieldFormatInfo('id', true, false, true);
    const styleInfo = null;
    const doc = realIframeDoc;
    const html = 'Some Html';
    icontent.writeNormalInitialBlendedIframe(info, html, styleInfo, realIframe);
    assertBodyCorrect(doc.body, 'id', html, true);

    // TODO(nicksantos): on Safari, there's a bug where all written iframes
    // are CSS1Compat. It's fixed in the nightlies as of 3/31/08, so remove
    // this guard when the latest version of Safari is on the farm.
    if (!userAgent.WEBKIT) {
      assertEquals(
          'BackCompat', doc.compatMode);  // always use quirks in whitebox
    }
  },

  testGetInitialIframeContent() {
    const info = new FieldFormatInfo('id', true, false, false);
    const styleInfo = null;
    const html = 'Some Html';
    propertyReplacer.set(BrowserFeature, 'HAS_CONTENT_EDITABLE', false);
    /** @suppress {visibility} suppression added to enable type checking */
    let htmlOut = icontent.getInitialIframeContent_(info, html, styleInfo);
    assertEquals(/contentEditable/i.test(htmlOut), false);
    propertyReplacer.set(BrowserFeature, 'HAS_CONTENT_EDITABLE', true);
    /** @suppress {visibility} suppression added to enable type checking */
    htmlOut = icontent.getInitialIframeContent_(info, html, styleInfo);
    assertEquals(/<body[^>]+?contentEditable/i.test(htmlOut), true);
    assertEquals(/<html[^>]+?style="[^>"]*min-width:\s*0/i.test(htmlOut), true);
    assertEquals(/<body[^>]+?style="[^>"]*min-width:\s*0/i.test(htmlOut), true);
  },

  testIframeMinWidthOverride() {
    if (BrowserFeature.HAS_CONTENT_EDITABLE) {
      return;  // only executes when using an iframe
    }

    const info = new FieldFormatInfo('id', true, true, false);
    const styleInfo = new FieldStyleInfo(
        wrapperDiv, '.MyClass { position: absolute; top: 500px; }');
    const doc = realIframeDoc;
    const html = '<div class="MyClass">Some Html</div>';
    icontent.writeNormalInitialBlendedIframe(info, html, styleInfo, realIframe);

    // Make sure that the minimum width isn't being inherited from the parent
    // document's style.
    assertTrue(doc.body.offsetWidth < 700);
  },

  testBlendedStandardsGrowingMatchesComparisonDiv() {
    // TODO(nicksantos): If we ever move
    // TR_EditableUtil.prototype.makeIframeField_
    // into goog.editor.icontent (and I think we should), we could actually run
    // functional tests to ensure that the iframed field matches the dimensions
    // of the equivalent uneditable div. Functional tests would help a lot here.
  },
});
