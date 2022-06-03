/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.format.EmailAddressTest');
goog.setTestOnly();

const EmailAddress = goog.require('goog.format.EmailAddress');
const Format = goog.require('goog.i18n.bidi.Format');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');
const LRM = Format.LRM;
const RLM = Format.RLM;
const LRE = Format.LRE;
const RLE = Format.RLE;
const PDF = Format.PDF;

function doIsValidTest(testFunc, valid, invalid) {
  googArray.forEach(valid, (str) => {
    assertTrue(`"${str}" should be valid.`, testFunc(str));
  });
  googArray.forEach(invalid, (str) => {
    assertFalse(`"${str}" should be invalid.`, testFunc(str));
  });
}

/**
 * Asserts that parsing the inputString produces a list of email addresses
 * containing the specified address strings, irrespective of their order.
 * @param {string} inputString A raw address list.
 * @param {Array<string>} expectedList The expected results.
 * @param {string=} opt_message An assertion message.
 * @return {string} the resulting email address objects.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertParsedList(inputString, expectedList, opt_message) {
  const message = opt_message || 'Should parse address correctly';
  const result = EmailAddress.parseList(inputString);
  assertEquals(
      'Should have correct # of addresses', expectedList.length, result.length);
  for (let i = 0; i < expectedList.length; ++i) {
    assertEquals(message, expectedList[i], result[i].getAddress());
  }
  return result;
}
testSuite({
  testparseList() {
    assertParsedList('', [], 'Failed to parse empty stringy');
    assertParsedList(',,', [], 'Failed to parse string with commas only');

    assertParsedList('<foo@gmail.com>', ['foo@gmail.com']);

    assertParsedList(
        '<foo@gmail.com>,', ['foo@gmail.com'],
        'Failed to parse 1 address with trailing comma');

    assertParsedList(
        '<foo@gmail.com>, ', ['foo@gmail.com'],
        'Failed to parse 1 address with trailing whitespace and comma');

    assertParsedList(
        ',<foo@gmail.com>', ['foo@gmail.com'],
        'Failed to parse 1 address with leading comma');

    assertParsedList(
        ' ,<foo@gmail.com>', ['foo@gmail.com'],
        'Failed to parse 1 address with leading whitespace and comma');

    assertParsedList(
        '<foo@gmail.com>, <bar@gmail.com>', ['foo@gmail.com', 'bar@gmail.com'],
        'Failed to parse 2 email addresses');

    assertParsedList(
        '<foo@gmail.com>, <bar@gmail.com>,', ['foo@gmail.com', 'bar@gmail.com'],
        'Failed to parse 2 email addresses and trailing comma');

    assertParsedList(
        '<foo@gmail.com>, <bar@gmail.com>, ',
        ['foo@gmail.com', 'bar@gmail.com'],
        'Failed to parse 2 email addresses, trailing comma and whitespace');

    assertParsedList(
        'John Doe <john@gmail.com>; Jane Doe <jane@gmail.com>, ' +
            '<jerry@gmail.com>',
        ['john@gmail.com', 'jane@gmail.com', 'jerry@gmail.com'],
        'Failed to parse addresses with semicolon separator');
  },

  testparseListOpenersAndClosers() {
    assertParsedList(
        'aaa@gmail.com, "bbb@gmail.com", <ccc@gmail.com>, ' +
            '(ddd@gmail.com), [eee@gmail.com]',
        [
          'aaa@gmail.com',
          '"bbb@gmail.com"',
          'ccc@gmail.com',
          '(ddd@gmail.com)',
          '[eee@gmail.com]',
        ],
        'Failed to handle all 5 opener/closer characters');
  },

  testparseListIdn() {
    const idnaddr = 'mailtest@\u4F8B\u3048.\u30C6\u30B9\u30C8';
    assertParsedList(idnaddr, [idnaddr]);
  },

  testparseListWithQuotedSpecialChars() {
    const res = assertParsedList(
        'a\\"b\\"c <d@e.f>,"g\\"h\\"i\\\\" <j@k.l>', ['d@e.f', 'j@k.l']);
    assertEquals('Wrong name 0', 'a"b"c', res[0].getName());
    assertEquals('Wrong name 1', 'g"h"i\\', res[1].getName());
  },

  testparseListWithCommaInLocalPart() {
    const res = assertParsedList(
        '"Doe, John" <doe.john@gmail.com>, <someone@gmail.com>',
        ['doe.john@gmail.com', 'someone@gmail.com']);

    assertEquals('Doe, John', res[0].getName());
    assertEquals('', res[1].getName());
  },

  testparseListWithWhitespaceSeparatedEmails() {
    const res = assertParsedList(
        'a@b.com <c@d.com> e@f.com "G H" <g@h.com> i@j.com',
        ['a@b.com', 'c@d.com', 'e@f.com', 'g@h.com', 'i@j.com']);
    assertEquals('G H', res[3].getName());
  },

  testparseListSystemNewlines() {
    // These Windows newlines can be inserted in IE8, or copied-and-pasted from
    // bad data on a Mac, as seen in bug 11081852.
    assertParsedList(
        'a@b.com\r\nc@d.com', ['a@b.com', 'c@d.com'],
        'Failed to parse Windows newlines');
    assertParsedList(
        'a@b.com\nc@d.com', ['a@b.com', 'c@d.com'],
        'Failed to parse *nix newlines');
    assertParsedList(
        'a@b.com\n\rc@d.com', ['a@b.com', 'c@d.com'],
        'Failed to parse obsolete newlines');
    assertParsedList(
        'a@b.com\rc@d.com', ['a@b.com', 'c@d.com'],
        'Failed to parse pre-OS X Mac newlines');
  },

  testparseListBidiMarks() {
    // These bidi marks can be copy pasted from an RTL formatted email
    assertParsedList(
        `ab ${LRE}${PDF}${RLM}<${LRE}a@b.com${PDF}${RLM}>` +
            `${PDF}${RLM},c@d.com${PDF}${RLM} `,
        ['a@b.com', 'c@d.com']);
    assertParsedList(
        `${PDF}ab ${RLE}${PDF}${LRM}<${RLE}a@b.com${PDF}${LRM}>` +
            `${PDF}${LRM},c@d.com${PDF}${LRM} `,
        ['a@b.com', 'c@d.com']);
  },

  testToString() {
    const f = (str) => EmailAddress.parse(str).toString();

    // No modification.
    assertEquals('JOHN Doe <john@gmail.com>', f('JOHN Doe <john@gmail.com>'));

    // Extra spaces.
    assertEquals(
        'JOHN Doe <john@gmail.com>', f(' JOHN  Doe  <john@gmail.com> '));

    // No name.
    assertEquals('john@gmail.com', f('<john@gmail.com>'));
    assertEquals('john@gmail.com', f('john@gmail.com'));

    // No address.
    assertEquals('JOHN Doe', f('JOHN Doe <>'));

    // Special chars in the name.
    assertEquals(
        '"JOHN, Doe" <john@gmail.com>', f('JOHN, Doe <john@gmail.com>'));
    assertEquals(
        '"JOHN(Johnny) Doe" <john@gmail.com>',
        f('JOHN(Johnny) Doe <john@gmail.com>'));
    assertEquals(
        '"JOHN[Johnny] Doe" <john@gmail.com>',
        f('JOHN[Johnny] Doe <john@gmail.com>'));
    assertEquals(
        '"JOHN@work Doe" <john@gmail.com>',
        f('JOHN@work Doe <john@gmail.com>'));
    assertEquals(
        '"JOHN:theking Doe" <john@gmail.com>',
        f('JOHN:theking Doe <john@gmail.com>'));
    assertEquals(
        '"JOHN\\\\ Doe" <john@gmail.com>', f('JOHN\\ Doe <john@gmail.com>'));
    assertEquals(
        '"JOHN.com Doe" <john@gmail.com>', f('JOHN.com Doe <john@gmail.com>'));

    // Already quoted.
    assertEquals(
        '"JOHN, Doe" <john@gmail.com>', f('"JOHN, Doe" <john@gmail.com>'));

    // Needless quotes.
    assertEquals('JOHN Doe <john@gmail.com>', f('"JOHN Doe" <john@gmail.com>'));
    // Not quoted-string, but has double quotes.
    assertEquals(
        '"JOHN, Doe" <john@gmail.com>', f('JOHN, "Doe" <john@gmail.com>'));

    // No special characters other than quotes.
    assertEquals('JOHN Doe <john@gmail.com>', f('JOHN "Doe" <john@gmail.com>'));

    // Escaped quotes are also removed.
    assertEquals(
        '"JOHN, Doe" <john@gmail.com>', f('JOHN, \\"Doe\\" <john@gmail.com>'));
  },

  testIsValid() {
    const valid = [
      'e@b.eu', '<a.b+foo@c.com>', 'eric <e@b.com>', '"e" <e@b.com>',
      'a@FOO.MUSEUM', 'bla@b.co.ac.uk', 'bla@a.b.com', 'o\'hara@gm.com',
      'plus+is+allowed@gmail.com', '!/#$%&\'*+-=~|`{}?^_@expample.com',
      'confirm-bhk=modulo.org@yahoogroups.com',
      `blah blahson ${LRE}${PDF}${RLM}<${LRE}blah@blah.com${PDF}${RLM}>`
    ];
    const invalid = [
      'e',
      '',
      'e @c.com',
      'a@b',
      'foo.com',
      'foo@c..com',
      'test@gma=il.com',
      'aaa@gmail',
      'has some spaces@gmail.com',
      'has@three@at@signs.com',
      '@no-local-part.com',
      'み.ん-あ@みんあ.みんあ',
      'みんあ@test.com',
      'test@test.みんあ',
      'test@みんあ.com',
      'fullwidthfullstop@sld' +
          '\uff0e' +
          'tld',
      'ideographicfullstop@sld' +
          '\u3002' +
          'tld',
      'halfwidthideographicfullstop@sld' +
          '\uff61' +
          'tld',
    ];
    doIsValidTest(EmailAddress.isValidAddress, valid, invalid);
  },

  testIsValidLocalPart() {
    const valid = [
      'e',
      'a.b+foo',
      'o\'hara',
      'user+someone',
      '!/#$%&\'*+-=~|`{}?^_',
      'confirm-bhk=modulo.org',
    ];
    const invalid = [
      'A@b@c',
      'a"b(c)d,e:f;g<h>i[j\\k]l',
      'just"not"right',
      'this is"not\\allowed',
      'this\\ still\"not\\\\allowed',
      'has some spaces',
    ];
    doIsValidTest(EmailAddress.isValidLocalPartSpec, valid, invalid);
  },

  testIsValidDomainPart() {
    const valid =
        ['example.com', 'dept.example.org', 'long.domain.with.lots.of.dots'];
    const invalid = [
      '',
      '@has.an.at.sign',
      '..has.leading.dots',
      'gma=il.com',
      'DoesNotHaveADot',
      'sld' +
          '\uff0e' +
          'tld',
      'sld' +
          '\u3002' +
          'tld',
      'sld' +
          '\uff61' +
          'tld',
    ];
    doIsValidTest(EmailAddress.isValidDomainPartSpec, valid, invalid);
  },
});
