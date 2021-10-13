/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for SafeHtml and its builders. */

goog.module('goog.html.safeHtmlTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const Dir = goog.require('goog.i18n.bidi.Dir');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SafeScript = goog.require('goog.html.SafeScript');
const SafeStyle = goog.require('goog.html.SafeStyle');
const SafeStyleSheet = goog.require('goog.html.SafeStyleSheet');
const SafeUrl = goog.require('goog.html.SafeUrl');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const browser = goog.require('goog.labs.userAgent.browser');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const trustedtypes = goog.require('goog.html.trustedtypes');

const stubs = new PropertyReplacer();
const policy = goog.createTrustedTypesPolicy('closure_test');

function assertSameHtml(expected, html) {
  assertEquals(expected, SafeHtml.unwrap(html));
}
testSuite({
  tearDown() {
    stubs.reset();
  },

  testSafeHtml() {
    // TODO(xtof): Consider using SafeHtmlBuilder instead of newSafeHtmlForTest,
    // when available.
    let safeHtml = testing.newSafeHtmlForTest('Hello <em>World</em>');
    assertSameHtml('Hello <em>World</em>', safeHtml);
    assertEquals('Hello <em>World</em>', SafeHtml.unwrap(safeHtml));
    assertEquals('Hello <em>World</em>', String(safeHtml));
    assertNull(safeHtml.getDirection());

    safeHtml = testing.newSafeHtmlForTest('World <em>Hello</em>', Dir.RTL);
    assertSameHtml('World <em>Hello</em>', safeHtml);
    assertEquals('World <em>Hello</em>', SafeHtml.unwrap(safeHtml));
    assertEquals('World <em>Hello</em>', String(safeHtml));
    assertEquals(Dir.RTL, safeHtml.getDirection());

    // Interface markers are present.
    assertTrue(safeHtml.implementsGoogStringTypedString);
    assertTrue(safeHtml.implementsGoogI18nBidiDirectionalString);

    // Pre-defined constant.
    assertSameHtml('', SafeHtml.EMPTY);
    assertSameHtml('<br>', SafeHtml.BR);
  },

  /** @suppress {checkTypes} */
  testUnwrap() {
    const privateFieldName = 'privateDoNotAccessOrElseSafeHtmlWrappedValue_';
    const propNames = googObject.getKeys(SafeHtml.htmlEscape(''));
    assertContains(privateFieldName, propNames);
    const evil = {};
    evil[privateFieldName] = '<script>evil()</script';

    const exception = assertThrows(() => {
      SafeHtml.unwrap(evil);
    });
    assertContains('expected object of type SafeHtml', exception.message);
  },

  testUnwrapTrustedHTML_policyIsNull() {
    stubs.set(trustedtypes, 'getPolicyPrivateDoNotAccessOrElse', function() {
      return null;
    });
    const safeValue = SafeHtml.htmlEscape('HTML');
    const trustedValue = SafeHtml.unwrapTrustedHTML(safeValue);
    assertEquals('string', typeof trustedValue);
    assertEquals(safeValue.getTypedStringValue(), trustedValue);
  },

  testUnwrapTrustedHTML_policyIsSet() {
    stubs.set(trustedtypes, 'getPolicyPrivateDoNotAccessOrElse', function() {
      return policy;
    });
    const safeValue = SafeHtml.htmlEscape('HTML');
    const trustedValue = SafeHtml.unwrapTrustedHTML(safeValue);
    assertEquals(safeValue.getTypedStringValue(), trustedValue.toString());
    assertTrue(
        globalThis.TrustedHTML ? trustedValue instanceof TrustedHTML :
                                 typeof trustedValue === 'string');
  },

  testHtmlEscape() {
    // goog.html.SafeHtml passes through unchanged.
    const safeHtmlIn = SafeHtml.htmlEscape('<b>in</b>');
    assertTrue(safeHtmlIn === SafeHtml.htmlEscape(safeHtmlIn));

    // Plain strings are escaped.
    let safeHtml = SafeHtml.htmlEscape('Hello <em>"\'&World</em>');
    assertSameHtml(
        'Hello &lt;em&gt;&quot;&#39;&amp;World&lt;/em&gt;', safeHtml);
    assertEquals(
        'Hello &lt;em&gt;&quot;&#39;&amp;World&lt;/em&gt;', String(safeHtml));

    // Creating from a SafeUrl escapes and retains the known direction (which is
    // fixed to RTL for URLs).
    const safeUrl =
        SafeUrl.fromConstant(Const.from('http://example.com/?foo&bar'));
    const escapedUrl = SafeHtml.htmlEscape(safeUrl);
    assertSameHtml('http://example.com/?foo&amp;bar', escapedUrl);
    assertEquals(Dir.LTR, escapedUrl.getDirection());

    // Creating SafeHtml from a goog.string.Const escapes as well (i.e., the
    // value is treated like any other string). To create HTML markup from
    // program literals, SafeHtmlBuilder should be used.
    assertSameHtml(
        'this &amp; that', SafeHtml.htmlEscape(Const.from('this & that')));
  },

  testSafeHtmlCreate() {
    const br = SafeHtml.create('br');

    assertSameHtml('<br>', br);

    assertSameHtml(
        '<span title="&quot;"></span>',
        SafeHtml.create('span', {'title': '"'}));

    assertSameHtml('<span>&lt;</span>', SafeHtml.create('span', {}, '<'));

    assertSameHtml('<span><br></span>', SafeHtml.create('span', {}, br));

    assertSameHtml('<span></span>', SafeHtml.create('span', {}, []));

    assertSameHtml(
        '<span></span>',
        SafeHtml.create('span', {'title': null, 'class': undefined}));

    assertSameHtml(
        '<span>x<br>y</span>', SafeHtml.create('span', {}, ['x', br, 'y']));

    assertSameHtml(
        '<table border="0"></table>', SafeHtml.create('table', {'border': 0}));

    const onclick = Const.from('alert(/"/)');
    assertSameHtml(
        '<span onclick="alert(/&quot;/)"></span>',
        SafeHtml.create('span', {'onclick': onclick}));

    const href = testing.newSafeUrlForTest('?a&b');
    assertSameHtml(
        '<a href="?a&amp;b"></a>', SafeHtml.create('a', {'href': href}));

    const style = testing.newSafeStyleForTest('border: /* " */ 0;');
    assertSameHtml(
        '<hr style="border: /* &quot; */ 0;">',
        SafeHtml.create('hr', {'style': style}));

    assertEquals(Dir.NEUTRAL, SafeHtml.create('span').getDirection());
    assertNull(SafeHtml.create('span', {'dir': 'x'}).getDirection());
    assertEquals(
        Dir.NEUTRAL,
        SafeHtml.create('span', {'dir': 'ltr'}, 'a').getDirection());

    assertThrows(() => {
      SafeHtml.create('script');
    });

    assertThrows(() => {
      SafeHtml.create('br', {}, 'x');
    });

    assertThrows(() => {
      SafeHtml.create('img', {'onerror': ''});
    });

    assertThrows(() => {
      SafeHtml.create('img', {'OnError': ''});
    });

    assertThrows(() => {
      SafeHtml.create('a href=""');
    });

    assertThrows(() => {
      SafeHtml.create('a', {'title="" href': ''});
    });

    assertThrows(() => {
      SafeHtml.create('applet');
    });

    assertThrows(() => {
      SafeHtml.create('applet', {'code': 'kittens.class'});
    });

    assertThrows(() => {
      SafeHtml.create('base');
    });

    assertThrows(() => {
      SafeHtml.create('base', {'href': 'http://example.org'});
    });

    assertThrows(() => {
      SafeHtml.create('math');
    });

    assertThrows(() => {
      SafeHtml.create('meta');
    });

    assertThrows(() => {
      SafeHtml.create('svg');
    });
  },

  testSafeHtmlCreate_styleAttribute() {
    stubs.replace(SafeHtml, 'SUPPORT_STYLE_ATTRIBUTE', true);
    const style = 'color:red;';
    const expected = `<hr style="${style}">`;
    assertThrows(() => {
      SafeHtml.create('hr', {'style': style});
    });
    assertSameHtml(expected, SafeHtml.create('hr', {
      'style': SafeStyle.fromConstant(Const.from(style)),
    }));
    assertSameHtml(
        expected, SafeHtml.create('hr', {'style': {'color': 'red'}}));

    stubs.replace(SafeHtml, 'SUPPORT_STYLE_ATTRIBUTE', false);
    assertThrows(() => {
      SafeHtml.create('hr', {'style': {'color': 'red'}});
    });
  },

  testSafeHtmlCreate_urlAttributes() {
    // TrustedResourceUrl is allowed.
    const trustedResourceUrl = TrustedResourceUrl.fromConstant(
        Const.from('https://google.com/trusted'));
    assertSameHtml(
        '<img src="https://google.com/trusted">',
        SafeHtml.create('img', {'src': trustedResourceUrl}));
    // SafeUrl is allowed.
    const safeUrl = SafeUrl.sanitize('https://google.com/safe');
    assertSameHtml(
        '<imG src="https://google.com/safe">',
        SafeHtml.create('imG', {'src': safeUrl}));
    // Const is allowed.
    const constUrl = Const.from('https://google.com/const');
    assertSameHtml(
        '<a href="https://google.com/const"></a>',
        SafeHtml.create('a', {'href': constUrl}));

    // string is allowed but escaped.
    assertSameHtml(
        '<a href="http://google.com/safe&quot;"></a>',
        SafeHtml.create('a', {'href': 'http://google.com/safe"'}));

    // string is allowed but sanitized.
    const badUrl = 'javascript:evil();';
    const sanitizedUrl = SafeUrl.unwrap(SafeUrl.sanitize(badUrl));

    assertTrue(typeof sanitizedUrl == 'string');
    assertNotEquals(badUrl, sanitizedUrl);

    assertSameHtml(
        `<a href="${sanitizedUrl}"></a>`,
        SafeHtml.create('a', {'href': badUrl}));

    // attribute case is ignored for url attributes purposes
    assertSameHtml(
        `<a hReF="${sanitizedUrl}"></a>`,
        SafeHtml.create('a', {'hReF': badUrl}));
  },

  /** @suppress {checkTypes} */
  testSafeHtmlCreateIframe() {
    // Setting src and srcdoc.
    const url = TrustedResourceUrl.fromConstant(
        Const.from('https://google.com/trusted<'));
    assertSameHtml(
        '<iframe src="https://google.com/trusted&lt;"></iframe>',
        SafeHtml.createIframe(url, null, {'sandbox': null}));
    const srcdoc = SafeHtml.BR;
    assertSameHtml(
        '<iframe srcdoc="&lt;br&gt;"></iframe>',
        SafeHtml.createIframe(null, srcdoc, {'sandbox': null}));

    // sandbox default and overriding it.
    assertSameHtml('<iframe sandbox=""></iframe>', SafeHtml.createIframe());
    assertSameHtml(
        '<iframe Sandbox="allow-same-origin allow-top-navigation"></iframe>',
        SafeHtml.createIframe(
            null, null, {'Sandbox': 'allow-same-origin allow-top-navigation'}));

    // Cannot override src and srddoc.
    assertThrows(() => {
      SafeHtml.createIframe(null, null, {'Src': url});
    });
    assertThrows(() => {
      SafeHtml.createIframe(null, null, {'Srcdoc': url});
    });

    // Unsafe src and srcdoc.
    assertThrows(() => {
      SafeHtml.createIframe('http://example.com');
    });
    assertThrows(() => {
      SafeHtml.createIframe(null, '<script>alert(1)</script>');
    });

    // Can set content.
    assertSameHtml(
        '<iframe>&lt;</iframe>',
        SafeHtml.createIframe(null, null, {'sandbox': null}, '<'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSafeHtmlCreateIframe_withMonkeypatchedObjectPrototype() {
    stubs.set(Object.prototype, 'foo', 'bar');
    const url = TrustedResourceUrl.fromConstant(
        Const.from('https://google.com/trusted<'));
    assertSameHtml(
        '<iframe src="https://google.com/trusted&lt;"></iframe>',
        SafeHtml.createIframe(url, null, {'sandbox': null}));
  },

  /** @suppress {checkTypes} */
  testSafeHtmlcreateSandboxIframe() {
    function assertSameHtmlIfSupportsSandbox(
        referenceHtml, testedHtmlFunction) {
      if (!SafeHtml.canUseSandboxIframe()) {
        assertThrows(testedHtmlFunction);
      } else {
        assertSameHtml(referenceHtml, testedHtmlFunction());
      }
    }

    // Setting src and srcdoc.
    const url = SafeUrl.fromConstant(Const.from('https://google.com/trusted<'));
    assertSameHtmlIfSupportsSandbox(
        '<iframe src="https://google.com/trusted&lt;" sandbox=""></iframe>',
        () => SafeHtml.createSandboxIframe(url, null));

    // If set with a string, src is sanitized.
    assertSameHtmlIfSupportsSandbox(
        '<iframe src="' + SafeUrl.INNOCUOUS_STRING + '" sandbox=""></iframe>',
        () => SafeHtml.createSandboxIframe('javascript:evil();', null));

    const srcdoc = '<br>';
    assertSameHtmlIfSupportsSandbox(
        '<iframe srcdoc="&lt;br&gt;" sandbox=""></iframe>',
        () => SafeHtml.createSandboxIframe(null, srcdoc));

    // Cannot override src, srcdoc.
    assertThrows(() => {
      SafeHtml.createSandboxIframe(null, null, {'Src': url});
    });
    assertThrows(() => {
      SafeHtml.createSandboxIframe(null, null, {'Srcdoc': url});
    });

    // Sandboxed by default, and can't be overriden.
    assertSameHtmlIfSupportsSandbox(
        '<iframe sandbox=""></iframe>', () => SafeHtml.createSandboxIframe());

    assertThrows(() => {
      SafeHtml.createSandboxIframe(null, null, {'sandbox': ''});
    });
    assertThrows(() => {
      SafeHtml.createSandboxIframe(null, null, {'SaNdBoX': 'allow-scripts'});
    });
    assertThrows(() => {
      SafeHtml.createSandboxIframe(
          null, null, {'sandbox': 'allow-same-origin allow-top-navigation'});
    });

    // Can set content.
    assertSameHtmlIfSupportsSandbox(
        '<iframe sandbox="">&lt;</iframe>',
        () => SafeHtml.createSandboxIframe(null, null, null, '<'));
  },

  /**
     @suppress {strictPrimitiveOperators} suppression added to enable type
     checking
   */
  testSafeHtmlCanUseIframeSandbox() {
    // We know that the IE < 10 do not support the sandbox attribute, so use
    // them as a reference.
    if (browser.isIE() && browser.getVersion() < 10) {
      assertEquals(false, SafeHtml.canUseSandboxIframe());
    } else {
      assertEquals(true, SafeHtml.canUseSandboxIframe());
    }
  },

  testSafeHtmlCreateScript() {
    const script = SafeScript.fromConstant(Const.from('function1();'));
    let scriptHtml = SafeHtml.createScript(script);
    assertSameHtml('<script>function1();</script>', scriptHtml);

    // Two pieces of script.
    const otherScript = SafeScript.fromConstant(Const.from('function2();'));
    scriptHtml = SafeHtml.createScript([script, otherScript]);
    assertSameHtml('<script>function1();function2();</script>', scriptHtml);

    // Set attribute.
    scriptHtml = SafeHtml.createScript(script, {'id': 'test'});
    assertContains('id="test"', SafeHtml.unwrap(scriptHtml));

    // Set attribute to null.
    scriptHtml = SafeHtml.createScript(SafeScript.EMPTY, {'id': null});
    assertSameHtml('<script></script>', scriptHtml);

    // Set attribute to invalid value.
    let exception = assertThrows(() => {
      SafeHtml.createScript(SafeScript.EMPTY, {'invalid.': 'cantdothis'});
    });
    assertContains('Invalid attribute name', exception.message);

    // Cannot override type attribute.
    exception = assertThrows(() => {
      SafeHtml.createScript(SafeScript.EMPTY, {'Type': 'cantdothis'});
    });
    assertContains('Cannot set "type"', exception.message);

    // Cannot set src attribute.
    exception = assertThrows(() => {
      SafeHtml.createScript(SafeScript.EMPTY, {'src': 'cantdothis'});
    });
    assertContains('Cannot set "src"', exception.message);

    // Directionality.
    assertEquals(Dir.NEUTRAL, scriptHtml.getDirection());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSafeHtmlCreateScript_withMonkeypatchedObjectPrototype() {
    stubs.set(Object.prototype, 'foo', 'bar');
    stubs.set(Object.prototype, 'type', 'baz');
    const scriptHtml = SafeHtml.createScript(SafeScript.EMPTY, {'id': null});
    assertSameHtml('<script></script>', scriptHtml);
  },

  /** @suppress {checkTypes} */
  testSafeHtmlCreateScriptSrc() {
    const url = TrustedResourceUrl.fromConstant(
        Const.from('https://google.com/trusted<'));

    assertSameHtml(
        '<script src="https://google.com/trusted&lt;"></script>',
        SafeHtml.createScriptSrc(url));

    assertSameHtml(
        '<script src="https://google.com/trusted&lt;" defer="defer"></script>',
        SafeHtml.createScriptSrc(url, {'defer': 'defer'}));

    // Unsafe src.
    assertThrows(() => {
      SafeHtml.createScriptSrc('http://example.com');
    });

    // Unsafe attribute.
    assertThrows(() => {
      SafeHtml.createScriptSrc(url, {'onerror': 'alert(1)'});
    });

    // Cannot override src.
    assertThrows(() => {
      SafeHtml.createScriptSrc(url, {'Src': url});
    });
  },

  testSafeHtmlCreateMeta() {
    const url = SafeUrl.fromConstant(Const.from('https://google.com/trusted<'));

    // SafeUrl with no timeout gets properly escaped.
    assertSameHtml(
        '<meta http-equiv="refresh" ' +
            'content="0; url=https://google.com/trusted&lt;">',
        SafeHtml.createMetaRefresh(url));

    // SafeUrl with 0 timeout also gets properly escaped.
    assertSameHtml(
        '<meta http-equiv="refresh" ' +
            'content="0; url=https://google.com/trusted&lt;">',
        SafeHtml.createMetaRefresh(url, 0));

    // Positive timeouts are supported.
    assertSameHtml(
        '<meta http-equiv="refresh" ' +
            'content="1337; url=https://google.com/trusted&lt;">',
        SafeHtml.createMetaRefresh(url, 1337));

    // Negative timeouts are also kept, though they're not correct HTML.
    assertSameHtml(
        '<meta http-equiv="refresh" ' +
            'content="-1337; url=https://google.com/trusted&lt;">',
        SafeHtml.createMetaRefresh(url, -1337));

    // String-based URLs work out of the box.
    assertSameHtml(
        '<meta http-equiv="refresh" ' +
            'content="0; url=https://google.com/trusted&lt;">',
        SafeHtml.createMetaRefresh('https://google.com/trusted<'));

    // Sanitization happens.
    assertSameHtml(
        '<meta http-equiv="refresh" ' +
            'content="0; url=about:invalid#zClosurez">',
        SafeHtml.createMetaRefresh('javascript:alert(1)'));
  },

  testSafeHtmlCreateStyle() {
    const styleSheet =
        SafeStyleSheet.fromConstant(Const.from('P.special { color:"red" ; }'));
    let styleHtml = SafeHtml.createStyle(styleSheet);
    assertSameHtml(
        '<style type="text/css">P.special { color:"red" ; }</style>',
        styleHtml);

    // Two stylesheets.
    const otherStyleSheet =
        SafeStyleSheet.fromConstant(Const.from('P.regular { color:blue ; }'));
    styleHtml = SafeHtml.createStyle([styleSheet, otherStyleSheet]);
    assertSameHtml(
        '<style type="text/css">P.special { color:"red" ; }' +
            'P.regular { color:blue ; }</style>',
        styleHtml);

    // Set attribute.
    styleHtml = SafeHtml.createStyle(styleSheet, {'id': 'test'});
    const styleHtmlString = SafeHtml.unwrap(styleHtml);
    assertContains('id="test"', styleHtmlString);
    assertContains('type="text/css"', styleHtmlString);

    // Set attribute to null.
    styleHtml = SafeHtml.createStyle(SafeStyleSheet.EMPTY, {'id': null});
    assertSameHtml('<style type="text/css"></style>', styleHtml);

    // Set attribute to invalid value.
    let exception = assertThrows(() => {
      SafeHtml.createStyle(SafeStyleSheet.EMPTY, {'invalid.': 'cantdothis'});
    });
    assertContains('Invalid attribute name', exception.message);

    // Cannot override type attribute.
    exception = assertThrows(() => {
      SafeHtml.createStyle(SafeStyleSheet.EMPTY, {'Type': 'cantdothis'});
    });
    assertContains('Cannot override "type"', exception.message);

    // Directionality.
    assertEquals(Dir.NEUTRAL, styleHtml.getDirection());
  },

  testSafeHtmlCreateWithDir() {
    const ltr = Dir.LTR;

    assertEquals(ltr, SafeHtml.createWithDir(ltr, 'br').getDirection());
  },

  testSafeHtmlJoin() {
    const br = SafeHtml.BR;
    assertSameHtml('Hello<br>World', SafeHtml.join(br, ['Hello', 'World']));
    assertSameHtml('Hello<br>World', SafeHtml.join(br, ['Hello', ['World']]));
    assertSameHtml('Hello<br>', SafeHtml.join('Hello', ['', br]));

    const ltr = testing.newSafeHtmlForTest('', Dir.LTR);
    assertEquals(Dir.LTR, SafeHtml.join(br, [ltr, ltr]).getDirection());
  },

  testSafeHtmlConcat() {
    const br = testing.newSafeHtmlForTest('<br>');

    const html = SafeHtml.htmlEscape('Hello');
    assertSameHtml('Hello<br>', SafeHtml.concat(html, br));

    assertSameHtml('', SafeHtml.concat());
    assertSameHtml('', SafeHtml.concat([]));

    assertSameHtml('a<br>c', SafeHtml.concat('a', br, 'c'));
    assertSameHtml('a<br>c', SafeHtml.concat(['a', br, 'c']));
    assertSameHtml('a<br>c', SafeHtml.concat('a', [br, 'c']));
    assertSameHtml('a<br>c', SafeHtml.concat(['a'], br, ['c']));

    const ltr = testing.newSafeHtmlForTest('', Dir.LTR);
    const rtl = testing.newSafeHtmlForTest('', Dir.RTL);
    const neutral = testing.newSafeHtmlForTest('', Dir.NEUTRAL);
    const unknown = testing.newSafeHtmlForTest('');
    assertEquals(Dir.NEUTRAL, SafeHtml.concat().getDirection());
    assertEquals(Dir.LTR, SafeHtml.concat(ltr, ltr).getDirection());
    assertEquals(Dir.LTR, SafeHtml.concat(ltr, neutral, ltr).getDirection());
    assertNull(SafeHtml.concat(ltr, unknown).getDirection());
    assertNull(SafeHtml.concat(ltr, rtl).getDirection());
    assertNull(SafeHtml.concat(ltr, [rtl]).getDirection());
  },

  testHtmlEscapePreservingNewlines() {
    // goog.html.SafeHtml passes through unchanged.
    const safeHtmlIn = SafeHtml.htmlEscapePreservingNewlines('<b>in</b>');
    assertTrue(
        safeHtmlIn === SafeHtml.htmlEscapePreservingNewlines(safeHtmlIn));

    assertSameHtml('a<br>c', SafeHtml.htmlEscapePreservingNewlines('a\nc'));
    assertSameHtml('&lt;<br>', SafeHtml.htmlEscapePreservingNewlines('<\n'));
    assertSameHtml('<br>', SafeHtml.htmlEscapePreservingNewlines('\r\n'));
    assertSameHtml('<br>', SafeHtml.htmlEscapePreservingNewlines('\r'));
    assertSameHtml('', SafeHtml.htmlEscapePreservingNewlines(''));
  },

  testHtmlEscapePreservingNewlinesAndSpaces() {
    // goog.html.SafeHtml passes through unchanged.
    const safeHtmlIn =
        SafeHtml.htmlEscapePreservingNewlinesAndSpaces('<b>in</b>');
    assertTrue(
        safeHtmlIn ===
        SafeHtml.htmlEscapePreservingNewlinesAndSpaces(safeHtmlIn));

    assertSameHtml(
        'a<br>c', SafeHtml.htmlEscapePreservingNewlinesAndSpaces('a\nc'));
    assertSameHtml(
        '&lt;<br>', SafeHtml.htmlEscapePreservingNewlinesAndSpaces('<\n'));
    assertSameHtml(
        '<br>', SafeHtml.htmlEscapePreservingNewlinesAndSpaces('\r\n'));
    assertSameHtml(
        '<br>', SafeHtml.htmlEscapePreservingNewlinesAndSpaces('\r'));
    assertSameHtml('', SafeHtml.htmlEscapePreservingNewlinesAndSpaces(''));

    assertSameHtml(
        'a &#160;b', SafeHtml.htmlEscapePreservingNewlinesAndSpaces('a  b'));
  },

  testComment() {
    assertSameHtml('<!--&lt;script&gt;-->', SafeHtml.comment('<script>'));
  },

  testSafeHtmlConcatWithDir() {
    const ltr = Dir.LTR;
    const rtl = Dir.RTL;
    const br = testing.newSafeHtmlForTest('<br>');

    assertEquals(ltr, SafeHtml.concatWithDir(ltr).getDirection());
    assertEquals(
        ltr,
        SafeHtml.concatWithDir(ltr, testing.newSafeHtmlForTest('', rtl))
            .getDirection());

    assertSameHtml('a<br>c', SafeHtml.concatWithDir(ltr, 'a', br, 'c'));
  },
});
