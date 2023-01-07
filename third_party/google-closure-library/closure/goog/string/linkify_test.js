/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.string.linkifyTest');
goog.setTestOnly();

const SafeHtml = goog.require('goog.html.SafeHtml');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const googString = goog.require('goog.string');
const linkify = goog.require('goog.string.linkify');
const safe = goog.require('goog.dom.safe');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');

/** @type {!HTMLDivElement} */
const div = dom.createElement(TagName.DIV);

function assertLinkify(comment, input, expected, preserveNewlines = undefined) {
  assertEquals(
      comment, expected,
      SafeHtml.unwrap(linkify.linkifyPlainTextAsHtml(
          input, {rel: '', target: ''}, preserveNewlines)));
}

testSuite({
  testContainsNoLink() {
    assertLinkify(
        'Text does not contain any links', 'Text with no links in it.',
        'Text with no links in it.');
  },

  testContainsALink() {
    assertLinkify(
        'Text only contains a link', 'http://www.google.com/',
        '<a href="http://www.google.com/">http://www.google.com/<\/a>');
  },

  testStartsWithALink() {
    assertLinkify(
        'Text starts with a link',
        'http://www.google.com/ is a well known search engine',
        '<a href="http://www.google.com/">http://www.google.com/<\/a>' +
            ' is a well known search engine');
  },

  testEndsWithALink() {
    assertLinkify(
        'Text ends with a link',
        'Look at this search engine: http://www.google.com/',
        'Look at this search engine: ' +
            '<a href="http://www.google.com/">http://www.google.com/<\/a>');
  },

  testContainsOnlyEmail() {
    assertLinkify(
        'Text only contains an email address', 'bolinfest@google.com',
        '<a href="mailto:bolinfest@google.com">bolinfest@google.com<\/a>');
  },

  testStartsWithAnEmail() {
    assertLinkify(
        'Text starts with an email address',
        'bolinfest@google.com wrote this test.',
        '<a href="mailto:bolinfest@google.com">bolinfest@google.com<\/a>' +
            ' wrote this test.');
  },

  testEndsWithAnEmail() {
    assertLinkify(
        'Text ends with an email address',
        'This test was written by bolinfest@google.com.',
        'This test was written by ' +
            '<a href="mailto:bolinfest@google.com">bolinfest@google.com<\/a>.');
  },

  testSingleQuotedUrl() {
    assertLinkify(
        'URLs surrounded by \'...\' exclude quotes from link',
        'Foo \'http://google.com\' bar',
        'Foo &#39;<a href="http://google.com">http://google.com</a>&#39; bar');
  },

  testSingleQuoteInUrl() {
    assertLinkify(
        'URLs containing \' include quote in link', 'http://google.com/yo\'yo',
        '<a href="http://google.com/yo&#39;yo">http://google.com/yo&#39;yo</a>');
  },

  testDoubleQuotedUrl() {
    assertLinkify(
        'URLs surrounded by "..." exclude quotes from link',
        'Foo "http://google.com" bar',
        'Foo &quot;<a href="http://google.com">http://google.com</a>&quot; bar');
  },

  testUrlWithPortNumber() {
    assertLinkify(
        'URL with a port number', 'http://www.google.com:80/',
        '<a href="http://www.google.com:80/">http://www.google.com:80/<\/a>');
  },

  testUrlWithUserPasswordAndPortNumber() {
    assertLinkify(
        'URL with a user, a password and a port number',
        'http://lascap:p4ssw0rd@google.com:80/s?q=a&hl=en',
        '<a href="http://lascap:p4ssw0rd@google.com:80/s?q=a&amp;hl=en">' +
            'http://lascap:p4ssw0rd@google.com:80/s?q=a&amp;hl=en<\/a>');
  },

  testUrlWithUnderscore() {
    assertLinkify(
        'URL with an underscore', 'http://www_foo.google.com/',
        '<a href="http://www_foo.google.com/">http://www_foo.google.com/<\/a>');
  },

  testInternalUrlWithoutDomain() {
    assertLinkify(
        'Internal URL without a proper domain', 'http://tracker/1068594',
        '<a href="http://tracker/1068594">http://tracker/1068594<\/a>');
  },

  testInternalUrlOneChar() {
    assertLinkify(
        'Internal URL with a one char domain', 'http://b',
        '<a href="http://b">http://b<\/a>');
  },

  testSecureInternalUrlWithoutDomain() {
    assertLinkify(
        'Secure Internal URL without a proper domain', 'https://review/6594805',
        '<a href="https://review/6594805">https://review/6594805<\/a>');
  },

  testTwoUrls() {
    assertLinkify(
        'Text with two URLs in it',
        'I use both http://www.google.com and http://yahoo.com, don\'t you?',
        'I use both <a href="http://www.google.com">http://www.google.com<\/a> ' +
            'and <a href="http://yahoo.com">http://yahoo.com<\/a>, ' +
            googString.htmlEscape('don\'t you?'));
  },

  testGetParams() {
    assertLinkify(
        'URL with GET params', 'http://google.com/?a=b&c=d&e=f',
        '<a href="http://google.com/?a=b&amp;c=d&amp;e=f">' +
            'http://google.com/?a=b&amp;c=d&amp;e=f<\/a>');
  },

  testGoogleCache() {
    assertLinkify(
        'Google search result from cache',
        'http://66.102.7.104/search?q=cache:I4LoMT6euUUJ:' +
            'www.google.com/intl/en/help/features.html+google+cache&hl=en',
        '<a href="http://66.102.7.104/search?q=cache:I4LoMT6euUUJ:' +
            'www.google.com/intl/en/help/features.html+google+cache&amp;hl=en">' +
            'http://66.102.7.104/search?q=cache:I4LoMT6euUUJ:' +
            'www.google.com/intl/en/help/features.html+google+cache&amp;hl=en' +
            '<\/a>');
  },

  testUrlWithoutHttp() {
    assertLinkify(
        'URL without http protocol',
        'It\'s faster to type www.google.com without the http:// in front.',
        googString.htmlEscape('It\'s faster to type ') +
            '<a href="http://www.google.com">www.google.com' +
            '<\/a> without the http:// in front.');
  },

  testUrlWithCapitalsWithoutHttp() {
    assertLinkify(
        'URL with capital letters without http protocol',
        'It\'s faster to type Www.google.com without the http:// in front.',
        googString.htmlEscape('It\'s faster to type ') +
            '<a href="http://Www.google.com">Www.google.com' +
            '<\/a> without the http:// in front.');
  },

  testUrlHashBang() {
    assertLinkify(
        'URL with #!',
        'Another test URL: ' +
            'https://www.google.com/testurls/#!/page',
        'Another test URL: ' +
            '<a href="https://www.google.com/testurls/#!/page">' +
            'https://www.google.com/testurls/#!/page<\/a>');
  },

  testTextLooksLikeUrlWithoutHttp() {
    assertLinkify(
        'Text looks like an url but is not',
        'This showww.is just great: www.is',
        'This showww.is just great: <a href="http://www.is">www.is<\/a>');
  },

  testEmailWithSubdomain() {
    assertLinkify(
        'Email with a subdomain', 'Send mail to bolinfest@groups.google.com.',
        'Send mail to <a href="mailto:bolinfest@groups.google.com">' +
            'bolinfest@groups.google.com<\/a>.');
  },

  testEmailWithHyphen() {
    assertLinkify(
        'Email with a hyphen in the domain name',
        'Send mail to bolinfest@google-groups.com.',
        'Send mail to <a href="mailto:bolinfest@google-groups.com">' +
            'bolinfest@google-groups.com<\/a>.');
  },

  testEmailUsernameWithSpecialChars() {
    assertLinkify(
        'Email with a hyphen, period, and + in the user name',
        'Send mail to bolin-fest+for.um@google.com',
        'Send mail to <a href="mailto:bolin-fest+for.um@google.com">' +
            'bolin-fest+for.um@google.com<\/a>');
    assertLinkify(
        'Email with all special characters in the user name',
        'Send mail to muad\'dib!#$%&*/=?^_`{|}~@google.com',
        'Send mail to ' +
            '<a href="mailto:muad&#39;dib!#$%&amp;*/=?^_`{|}~@google.com">' +
            'muad&#39;dib!#$%&amp;*/=?^_`{|}~@google.com<\/a>');
  },

  testEmailWithUnderscoreInvalid() {
    assertLinkify(
        'Email with an underscore in the domain name, which is invalid',
        'Do not email bolinfest@google_groups.com.',
        'Do not email bolinfest@google_groups.com.');
  },

  testUrlNotHttp() {
    assertLinkify(
        'Url using unusual scheme',
        'Looking for some goodies: ftp://ftp.google.com/goodstuff/',
        'Looking for some goodies: ' +
            '<a href="ftp://ftp.google.com/goodstuff/">' +
            'ftp://ftp.google.com/goodstuff/<\/a>');
  },

  testJsInjection() {
    assertLinkify(
        'Text includes some javascript',
        'Welcome in hell <script>alert(\'this is hell\')<\/script>',
        googString.htmlEscape(
            'Welcome in hell <script>alert(\'this is hell\')<\/script>'));
  },

  testJsInjectionDotIsBlind() {
    assertLinkify(
        'JavaScript injection using regex . blindness to newline chars',
        '<script>malicious_code()<\/script>\nVery nice url: www.google.com',
        '&lt;script&gt;malicious_code()&lt;/script&gt;\nVery nice url: ' +
            '<a href="http://www.google.com">www.google.com<\/a>');
  },

  testJsInjectionWithUnicodeLineReturn() {
    assertLinkify(
        'JavaScript injection using regex . blindness to newline chars with a ' +
            'unicode newline character.',
        '<script>malicious_code()<\/script>\u2029Vanilla text',
        '&lt;script&gt;malicious_code()&lt;/script&gt;\u2029Vanilla text');
  },

  testJsInjectionWithIgnorableNonTagChar() {
    assertLinkify(
        'Angle brackets are normalized even when followed by an ignorable ' +
            'non-tag character.',
        '<\u0000img onerror=alert(1337) src=\n>',
        '&lt;&#0;img onerror=alert(1337) src=\n&gt;');
  },

  testJsInjectionWithTextarea() {
    assertLinkify(
        'Putting the result in a textarea can\'t cause other textarea text to ' +
            'be treated as tag content.',
        '</textarea', '&lt;/textarea');
  },

  testJsInjectionWithNewlineConversion() {
    assertLinkify(
        'Any newline conversion and whitespace normalization won\'t cause tag ' +
            'parts to be recombined.',
        '<<br>script<br>>alert(1337)<<br>/<br>script<br>>',
        '&lt;&lt;br&gt;script&lt;br&gt;&gt;alert(1337)&lt;&lt;br&gt;/&lt;' +
            'br&gt;script&lt;br&gt;&gt;');
  },

  testNoProtocolBlacklisting() {
    assertLinkify(
        'No protocol blacklisting.',
        'Click: jscript:alert%281337%29\nClick: JSscript:alert%281337%29\n' +
            'Click: VBscript:alert%281337%29\nClick: Script:alert%281337%29\n' +
            'Click: flavascript:alert%281337%29',
        'Click: jscript:alert%281337%29\nClick: JSscript:alert%281337%29\n' +
            'Click: VBscript:alert%281337%29\nClick: Script:alert%281337%29\n' +
            'Click: flavascript:alert%281337%29');
  },

  testProtocolWhitelistingEffective() {
    assertLinkify(
        'Protocol whitelisting is effective.',
        'Click httpscript:alert%281337%29\nClick mailtoscript:alert%281337%29\n' +
            'Click j\u00A0avascript:alert%281337%29\n' +
            'Click \u00A0javascript:alert%281337%29',
        'Click httpscript:alert%281337%29\nClick mailtoscript:alert%281337%29\n' +
            'Click j\u00A0avascript:alert%281337%29\n' +
            'Click \u00A0javascript:alert%281337%29');
  },

  testLinkifyNoOptions() {
    safe.setInnerHtml(
        div, linkify.linkifyPlainTextAsHtml('http://www.google.com'));
    testingDom.assertHtmlContentsMatch(
        '<a href="http://www.google.com" target="_blank" rel="nofollow">' +
            'http://www.google.com<\/a>',
        div, true /* opt_strictAttributes */);
  },

  testLinkifyOptionsNoAttributes() {
    safe.setInnerHtml(
        div,
        linkify.linkifyPlainTextAsHtml(
            'The link for www.google.com is located somewhere in ' +
                'https://www.google.fr/?hl=en, you should find it easily.',
            {rel: '', target: ''}));
    testingDom.assertHtmlContentsMatch(
        'The link for <a href="http://www.google.com">www.google.com<\/a> is ' +
            'located somewhere in ' +
            '<a href="https://www.google.fr/?hl=en">https://www.google.fr/?hl=en' +
            '<\/a>, you should find it easily.',
        div, true /* opt_strictAttributes */);
  },

  testLinkifyOptionsClassName() {
    safe.setInnerHtml(
        div,
        linkify.linkifyPlainTextAsHtml(
            'Attribute with <class> name www.w3c.org.',
            {'class': 'link-added'}));
    testingDom.assertHtmlContentsMatch(
        'Attribute with &lt;class&gt; name <a href="http://www.w3c.org" ' +
            'target="_blank" rel="nofollow" class="link-added">www.w3c.org<\/a>.',
        div, true /* opt_strictAttributes */);
  },

  testFindFirstUrlNoScheme() {
    assertEquals('www.google.com', linkify.findFirstUrl('www.google.com'));
  },

  testFindFirstUrlNoSchemeUppercase() {
    assertEquals('WWW.GOOGLE.COM', linkify.findFirstUrl('WWW.GOOGLE.COM'));
  },

  testFindFirstUrlNoSchemeMixedcase() {
    assertEquals('WwW.GoOgLe.CoM', linkify.findFirstUrl('WwW.GoOgLe.CoM'));
  },

  testFindFirstUrlNoSchemeWithText() {
    assertEquals(
        'www.google.com',
        linkify.findFirstUrl('prefix www.google.com something'));
  },

  testFindFirstUrlScheme() {
    assertEquals(
        'http://www.google.com', linkify.findFirstUrl('http://www.google.com'));
  },

  testFindFirstUrlSchemeUppercase() {
    assertEquals(
        'HTTP://WWW.GOOGLE.COM', linkify.findFirstUrl('HTTP://WWW.GOOGLE.COM'));
  },

  testFindFirstUrlSchemeMixedcase() {
    assertEquals(
        'HtTp://WwW.gOoGlE.cOm', linkify.findFirstUrl('HtTp://WwW.gOoGlE.cOm'));
  },

  testFindFirstUrlSchemeWithText() {
    assertEquals(
        'http://www.google.com',
        linkify.findFirstUrl('prefix http://www.google.com something'));
  },

  testFindFirstUrlNoUrl() {
    assertEquals(
        '', linkify.findFirstUrl('ygvtfr676 5v68fk uygbt85F^&%^&I%FVvc .'));
  },

  testFindFirstEmailNoScheme() {
    assertEquals('fake@google.com', linkify.findFirstEmail('fake@google.com'));
  },

  testFindFirstEmailNoSchemeUppercase() {
    assertEquals('FAKE@GOOGLE.COM', linkify.findFirstEmail('FAKE@GOOGLE.COM'));
  },

  testFindFirstEmailNoSchemeMixedcase() {
    assertEquals('fAkE@gOoGlE.cOm', linkify.findFirstEmail('fAkE@gOoGlE.cOm'));
  },

  testFindFirstEmailNoSchemeWithText() {
    assertEquals(
        'fake@google.com',
        linkify.findFirstEmail('prefix fake@google.com something'));
  },

  testFindFirstEmailScheme() {
    assertEquals(
        'mailto:fake@google.com',
        linkify.findFirstEmail('mailto:fake@google.com'));
  },

  testFindFirstEmailSchemeUppercase() {
    assertEquals(
        'MAILTO:FAKE@GOOGLE.COM',
        linkify.findFirstEmail('MAILTO:FAKE@GOOGLE.COM'));
  },

  testFindFirstEmailSchemeMixedcase() {
    assertEquals(
        'MaIlTo:FaKe@GoOgLe.CoM',
        linkify.findFirstEmail('MaIlTo:FaKe@GoOgLe.CoM'));
  },

  testFindFirstEmailSchemeWithText() {
    assertEquals(
        'mailto:fake@google.com',
        linkify.findFirstEmail('prefix mailto:fake@google.com something'));
  },

  testFindFirstEmailNoEmail() {
    assertEquals(
        '', linkify.findFirstEmail('ygvtfr676 5v68fk uygbt85F^&%^&I%FVvc .'));
  },

  testContainsPunctuation_parens() {
    assertLinkify(
        'Link contains parens, but does not end with them',
        'www.google.com/abc(v1).html',
        '<a href="http://www.google.com/abc(v1).html">' +
            'www.google.com/abc(v1).html<\/a>');
  },

  testEndsWithPunctuation() {
    assertLinkify(
        'Link ends with punctuation',
        'Have you seen www.google.com? It\'s awesome.',
        'Have you seen <a href="http://www.google.com">www.google.com<\/a>?' +
            googString.htmlEscape(' It\'s awesome.'));
  },

  testEndsWithPunctuation_closeParen() {
    assertLinkify(
        'Link inside parentheses', '(For more info see www.googl.com)',
        '(For more info see <a href="http://www.googl.com">www.googl.com<\/a>)');
    assertLinkify(
        'Parentheses inside link',
        'http://en.wikipedia.org/wiki/Titanic_(1997_film)',
        '<a href="http://en.wikipedia.org/wiki/Titanic_(1997_film)">' +
            'http://en.wikipedia.org/wiki/Titanic_(1997_film)<\/a>');
  },

  testEndsWithPunctuation_openParen() {
    assertLinkify(
        'Link followed by open parenthesis', 'www.google.com(',
        '<a href="http://www.google.com(">www.google.com(<\/a>');
  },

  testEndsWithPunctuation_angles() {
    assertLinkify(
        'Link inside angled brackets',
        'Here is a bibliography entry <http://www.google.com/>',
        'Here is a bibliography entry &lt;<a href="http://www.google.com/">' +
            'http://www.google.com/<\/a>&gt;');
  },

  testEndsWithPunctuation_curlies() {
    assertLinkify(
        'Link inside curly brackets', '{http://www.google.com/}',
        '{<a href="http://www.google.com/">' +
            'http://www.google.com/<\/a>}');
    assertLinkify(
        'Curly brackets inside link', 'http://www.google.com/abc{arg=1}',
        '<a href="http://www.google.com/abc{arg=1}">' +
            'http://www.google.com/abc{arg=1}<\/a>');
  },

  testEndsWithPunctuation_closingPairThenSingle() {
    assertLinkify(
        'Link followed by closing punctuation pair then singular punctuation',
        'Here is a bibliography entry <http://www.google.com/>, PTAL.',
        'Here is a bibliography entry &lt;<a href="http://www.google.com/">' +
            'http://www.google.com/<\/a>&gt;, PTAL.');
  },

  testEndsWithPunctuation_ellipses() {
    assertLinkify(
        'Link followed by three dots', 'just look it up on www.google.com...',
        'just look it up on <a href="http://www.google.com">www.google.com' +
            '<\/a>...');
  },

  testBracketsInUrl() {
    assertLinkify(
        'Link containing brackets',
        'before http://google.com/details?answer[0]=42 after',
        'before <a href="http://google.com/details?answer[0]=42">' +
            'http://google.com/details?answer[0]=42<\/a> after');
  },

  testUrlWithExclamation() {
    assertLinkify(
        'URL with exclamation points', 'This is awesome www.google.com!',
        'This is awesome <a href="http://www.google.com">www.google.com<\/a>!');
  },

  testSpecialCharactersInUrl() {
    assertLinkify(
        'Link with characters that are neither reserved nor unreserved as per' +
            'RFC 3986 but that are recognized by other Google properties.',
        'https://www.google.com/?q=\`{|}recognized',
        '<a href="https://www.google.com/?q=\`{|}recognized">' +
            'https://www.google.com/?q=\`{|}recognized<\/a>');
  },

  testUsuallyUnrecognizedCharactersAreNotInUrl() {
    assertLinkify(
        'Link with characters that are neither reserved nor unreserved as per' +
            'RFC 3986 and which are not recognized by other Google properties.',
        'https://www.google.com/?q=<^>"',
        '<a href="https://www.google.com/?q=">' +
            'https://www.google.com/?q=<\/a>&lt;^&gt;&quot;');
  },

  testIpv6Url() {
    assertLinkify(
        'IPv6 URL', 'http://[::FFFF:129.144.52.38]:80/index.html',
        '<a href="http://[::FFFF:129.144.52.38]:80/index.html">' +
            'http://[::FFFF:129.144.52.38]:80/index.html<\/a>');
  },

  testPreserveNewlines() {
    assertLinkify(
        'Preserving newlines', 'Example:\nhttp://www.google.com/',
        'Example:<br>' +
            '<a href="http://www.google.com/">http://www.google.com/<\/a>',
        /* preserveNewlines */ true);
    assertLinkify(
        'Preserving newlines with no links', 'Line 1\nLine 2',
        'Line 1<br>Line 2',
        /* preserveNewlines */ true);
  },
});
