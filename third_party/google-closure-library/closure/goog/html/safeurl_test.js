/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for SafeUrl and its builders. */

goog.module('goog.html.safeUrlTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const Dir = goog.require('goog.i18n.bidi.Dir');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeUrl = goog.require('goog.html.SafeUrl');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const fsUrl = goog.require('goog.fs.url');
const googObject = goog.require('goog.object');
const safeUrlTestVectors = goog.require('goog.html.safeUrlTestVectors');
const testSuite = goog.require('goog.testing.testSuite');
const {assertExists} = goog.require('goog.asserts');


/**
 * Tests creating a SafeUrl from a blob with the given MIME type, asserting
 * whether or not the SafeUrl returned is innocuous or not depending on the
 * given boolean.
 * @param {string} type MIME type to test
 * @param {boolean} isSafe Whether the given MIME type should be considered safe
 *     by {@link SafeUrl.fromBlob}.
 */
function assertBlobTypeIsSafe(type, isSafe) {
  const safeUrl = SafeUrl.fromBlob(new Blob(['test'], {type: type}));
  const extracted = SafeUrl.unwrap(safeUrl);
  if (isSafe) {
    assertEquals('blob:', extracted.substring(0, 5));
  } else {
    assertEquals(SafeUrl.INNOCUOUS_STRING, extracted);
  }
}

/**
 * @type {!PropertyReplacer}
 */
let stubs;

testSuite({

  setUp() {
    stubs = new PropertyReplacer();
  },

  tearDown() {
    stubs.reset();
  },

  testSafeUrl() {
    const safeUrl = SafeUrl.fromConstant(Const.from('javascript:trusted();'));
    const extracted = SafeUrl.unwrap(safeUrl);
    assertEquals('javascript:trusted();', extracted);
    assertEquals('javascript:trusted();', SafeUrl.unwrap(safeUrl));
    assertEquals('javascript:trusted();', String(safeUrl));

    // URLs are always LTR.
    assertEquals(Dir.LTR, safeUrl.getDirection());

    // Interface markers are present.
    assertTrue(safeUrl.implementsGoogStringTypedString);
    assertTrue(safeUrl.implementsGoogI18nBidiDirectionalString);
  },

  testSafeUrlIsSafeMimeType_withSafeType() {
    assertTrue(SafeUrl.isSafeMimeType('audio/ogg'));
    assertTrue(SafeUrl.isSafeMimeType('audio/x-matroska'));
    assertTrue(SafeUrl.isSafeMimeType('font/woff'));
    assertTrue(SafeUrl.isSafeMimeType('image/png'));
    assertTrue(SafeUrl.isSafeMimeType('iMage/pNg'));
    assertTrue(SafeUrl.isSafeMimeType('video/mpeg'));
    assertTrue(SafeUrl.isSafeMimeType('video/ogg'));
    assertTrue(SafeUrl.isSafeMimeType('video/mp4'));
    assertTrue(SafeUrl.isSafeMimeType('video/ogg'));
    assertTrue(SafeUrl.isSafeMimeType('video/webm'));
    assertTrue(SafeUrl.isSafeMimeType('video/quicktime'));
    assertTrue(SafeUrl.isSafeMimeType('video/x-matroska'));
    // Allow comma-separated, quoted MIME parameters with and without spaces.
    assertTrue(SafeUrl.isSafeMimeType('video/webm;codecs="vp8,opus"'));
    assertTrue(SafeUrl.isSafeMimeType('video/webm;codecs="vp8, opus"'));
  },

  testSafeUrlIsSafeMimeType_withUnsafeType() {
    assertFalse(SafeUrl.isSafeMimeType(''));
    assertFalse(SafeUrl.isSafeMimeType('ximage/png'));
    assertFalse(SafeUrl.isSafeMimeType('image/pngx'));
    assertFalse(SafeUrl.isSafeMimeType('video/whatever'));
    assertFalse(SafeUrl.isSafeMimeType('video/'));
    // Complex MIME parameters must be quoted.
    assertFalse(SafeUrl.isSafeMimeType('video/webm;codecs=vp8,opus'));
  },

  testSafeUrlFromBlob_withSafeType() {
    assertBlobTypeIsSafe('audio/ogg', true);
    assertBlobTypeIsSafe('image/png', true);
    assertBlobTypeIsSafe('iMage/pNg', true);
    assertBlobTypeIsSafe('video/mpeg', true);
    assertBlobTypeIsSafe('video/ogg', true);
    assertBlobTypeIsSafe('video/mp4', true);
    assertBlobTypeIsSafe('video/ogg', true);
    assertBlobTypeIsSafe('video/webm', true);
    assertBlobTypeIsSafe('video/quicktime', true);

    assertBlobTypeIsSafe('image/png;foo=bar', true);
    assertBlobTypeIsSafe('image/png;foo="bar"', true);
    assertBlobTypeIsSafe('image/png;foo="bar;baz"', true);
    assertBlobTypeIsSafe('image/png;foo="bar";baz=bar', true);
  },

  testSafeUrlFromBlob_withUnsafeType() {
    assertBlobTypeIsSafe('', false);
    assertBlobTypeIsSafe('ximage/png', false);
    assertBlobTypeIsSafe('image/pngx', false);
    assertBlobTypeIsSafe('video/whatever', false);
    assertBlobTypeIsSafe('video/', false);

    assertBlobTypeIsSafe('image/png;foo', false);
    assertBlobTypeIsSafe('image/png;foo=', false);
    assertBlobTypeIsSafe('image/png;foo=bar;', false);
    assertBlobTypeIsSafe('image/png;foo=bar;baz', false);

    // Maybe not wrong, but we reject nonetheless for simplicity.
    assertBlobTypeIsSafe('image/png;foo=bar&', false);
    assertBlobTypeIsSafe('image/png;foo=%3Cbar', false);
  },

  testSafeUrlFromBlob_revocation() {
    let timesCalled = 0;
    stubs.replace(fsUrl, 'revokeObjectUrl', function(arg) {
      timesCalled++;
    });
    SafeUrl.revokeObjectUrl(
        SafeUrl.fromBlob(new Blob(['test'], {type: 'image/png'})));
    assertEquals(1, timesCalled);
    SafeUrl.revokeObjectUrl(
        SafeUrl.fromBlob(new Blob(['test'], {type: 'text/html'})));
    // No revocation, as object URL was never created.
    assertEquals(1, timesCalled);
  },

  testSafeUrlFromMediaSource_createsBlob() {
    if (!('MediaSource' in globalThis)) {
      return;
    }
    const safeUrl = SafeUrl.fromMediaSource(new MediaSource());
    const extracted = SafeUrl.unwrap(safeUrl);
    assertEquals('blob:', extracted.substring(0, 5));
  },

  testSafeUrlFromMediaSource_rejectsBlobs() {
    if (!('MediaSource' in globalThis)) {
      return;
    }
    /** @suppress {checkTypes} suppression added to enable type checking */
    const safeUrl =
        SafeUrl.fromMediaSource(new Blob([''], {type: 'text/plain'}));
    const extracted = SafeUrl.unwrap(safeUrl);
    assertEquals(SafeUrl.INNOCUOUS_STRING, extracted);
  },

  testSafeUrlFromFacebookMessengerUrl_fbMessengerShareUrl() {
    const expected = 'fb-messenger://share?link=https%3A%2F%2Fwww.google.com';
    const observed = SafeUrl.fromFacebookMessengerUrl(
        'fb-messenger://share?link=https%3A%2F%2Fwww.google.com');
    assertEquals(expected, SafeUrl.unwrap(observed));
  },

  testSafeUrlFromFacebookMessengerUrl_fbMessengerEvilUrl() {
    const observed = SafeUrl.fromFacebookMessengerUrl(
        'fb-messenger://evil?link=https%3A%2F%2Fwww.google.com');
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testSafeUrlFromWhatsAppUrl_whatsAppSendUrl() {
    const expected = 'whatsapp://send?text=foo';
    const observed = SafeUrl.fromWhatsAppUrl('whatsapp://send?text=foo');
    assertEquals(expected, SafeUrl.unwrap(observed));
  },

  testSafeUrlFromWhatsAppUrl_whatsAppEvilUrl() {
    const observed = SafeUrl.fromWhatsAppUrl('whatsapp://evil?text=foo');
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testSafeUrlSanitize_sanitizeTelUrl() {
    const vectors = safeUrlTestVectors.TEL_VECTORS;
    for (let i = 0; i < vectors.length; ++i) {
      const v = vectors[i];
      const observed = SafeUrl.fromTelUrl(v.input);
      assertEquals(v.expected, SafeUrl.unwrap(observed));
    }
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testSafeUrlSanitize_sanitizeSshUrl() {
    const vectors = safeUrlTestVectors.SSH_VECTORS;
    for (let i = 0; i < vectors.length; ++i) {
      const v = vectors[i];
      const observed = SafeUrl.fromSshUrl(v.input);
      assertEquals('SSH Url: ' + v.input, v.expected, SafeUrl.unwrap(observed));
    }
  },

  testSafeUrlSanitize_sipUrlEmail() {
    const expected = 'sip:username@example.com';
    const observed = SafeUrl.fromSipUrl('sip:username@example.com');
    assertEquals(expected, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sipsUrlEmail() {
    const expected = 'sips:username@example.com';
    const observed = SafeUrl.fromSipUrl('sips:username@example.com');
    assertEquals(expected, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sipProtocolCase() {
    const expected = 'Sip:username@example.com';
    const observed = SafeUrl.fromSipUrl('Sip:username@example.com');
    assertEquals(expected, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sipUrlWithPort() {
    const observed = SafeUrl.fromSipUrl('sip:username@example.com:5000');
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sipUrlFragment() {
    const observed = SafeUrl.fromSipUrl('sip:user#name@example.com');
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sipUrlWithPassword() {
    const observed = SafeUrl.fromSipUrl('sips:username:password@example.com');
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sipUrlWithOptions() {
    const observed = SafeUrl.fromSipUrl('sips:user;na=me@example.com');
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sipUrlWithPercent() {
    const observed = SafeUrl.fromSipUrl('sip:user%40name@example.com');
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sipUrlWithAmbiguousQuery() {
    const observed = SafeUrl.fromSipUrl('sip:user?name@example.com');
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testSafeUrlSanitize_sanitizeSmsUrl() {
    const vectors = safeUrlTestVectors.SMS_VECTORS;
    for (let i = 0; i < vectors.length; ++i) {
      const v = vectors[i];
      const observed = SafeUrl.fromSmsUrl(v.input);
      assertEquals(v.expected, SafeUrl.unwrap(observed));
    }
  },

  testSafeUrlSanitize_sanitizeChromeExtension() {
    const extensionId = Const.from('1234567890abcdef');
    let observed = SafeUrl.sanitizeChromeExtensionUrl(
        'chrome-extension://1234567890abcdef/foo/bar', extensionId);
    assertEquals(
        'chrome-extension://1234567890abcdef/foo/bar',
        SafeUrl.unwrap(observed));

    observed = SafeUrl.sanitizeChromeExtensionUrl(
        'chrome-extension://1234567890abcdef/foo/bar', [extensionId]);
    assertEquals(
        'chrome-extension://1234567890abcdef/foo/bar',
        SafeUrl.unwrap(observed));

    observed = SafeUrl.sanitizeChromeExtensionUrl(
        'not-a-chrome-extension://1234567890abcdef/foo/bar', extensionId);
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));

    observed = SafeUrl.sanitizeChromeExtensionUrl(
        'chrome-extension://fedcba0987654321/foo/bar', extensionId);
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sanitizeFirefoxExtension() {
    const extensionId = Const.from('1234-5678-90ab-cdef');
    let observed = SafeUrl.sanitizeFirefoxExtensionUrl(
        'moz-extension://1234-5678-90ab-cdef/foo/bar', extensionId);
    assertEquals(
        'moz-extension://1234-5678-90ab-cdef/foo/bar',
        SafeUrl.unwrap(observed));

    observed = SafeUrl.sanitizeFirefoxExtensionUrl(
        'moz-extension://ms-browser-extension://1234-5678-90ab-cdef/foo/bar',
        extensionId);
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testSafeUrlSanitize_sanitizeEdgeExtension() {
    const extensionId = Const.from('1234-5678-90ab-cdef');
    let observed = SafeUrl.sanitizeEdgeExtensionUrl(
        'ms-browser-extension://1234-5678-90ab-cdef/foo/bar', extensionId);
    assertEquals(
        'ms-browser-extension://1234-5678-90ab-cdef/foo/bar',
        SafeUrl.unwrap(observed));

    observed = SafeUrl.sanitizeEdgeExtensionUrl(
        'chrome-extension://1234-5678-90ab-cdef/foo/bar', extensionId);
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(observed));
  },

  testFromTrustedResourceUrl() {
    const url = Const.from('test');
    const trustedResourceUrl = TrustedResourceUrl.fromConstant(url);
    const safeUrl = SafeUrl.fromTrustedResourceUrl(trustedResourceUrl);
    assertEquals(Const.unwrap(url), SafeUrl.unwrap(safeUrl));
  },

  /** @suppress {checkTypes} */
  testUnwrap() {
    const privateFieldName = 'privateDoNotAccessOrElseSafeUrlWrappedValue_';
    const propNames = googObject.getKeys(SafeUrl.sanitize(''));
    assertContains(privateFieldName, propNames);
    const evil = {};
    evil[privateFieldName] = 'javascript:evil()';

    const exception = assertThrows(() => {
      SafeUrl.unwrap(evil);
    });
    assertContains('expected object of type SafeUrl', exception.message);
  },

  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testSafeUrlSanitize_trySanitize() {
    for (const v of safeUrlTestVectors.BASE_VECTORS) {
      const isDataUrl = v.input.match(/^data:/i);
      const observed = isDataUrl ? SafeUrl.tryFromDataUrl(v.input) :
                                   SafeUrl.trySanitize(v.input);
      if (v.safe) {
        assertEquals(v.expected, SafeUrl.unwrap(assertExists(observed)));
      } else {
        assertNull(observed);
      }
    }
  },

  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testSafeUrlSanitize_sanitize() {
    for (const v of safeUrlTestVectors.BASE_VECTORS) {
      const observed = SafeUrl.sanitize(v.input);
      assertEquals(v.expected, SafeUrl.unwrap(observed));
    }
  },

  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testSafeUrlSanitize_sanitizeAssertUnchanged() {
    for (const v of safeUrlTestVectors.BASE_VECTORS) {
      const isDataUrl = v.input.match(/^data:/i);
      if (v.safe) {
        const asserted = SafeUrl.sanitizeAssertUnchanged(v.input, isDataUrl);
        assertEquals(v.expected, SafeUrl.unwrap(asserted));
      } else {
        assertThrows(() => {
          SafeUrl.sanitizeAssertUnchanged(v.input, isDataUrl);
        });
      }
    }
  },

  testSafeUrlSanitize_sanitizeProgramConstants() {
    // .sanitize() works on program constants.
    const good = Const.from('http://example.com/');
    const goodOutput = SafeUrl.sanitize(good);
    assertEquals('http://example.com/', SafeUrl.unwrap(goodOutput));
    const asserted = SafeUrl.sanitizeAssertUnchanged(good);
    assertEquals('http://example.com/', SafeUrl.unwrap(asserted));

    // .sanitize() does not exempt values known to be program constants.
    const bad = Const.from('data:blah');
    const badOutput = SafeUrl.sanitize(bad);
    assertEquals(SafeUrl.INNOCUOUS_STRING, SafeUrl.unwrap(badOutput));
    assertThrows(() => {
      SafeUrl.sanitizeAssertUnchanged(bad);
    });
  },

  testSafeUrlSanitize_idempotentForSafeUrlArgument() {
    // This matches the safe prefix.
    let safeUrl = SafeUrl.sanitize('https://www.google.com/');
    let safeUrl2 = SafeUrl.sanitize(safeUrl);
    assertEquals(SafeUrl.unwrap(safeUrl), SafeUrl.unwrap(safeUrl2));

    // This doesn't match the safe prefix, getting converted into an innocuous
    // string.
    safeUrl = SafeUrl.sanitize('disallowed:foo');
    safeUrl2 = SafeUrl.sanitize(safeUrl);
    assertEquals(SafeUrl.unwrap(safeUrl), SafeUrl.unwrap(safeUrl2));
  },

  testSafeUrlSanitize_base64ImageSrc() {
    const dataUrl = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAT4AAA';
    const safeUrl = SafeUrl.fromDataUrl(dataUrl);
    assertEquals(SafeUrl.unwrap(safeUrl), dataUrl);
  },

  testSafeUrlSanitize_base64ImageSrcWithCRLF() {
    const dataUrl =
        'data:image/png;base64,iVBORw0KGgoA%0AAAANSUhEUgA%0DAAT4AAA%0A';
    const safeUrl = SafeUrl.fromDataUrl(dataUrl);
    assertEquals(
        SafeUrl.unwrap(safeUrl),
        'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAT4AAA');
  },
});
