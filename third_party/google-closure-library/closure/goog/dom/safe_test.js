/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for safe. */

goog.module('goog.dom.safeTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const InsertAdjacentHtmlPosition = goog.require('goog.dom.safe.InsertAdjacentHtmlPosition');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SafeScript = goog.require('goog.html.SafeScript');
const SafeStyle = goog.require('goog.html.SafeStyle');
const SafeUrl = goog.require('goog.html.SafeUrl');
const TagName = goog.require('goog.dom.TagName');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const asserts = goog.require('goog.asserts');
const dom = goog.require('goog.dom');
const googString = goog.require('goog.string');
const googTesting = goog.require('goog.testing');
const safe = goog.require('goog.dom.safe');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const userAgent = goog.require('goog.userAgent');

let mockWindowOpen;

/**
 * Returns a link element, incorrectly typed as a Location.
 * @return {!Location}
 * @suppress {checkTypes}
 */
function makeLinkElementTypedAsLocation() {
  return document.createElement('LINK');
}

/**
 * Tests that f raises an AssertionError and runs f while disabling assertion
 * errors.
 * @param {function():*} f function with a failing assertion.
 * @return {*} the return value of f.
 */
function withAssertionFailure(f) {
  assertThrows(f);
  asserts.setErrorHandler((error) => {});
  try {
    return f();
  } finally {
    asserts.setErrorHandler(asserts.DEFAULT_ERROR_HANDLER);
  }
}
testSuite({
  tearDown() {
    if (mockWindowOpen) {
      mockWindowOpen.$tearDown();
    }
  },

  testInsertAdjacentHtml() {
    let writtenHtml;
    let writtenPosition;
    const mockNode = /** @type {!Node} */ ({
      'insertAdjacentHTML': function(position, html) {
        writtenPosition = position;
        writtenHtml = html.toString();
      },
    });

    safe.insertAdjacentHtml(
        mockNode, InsertAdjacentHtmlPosition.BEFOREBEGIN,
        SafeHtml.create('div', {}, 'foobar'));
    assertEquals('<div>foobar</div>', writtenHtml);
    assertEquals('beforebegin', writtenPosition);
  },

  testSetInnerHtml() {
    const mockElement =
        /** @type {!Element} */ ({'tagName': 'DIV', 'innerHTML': 'blarg'});
    const html = '<script>somethingTrusted();<' +
        '/script>';
    const safeHtml = testing.newSafeHtmlForTest(html);
    safe.setInnerHtml(mockElement, safeHtml);
    assertEquals(html, mockElement.innerHTML.toString());
  },

  testSetInnerHtml_doesntAllowScript() {
    const script =
        /** @type {!Element} */ ({'tagName': 'SCRIPT', 'innerHTML': 'blarg'});
    const safeHtml = SafeHtml.htmlEscape('alert(1);');
    assertThrows(() => {
      safe.setInnerHtml(script, safeHtml);
    });
  },

  testSetInnerHtml_doesntAllowStyle() {
    const style =
        /** @type {!Element} */ ({'tagName': 'STYLE', 'innerHTML': 'blarg'});
    const safeHtml = SafeHtml.htmlEscape('A { color: red; }');
    assertThrows(() => {
      safe.setInnerHtml(style, safeHtml);
    });
  },

  /**
   * When innerHTML is assigned on an element in IE, IE recursively severs all
   * parent-children links in the removed content. This test ensures that that
   * doesn't happen when re-rendering an element with soy.
   */
  testSetInnerHtml_leavesChildrenInIE() {
    // Given a div with existing content.
    const grandchildDiv = dom.createElement(TagName.DIV);
    const childDiv = dom.createDom(TagName.DIV, null, [grandchildDiv]);
    const testDiv = dom.createDom(TagName.DIV, null, [childDiv]);
    // Expect parent/children links.
    assertArrayEquals(
        'Expect testDiv to contain childDiv.', [childDiv],
        Array.from(testDiv.children));
    assertEquals(
        'Expect childDiv to be contained in testDiv.', testDiv,
        childDiv.parentElement);
    assertArrayEquals(
        'Expect childDiv to contain grandchildDiv.', [grandchildDiv],
        Array.from(childDiv.children));
    assertEquals(
        'Expect grandchildDiv to be contained in childDiv.', childDiv,
        grandchildDiv.parentElement);

    // When the div's content is re-rendered.
    const safeHtml = testing.newSafeHtmlForTest('<a></a>');
    safe.setInnerHtml(testDiv, safeHtml);
    assertEquals(
        `Expect testDiv's contents to complete change`, '<a></a>',
        testDiv.innerHTML.toLowerCase());
    // Expect the previous childDiv tree to retain its parent-child connections.
    assertArrayEquals(
        'Expect childDiv to still contain grandchildDiv.', [grandchildDiv],
        Array.from(childDiv.children));
    assertEquals(
        'Expect grandchildDiv to still be contained in childDiv.', childDiv,
        grandchildDiv.parentElement);
  },

  testSetInnerHtmlFromConstant() {
    const element = document.createElement('div');
    const html = '<b>c</b>';
    safe.setInnerHtmlFromConstant(element, Const.from(html));
    assertEquals(html, element.innerHTML);
  },

  testSetStyle() {
    const style = SafeStyle.fromConstant(Const.from('color: red;'));
    const elem = document.createElement('div');
    assertEquals('', elem.style.color);  // sanity check

    safe.setStyle(elem, style);
    assertEquals('red', elem.style.color);
  },

  testDocumentWrite() {
    const mockDoc = /** @type {!Document} */ ({
      'html': null,
      /** @suppress {globalThis} */
      'write': function(html) {
        this['html'] = html.toString();
      },
    });
    const html = '<script>somethingTrusted();<' +
        '/script>';
    const safeHtml = testing.newSafeHtmlForTest(html);
    safe.documentWrite(mockDoc, safeHtml);
    assertEquals(html, mockDoc.html);
  },

  testsetLinkHrefAndRel_trustedResourceUrl() {
    const mockLink =
        /** @type {!HTMLLinkElement} */ (
            {'href': null, 'rel': null, setAttribute: () => {}});

    const url =
        TrustedResourceUrl.fromConstant(Const.from('javascript:trusted();'));
    // Test case-insensitive too.
    safe.setLinkHrefAndRel(mockLink, url, 'foo, Stylesheet, bar');
    assertEquals('javascript:trusted();', mockLink.href);

    safe.setLinkHrefAndRel(mockLink, url, 'foo, bar');
    assertEquals('javascript:trusted();', mockLink.href);
  },

  testsetLinkHrefAndRel_safeUrl() {
    const mockLink =
        /** @type {!HTMLLinkElement} */ (
            {'href': null, 'rel': null, setAttribute: () => {}});

    const url = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    assertThrows(() => {
      safe.setLinkHrefAndRel(mockLink, url, 'foo, stylesheet, bar');
    });

    safe.setLinkHrefAndRel(mockLink, url, 'foo, bar');
    assertEquals('javascript:trusted();', mockLink.href);
  },

  testsetLinkHrefAndRel_string() {
    const mockLink =
        /** @type {!HTMLLinkElement} */ (
            {'href': null, 'rel': null, setAttribute: () => {}});

    assertThrows(() => {
      safe.setLinkHrefAndRel(
          mockLink, 'javascript:evil();', 'foo, stylesheet, bar');
    });
    withAssertionFailure(() => {
      safe.setLinkHrefAndRel(mockLink, 'javascript:evil();', 'foo, bar');
    });
    assertEquals('about:invalid#zClosurez', mockLink.href);
  },

  testsetLinkHrefAndRel_assertsType() {
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('A');
      const ex = assertThrows(() => {
        safe.setLinkHrefAndRel(
            /** @type {!HTMLLinkElement} */ (otherElement),
            'http://example.com/', 'author');
      });
      assert(
          googString.contains(ex.message, 'Argument is not a HTMLLinkElement'));
    }
  },

  testSetLocationHref() {
    let mockLoc = /** @type {!Location} */ ({'href': 'blarg'});
    withAssertionFailure(() => {
      safe.setLocationHref(mockLoc, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', mockLoc.href);

    mockLoc = /** @type {!Location} */ ({'href': 'blarg'});
    const safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    safe.setLocationHref(mockLoc, safeUrl);
    assertEquals('javascript:trusted();', mockLoc.href);

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const ex = assertThrows(() => {
        safe.setLocationHref(makeLinkElementTypedAsLocation(), safeUrl);
      });
      assert(googString.contains(ex.message, 'Argument is not a Location'));
    }
  },

  testReplaceLocationSafeString() {
    // TODO(bangert): the mocks don't work on IE 8
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      /** @type {?} */
      const mockLoc = new googTesting.StrictMock(window.location);
      mockLoc.replace('http://example.com/');
      mockLoc.$replay();
      safe.replaceLocation(mockLoc, 'http://example.com/');
      mockLoc.$verify();
      mockLoc.$reset();
    }
  },

  testReplaceLocationEvilString() {
    // TODO(bangert): the mocks don't work on IE 8
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      /** @type {?} */
      const mockLoc = new googTesting.StrictMock(window.location);
      mockLoc.replace('about:invalid#zClosurez');
      mockLoc.$replay();
      withAssertionFailure(() => {
        safe.replaceLocation(mockLoc, 'javascript:evil();');
      });
      mockLoc.$verify();
      mockLoc.$reset();
    }
  },

  testReplaceLocationSafeUrl() {
    // TODO(bangert): the mocks don't work on IE 8
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
      /** @type {?} */
      const mockLoc = new googTesting.StrictMock(window.location);
      mockLoc.replace('javascript:trusted();');
      mockLoc.$replay();
      safe.replaceLocation(mockLoc, safeUrl);
      mockLoc.$verify();
      mockLoc.$reset();
    }
  },

  testAssignLocationSafeString() {
    let location;
    const fakeLoc = /** @type {!Location} */ ({
      assign: function(value) {
        location = value;
      },
    });
    safe.assignLocation(fakeLoc, 'http://example.com/');
    assertEquals(location, 'http://example.com/');
  },

  testAssignLocationEvilString() {
    let location;
    const fakeLoc = /** @type {!Location} */ ({
      assign: function(value) {
        location = value;
      },
    });
    withAssertionFailure(() => {
      safe.assignLocation(fakeLoc, 'javascript:evil();');
    });
    assertEquals(location, 'about:invalid#zClosurez');
  },

  testAssignLocationSafeUrl() {
    let location;
    const fakeLoc = /** @type {!Location} */ ({
      assign: function(value) {
        location = value;
      },
    });
    const safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    safe.assignLocation(fakeLoc, safeUrl);
    assertEquals(location, 'javascript:trusted();');
  },

  testSetAnchorHref() {
    let anchor =
        /** @type {!HTMLAnchorElement} */ (document.createElement('A'));
    withAssertionFailure(() => {
      safe.setAnchorHref(anchor, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', anchor.href);

    anchor = /** @type {!HTMLAnchorElement} */ (document.createElement('A'));
    let safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    safe.setAnchorHref(anchor, safeUrl);
    assertEquals('javascript:trusted();', anchor.href);

    // Works with mocks too.
    let mockAnchor = /** @type {!HTMLAnchorElement} */ ({'href': 'blarg'});
    withAssertionFailure(() => {
      safe.setAnchorHref(mockAnchor, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', mockAnchor.href);

    mockAnchor = /** @type {!HTMLAnchorElement} */ ({'href': 'blarg'});
    safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    safe.setAnchorHref(mockAnchor, safeUrl);
    assertEquals('javascript:trusted();', mockAnchor.href);

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('LINK');
      const ex = assertThrows(() => {
        safe.setAnchorHref(
            /** @type {!HTMLAnchorElement} */ (otherElement), safeUrl);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLAnchorElement'));
    }
  },

  testSetInputFormActionHarmlessString() {
    const element = dom.createElement(TagName.INPUT);
    safe.setInputFormAction(element, 'http://foo.com/');
    assertEquals('http://foo.com/', element.formAction);
  },

  testSetInputFormActionEvilString() {
    const element = dom.createElement(TagName.INPUT);
    withAssertionFailure(() => {
      safe.setInputFormAction(element, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', element.formAction);
  },

  testSetInputFormActionSafeUrl() {
    const element = dom.createElement(TagName.INPUT);
    safe.setInputFormAction(
        element, SafeUrl.fromConstant(Const.from('javascript:trusted();')));
    assertEquals('javascript:trusted();', element.formAction);
  },

  testSetInputFormActionAssertsType() {
    /** @type {?} */
    const element = dom.createElement(TagName.FORM);
    withAssertionFailure(() => {
      safe.setInputFormAction(element, 'foo');
    });
    assertEquals('foo', element.formAction);
  },

  testSetButtonFormActionHarmlessString() {
    const element = dom.createElement(TagName.BUTTON);
    safe.setButtonFormAction(element, 'http://foo.com/');
    assertEquals('http://foo.com/', element.formAction);
  },

  testSetButtonFormActionEvilString() {
    const element = dom.createElement(TagName.BUTTON);
    withAssertionFailure(() => {
      safe.setButtonFormAction(element, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', element.formAction);
  },

  testSetButtonFormActionSafeUrl() {
    const element = dom.createElement(TagName.BUTTON);
    safe.setButtonFormAction(
        element, SafeUrl.fromConstant(Const.from('javascript:trusted();')));
    assertEquals('javascript:trusted();', element.formAction);
  },

  testSetFormElementActionAssertsType() {
    /** @type {?} */
    const element = dom.createElement(TagName.INPUT);
    withAssertionFailure(() => {
      safe.setFormElementAction(element, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', element.action);
  },

  testSetFormElementActionHarmlessString() {
    const element = dom.createElement(TagName.FORM);
    safe.setFormElementAction(element, 'http://foo.com');
    assertEquals('http://foo.com/', element.action);  // url is normalized
  },

  testSetFormElementActionEvilString() {
    const element = dom.createElement(TagName.FORM);
    withAssertionFailure(() => {
      safe.setFormElementAction(element, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', element.action);
  },

  testSetFormElementActionSafeUrl() {
    const element = dom.createElement(TagName.FORM);
    safe.setFormElementAction(
        element, SafeUrl.fromConstant(Const.from('javascript:trusted();')));
    assertEquals('javascript:trusted();', element.action);
  },

  testSetImageSrc_withSafeUrlObject() {
    let mockImageElement = /** @type {!HTMLImageElement} */ ({'src': 'blarg'});
    withAssertionFailure(() => {
      safe.setImageSrc(mockImageElement, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', mockImageElement.src);

    mockImageElement = /** @type {!HTMLImageElement} */ ({'src': 'blarg'});
    const safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    safe.setImageSrc(mockImageElement, safeUrl);
    assertEquals('javascript:trusted();', mockImageElement.src);

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('SCRIPT');
      const ex = assertThrows(() => {
        safe.setImageSrc(
            /** @type {!HTMLImageElement} */ (otherElement), safeUrl);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLImageElement'));
    }
  },

  testSetImageSrc_withHttpsUrl() {
    const mockImageElement =
        /** @type {!HTMLImageElement} */ ({'src': 'blarg'});

    const safeUrl = 'https://trusted_url';
    safe.setImageSrc(mockImageElement, safeUrl);
    assertEquals(safeUrl, mockImageElement.src);
  },

  testSetImageSrc_withDataUrl() {
    const mockImageElement =
        /** @type {!HTMLImageElement} */ ({'src': 'blarg'});
    const safeUrl = 'data:image/gif;base64,a';
    safe.setImageSrc(mockImageElement, safeUrl);
    assertEquals(safeUrl, mockImageElement.src);
    assertThrows(() => {
      safe.setImageSrc(mockImageElement, 'data:text/plain;base64,a');
    });
    assertThrows(() => {
      safe.setImageSrc(mockImageElement, 'data:image/gif;bad');
    });
  },

  testSetAudioSrc() {
    let mockAudioElement = /** @type {!HTMLAudioElement} */ ({'src': 'blarg'});
    let safeUrl = 'https://trusted_url';
    safe.setAudioSrc(mockAudioElement, safeUrl);
    assertEquals(safeUrl, mockAudioElement.src);

    mockAudioElement = /** @type {!HTMLAudioElement} */ ({'src': 'blarg'});
    withAssertionFailure(() => {
      safe.setAudioSrc(mockAudioElement, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', mockAudioElement.src);

    mockAudioElement = /** @type {!HTMLAudioElement} */ ({'src': 'blarg'});
    safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    safe.setAudioSrc(mockAudioElement, safeUrl);
    assertEquals('javascript:trusted();', mockAudioElement.src);

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('SCRIPT');
      const ex = assertThrows(() => {
        safe.setAudioSrc(
            /** @type {!HTMLAudioElement} */ (otherElement), safeUrl);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLAudioElement'));
    }
  },

  testSetAudioSrc_withDataUrl() {
    const mockAudioElement =
        /** @type {!HTMLAudioElement} */ ({'src': 'blarg'});
    const safeUrl = 'data:audio/mp3;base64,a';
    safe.setAudioSrc(mockAudioElement, safeUrl);
    assertEquals(safeUrl, mockAudioElement.src);
    assertThrows(() => {
      safe.setAudioSrc(mockAudioElement, 'data:image/gif;base64,a');
    });
  },

  testSetVideoSrc() {
    let mockVideoElement = /** @type {!HTMLVideoElement} */ ({'src': 'blarg'});
    let safeUrl = 'https://trusted_url';
    safe.setVideoSrc(mockVideoElement, safeUrl);
    assertEquals(safeUrl, mockVideoElement.src);

    mockVideoElement = /** @type {!HTMLVideoElement} */ ({'src': 'blarg'});
    withAssertionFailure(() => {
      safe.setVideoSrc(mockVideoElement, 'javascript:evil();');
    });
    assertEquals('about:invalid#zClosurez', mockVideoElement.src);

    mockVideoElement = /** @type {!HTMLVideoElement} */ ({'src': 'blarg'});
    safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    safe.setVideoSrc(mockVideoElement, safeUrl);
    assertEquals('javascript:trusted();', mockVideoElement.src);

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('SCRIPT');
      const ex = assertThrows(() => {
        safe.setVideoSrc(
            /** @type {!HTMLVideoElement} */ (otherElement), safeUrl);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLVideoElement'));
    }
  },

  testSetVideoSrc_withDataUrl() {
    const mockVideoElement =
        /** @type {!HTMLVideoElement} */ ({'src': 'blarg'});
    const safeUrl = 'data:video/mp4;base64,a';
    safe.setVideoSrc(mockVideoElement, safeUrl);
    assertEquals(safeUrl, mockVideoElement.src);
    assertThrows(() => {
      safe.setVideoSrc(mockVideoElement, 'data:image/gif;base64,a');
    });
  },

  testSetEmbedSrc() {
    const url =
        TrustedResourceUrl.fromConstant(Const.from('javascript:trusted();'));
    const mockElement = /** @type {!HTMLEmbedElement} */ ({'src': 'blarg'});
    safe.setEmbedSrc(mockElement, url);
    assertEquals('javascript:trusted();', mockElement.src.toString());

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('IMAGE');
      const ex = assertThrows(() => {
        safe.setEmbedSrc(
            /** @type {!HTMLEmbedElement} */ (otherElement), url);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLEmbedElement'));
    }
  },

  testSetFrameSrc() {
    const url =
        TrustedResourceUrl.fromConstant(Const.from('javascript:trusted();'));
    const mockElement = /** @type {!HTMLFrameElement} */ ({'src': 'blarg'});
    safe.setFrameSrc(mockElement, url);
    assertEquals('javascript:trusted();', mockElement.src);

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('IMAGE');
      const ex = assertThrows(() => {
        safe.setFrameSrc(
            /** @type {!HTMLFrameElement} */ (otherElement), url);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLFrameElement'));
    }
  },

  testSetIframeSrc() {
    const url =
        TrustedResourceUrl.fromConstant(Const.from('javascript:trusted();'));
    const mockElement = /** @type {!HTMLIFrameElement} */ ({'src': 'blarg'});
    safe.setIframeSrc(mockElement, url);
    assertEquals('javascript:trusted();', mockElement.src);

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('IMAGE');
      const ex = assertThrows(() => {
        safe.setIframeSrc(
            /** @type {!HTMLIFrameElement} */ (otherElement), url);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLIFrameElement'));
    }
  },

  testSetIframeSrcdoc() {
    const html = SafeHtml.create('div', {}, 'foobar');
    const mockIframe = /** @type {!HTMLIFrameElement} */ ({'srcdoc': ''});
    safe.setIframeSrcdoc(mockIframe, html);
    assertEquals('<div>foobar</div>', mockIframe.srcdoc.toString());

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('IMAGE');
      const ex = assertThrows(() => {
        safe.setIframeSrcdoc(
            /** @type {!HTMLIFrameElement} */ (otherElement), html);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLIFrameElement'));
    }
  },

  testSetObjectData() {
    const url =
        TrustedResourceUrl.fromConstant(Const.from('javascript:trusted();'));
    const mockElement = /** @type {!HTMLObjectElement} */ ({'data': 'blarg'});
    safe.setObjectData(mockElement, url);
    assertEquals('javascript:trusted();', mockElement.data.toString());

    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('IMAGE');
      const ex = assertThrows(() => {
        safe.setObjectData(
            /** @type {!HTMLObjectElement} */ (otherElement), url);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLObjectElement'));
    }
  },

  testSetScriptSrc() {
    const url =
        TrustedResourceUrl.fromConstant(Const.from('javascript:trusted();'));
    const mockElement = /** @type {!HTMLScriptElement} */ ({
      'src': 'blarg',
      /** @suppress {globalThis} */
      'setAttribute': function(attr, value) {
        this[attr] = value;
      },
    });
    let nonce = safe.getScriptNonce();
    if (!nonce) {
      // Place a nonced script in the page.
      nonce = 'ThisIsANonceThisIsANonceThisIsANonce';
    }

    const noncedScript = dom.createElement(TagName.SCRIPT);
    noncedScript.setAttribute('nonce', nonce);
    document.body.appendChild(noncedScript);
    safe.setScriptSrc(mockElement, url);

    try {
      assertEquals('javascript:trusted();', mockElement.src.toString());
      assertEquals(nonce, mockElement.nonce);
    } finally {
      dom.removeNode(noncedScript);
    }
    // Asserts correct runtime type.
    if (!userAgent.IE || userAgent.isVersionOrHigher(10)) {
      const otherElement = document.createElement('IMAGE');
      const ex = assertThrows(() => {
        safe.setScriptSrc(
            /** @type {!HTMLScriptElement} */ (otherElement), url);
      });
      assert(googString.contains(
          ex.message, 'Argument is not a HTMLScriptElement'));
    }
  },

  testSetScriptSrc_withIframe() {
    const url =
        TrustedResourceUrl.fromConstant(Const.from('javascript:trusted();'));
    // create the iframe and set up a script inside the iframe.
    let nonce = safe.getScriptNonce();
    if (!nonce) {
      nonce = 'ThisIsANonceThisIsANonceThisIsANonce';
    }

    const iframe = dom.createElement(TagName.IFRAME);
    document.body.appendChild(iframe);
    const iframeWindow = iframe.contentWindow;
    const iframeDocument = iframeWindow.document;
    iframeDocument.write('<HTML><BODY></BODY></HTML>');
    iframeDocument.close();
    const iframeScript = iframeDocument.createElement('SCRIPT');
    iframeScript.setAttribute('nonce', nonce);
    iframeDocument.body.appendChild(iframeScript);
    const mockElement = /** @type {!HTMLScriptElement} */ ({
      'src': 'blarg',
      /** @suppress {globalThis} */
      'setAttribute': function(attr, value) {
        this[attr] = value;
      },
      ownerDocument: {defaultView: iframeWindow}
    });
    safe.setScriptSrc(mockElement, url);
    try {
      assertEquals('javascript:trusted();', mockElement.src.toString());
      assertEquals(nonce, mockElement.nonce);
    } finally {
      dom.removeNode(iframe);
    }
  },

  testSetScriptContent() {
    const mockScriptElement = /** @type {!HTMLScriptElement} */ ({
      /** @suppress {globalThis} */
      'setAttribute': function(attr, value) {
        this[attr] = value;
      },
    });
    // Place a nonced script in the page.
    let nonce = safe.getScriptNonce();
    if (!nonce) {
      nonce = 'ThisIsANonceThisIsANonceThisIsANonce';
    }

    const noncedScript = dom.createElement(TagName.SCRIPT);
    noncedScript.setAttribute('nonce', nonce);
    document.body.appendChild(noncedScript);
    const content = SafeScript.fromConstant(Const.from('alert(1);'));
    safe.setScriptContent(mockScriptElement, content);

    try {
      assertEquals(
          SafeScript.unwrap(content), mockScriptElement.textContent.toString());
      assertEquals(nonce, mockScriptElement.nonce);
    } finally {
      dom.removeNode(noncedScript);
    }
  },

  testSetScriptContentWithSpecialCharacters() {
    const scriptElement = dom.createElement(TagName.SCRIPT);
    document.body.appendChild(scriptElement);
    const TEST_PROPERTY = 'scriptContentTestProperty';
    const content = SafeScript.fromConstant(Const.from(`
      // Comment to ensure newlines are preserved.
      window.${TEST_PROPERTY} = 'tricky<{}>value';
    `));
    safe.setScriptContent(scriptElement, content);

    try {
      assertEquals(SafeScript.unwrap(content), scriptElement.text);
      assertEquals('tricky<{}>value', window[TEST_PROPERTY]);

      // Ensure no <br> tags were inserted into the script tag.
      assertEquals(1, scriptElement.childNodes.length);
    } finally {
      dom.removeNode(scriptElement);
      delete window[TEST_PROPERTY];
    }
  },

  testOpenInWindow() {
    mockWindowOpen =
        /** @type {?} */ (googTesting.createMethodMock(window, 'open'));
    const fakeWindow = {};

    mockWindowOpen('about:invalid#zClosurez', 'name', 'specs')
        .$returns(fakeWindow);
    mockWindowOpen.$replay();
    let retVal = withAssertionFailure(
        () => safe.openInWindow(
            'javascript:evil();', window, Const.from('name'), 'specs'));
    mockWindowOpen.$verify();
    assertEquals(
        'openInWindow should return the created window', fakeWindow, retVal);

    mockWindowOpen.$reset();
    retVal = null;

    const safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    mockWindowOpen('javascript:trusted();', 'name', 'specs')
        .$returns(fakeWindow);
    mockWindowOpen.$replay();
    retVal = safe.openInWindow(safeUrl, window, Const.from('name'), 'specs');
    mockWindowOpen.$verify();
    assertEquals(
        'openInWindow should return the created window', fakeWindow, retVal);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testParseFromStringHtml() {
    if (userAgent.IE && !userAgent.isVersionOrHigher('10')) {
      return;
    }
    const html = SafeHtml.create('A', {'class': 'b'}, 'c');
    const node =
        safe.parseFromStringHtml(new DOMParser(), html).body.firstChild;
    assertEquals('A', node.tagName);
    assertEquals('b', node.className);
    assertEquals('c', node.textContent);
  },

  testParseFromString() {
    if (userAgent.IE && !userAgent.isVersionOrHigher('10')) {
      return;
    }
    const html = SafeHtml.create('a', {'class': 'b'}, 'c');
    const node = safe.parseFromString(new DOMParser(), html, 'application/xml')
                     .firstChild;
    assertEquals('a', node.tagName);
    assertEquals('b', node.getAttribute('class'));
    assertEquals('c', node.textContent);
  },

  testCreateImageFromBlob() {
    const blob = new Blob(['data'], {type: 'image/svg+xml'});
    const fakeObjectUrl = 'blob:http://fakeurl.com';
    const mockCreateObject = /** @type {?} */ (
        googTesting.createMethodMock(window.URL, 'createObjectURL'));
    const mockRevokeObject = /** @type {?} */ (
        googTesting.createMethodMock(window.URL, 'revokeObjectURL'));
    mockCreateObject(blob).$returns(fakeObjectUrl);
    mockRevokeObject(fakeObjectUrl);
    mockCreateObject.$replay();
    mockRevokeObject.$replay();

    const image = safe.createImageFromBlob(blob);

    mockCreateObject.$verify();
    assertEquals('function', typeof image.onload);
    image.onload(null);  // Trigger image load.
    mockRevokeObject.$verify();
    assertEquals(fakeObjectUrl, image.src);
    assertTrue(image instanceof HTMLImageElement);
  },

  testCreateImageFromBlobBadMimeType() {
    // Skip unsupported test if IE9 or lower.
    if (userAgent.IE && !userAgent.isVersionOrHigher('10')) {
      return;
    }
    const blob = new Blob(['data'], {type: 'badmimetype'});
    assertThrows(() => {
      safe.createImageFromBlob(blob);
    });
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCreateContextualFragment() {
    if (userAgent.IE && !userAgent.isVersionOrHigher('11')) {
      return;
    }
    const html = SafeHtml.create('A', {'class': 'b'}, 'c');
    const node = safe.createContextualFragment(
                         /** @type {!Range} */ (document.createRange()), html)
                     .childNodes[0];
    assertEquals('A', node.tagName);
    assertEquals('b', node.className);
    assertEquals('c', node.textContent);
  },

  testGetScriptNonce() {
    assertEquals('CSP+Nonce+For+Tests+Only', safe.getScriptNonce());
  },

  testGetStyleNonce() {
    assertEquals('NONCE', safe.getStyleNonce());
    const style = document.querySelector('style[nonce]');
    style.parentNode.removeChild(style);
    assertEquals('NONCE2', safe.getStyleNonce(window));
  }
});
