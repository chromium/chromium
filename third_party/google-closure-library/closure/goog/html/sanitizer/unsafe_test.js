/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit Test for the unsafe API of the HTML Sanitizer. */

goog.module('goog.html.UnsafeTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const HtmlSanitizer = goog.require('goog.html.sanitizer.HtmlSanitizer');
const HtmlSanitizerAttributePolicy = goog.requireType('goog.html.sanitizer.HtmlSanitizerAttributePolicy');
const SafeHtml = goog.require('goog.html.SafeHtml');
const TagWhitelist = goog.require('goog.html.sanitizer.TagWhitelist');
const dom = goog.require('goog.testing.dom');
const functions = goog.require('goog.functions');
const testSuite = goog.require('goog.testing.testSuite');
const unsafe = goog.require('goog.html.sanitizer.unsafe');
const userAgent = goog.require('goog.userAgent');
const {AllowedAttributes} = goog.require('goog.html.sanitizer.attributeallowlists');

const isSupported = !userAgent.IE || userAgent.isVersionOrHigher(10);

const just = Const.from('test');

/**
 * Sanitizes the original HTML and asserts that it is the same as the expected
 * HTML. Supports adding tags and attributes through the unsafe API.
 * @param {string} originalHtml
 * @param {string} expectedHtml
 * @param {?Array<string>=} tags
 * @param {?Array<(string|!HtmlSanitizerAttributePolicy)>=}
 *     attrs
 * @param {?HtmlSanitizer.Builder=} opt_builder
 */
function assertSanitizedHtml(
    originalHtml, expectedHtml, tags = undefined, attrs = undefined,
    opt_builder) {
  let builder = opt_builder || new HtmlSanitizer.Builder();
  if (tags) builder = unsafe.alsoAllowTags(just, builder, tags);
  if (attrs) builder = unsafe.alsoAllowAttributes(just, builder, attrs);
  const sanitizer = builder.build();
  const sanitized = sanitizer.sanitize(originalHtml);
  if (!isSupported) {
    assertEquals('', SafeHtml.unwrap(sanitized));
    return;
  }
  dom.assertHtmlMatches(
      expectedHtml, SafeHtml.unwrap(sanitized),
      true /* opt_strictAttributes */);
}

testSuite({
  testAllowEmptyTagList() {
    const input = '<sdf><aaa></aaa></sdf><b></b>';
    const expected = '<span><span></span></span><b></b>';
    assertSanitizedHtml(input, expected, []);
  },

  testAllowBlacklistedTag() {
    const input = '<div><script>aaa</script></div>';
    assertSanitizedHtml(input, input, ['SCriPT']);
  },

  testAllowUnknownTags() {
    const input = '<hello><bye>aaa</bye></hello><zzz></zzz>';
    const expected = '<hello><span>aaa</span></hello><zzz></zzz>';
    assertSanitizedHtml(input, expected, ['HElLO', 'zZZ']);
  },

  testAllowAlreadyWhiteListedTag() {
    const input = '<hello><p><zzz></zzz></p></hello>';
    const expected = '<span><p><zzz></zzz></p></span>';
    assertSanitizedHtml(input, expected, ['p', 'ZZZ']);
  },

  testAllowEmptyAttrList() {
    const input = '<a href="#" qwe="nope">b</a>';
    const expected = '<a href="#">b</a>';
    assertSanitizedHtml(input, expected, null, []);
  },

  testAllowUnknownAttributeSimple() {
    const input = '<qqq zzz="3" nnn="no"></qqq>';
    const expected = '<span zzz="3"></span>';
    assertSanitizedHtml(input, expected, null, ['Zzz']);
  },

  testAllowUnknownAttributeWildCard() {
    const input = '<div ab="yes" bb="no"><img ab="yep" bb="no" /></div>';
    const expected = '<div ab="yes"><img ab="yep" /></div>';
    assertSanitizedHtml(
        input, expected, null, [{tagName: '*', attributeName: 'aB'}]);
  },

  testAllowUnknownAttributeOnSpecificTag() {
    const input = '<a www="3" zzz="4">fff</a><img www="3" />';
    const expected = '<a www="3">fff</a><img />';
    assertSanitizedHtml(
        input, expected, null, [{tagName: 'a', attributeName: 'WwW'}]);
  },

  testAllowUnknownAttributePolicy() {
    const input = '<img ab="yes" /><img ab="no" />';
    const expected = '<img ab="yes" /><img />';
    assertSanitizedHtml(input, expected, null, [{
                          tagName: '*',
                          attributeName: 'aB',
                          policy: function(value, hints) {
                            assertEquals(hints.attributeName, 'ab');
                            return value === 'yes' ? value : null;
                          },
                        }]);
  },

  testAllowOverwriteAttrPolicy() {
    const input = '<a href="yes"></a><a href="no"></a>';
    const expected = '<a href="yes"></a><a></a>';
    assertSanitizedHtml(input, expected, null, [{
                          tagName: 'a',
                          attributeName: 'href',
                          policy: function(value) {
                            return value === 'yes' ? value : null;
                          },
                        }]);
  },

  testAllowDAttribute() {
    const input = '<path d="1.5 1.5 1.5 14.5 14.5 14.5 14.5 1.5"/>';
    const expected = '<path d="1.5 1.5 1.5 14.5 14.5 14.5 14.5 1.5"/>';
    assertSanitizedHtml(
        input, expected, ['path'], [{tagName: 'path', attributeName: 'd'}]);
  },

  testWhitelistAliasing() {
    const builder = new HtmlSanitizer.Builder();
    unsafe.alsoAllowTags(just, builder, ['QqQ']);
    unsafe.alsoAllowAttributes(just, builder, ['QqQ']);
    builder.build();
    assertUndefined(TagWhitelist['QQQ']);
    assertUndefined(TagWhitelist['QqQ']);
    assertUndefined(TagWhitelist['qqq']);
    assertUndefined(AllowedAttributes['* QQQ']);
    assertUndefined(AllowedAttributes['* QqQ']);
    assertUndefined(AllowedAttributes['* qqq']);
  },

  testAllowRelaxExistingAttributePolicyWildcard() {
    const input = '<a href="javascript:alert(1)"></a>';
    // define a tag-specific one, takes precedence
    assertSanitizedHtml(
        input, input, null,
        [{tagName: 'a', attributeName: 'href', policy: functions.identity}]);
    // overwrite the global one
    assertSanitizedHtml(
        input, input, null,
        [{tagName: '*', attributeName: 'href', policy: functions.identity}]);
  },

  testAllowRelaxExistingAttributePolicySpecific() {
    const input = '<a target="foo"></a>';
    const expected = '<a></a>';
    // overwrite the global one, the specific one still has precedence
    assertSanitizedHtml(input, expected, null, [
      {tagName: '*', attributeName: 'target', policy: functions.identity},
    ]);
    // overwrite the tag-specific one, this one should take precedence
    assertSanitizedHtml(input, input, null, [
      {tagName: 'a', attributeName: 'target', policy: functions.identity},
    ]);
  },

  testAlsoAllowTagsInBlacklist() {
    const input = '<video controls><source src="video.mp4" type="video/mp4">';
    assertSanitizedHtml(input, input, ['video', 'source'], [
      {tagName: 'video', attributeName: 'controls', policy: functions.identity},
      {tagName: 'source', attributeName: 'src', policy: functions.identity},
      {tagName: 'source', attributeName: 'type', policy: functions.identity},
    ]);
  },
});
