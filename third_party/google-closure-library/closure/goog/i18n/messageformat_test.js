/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.MessageFormatTest');
goog.setTestOnly();

const MessageFormat = goog.require('goog.i18n.MessageFormat');
const NumberFormatSymbols_hr = goog.require('goog.i18n.NumberFormatSymbols_hr');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const pluralRules = goog.require('goog.i18n.pluralRules');
const testSuite = goog.require('goog.testing.testSuite');

// Testing stubs that autoreset after each test run.
const stubs = new PropertyReplacer();

testSuite({
  tearDown() {
    stubs.reset();
  },

  testEmptyPattern() {
    const fmt = new MessageFormat('');
    assertEquals('', fmt.format({}));
  },

  testMissingLeftCurlyBrace() {
    const err = assertThrows(() => {
      const fmt = new MessageFormat('\'\'{}}');
      fmt.format({});
    });
    assertEquals('Assertion failed: No matching { for }.', err.message);
  },

  testTooManyLeftCurlyBraces() {
    const err = assertThrows(() => {
      const fmt = new MessageFormat('{} {');
      fmt.format({});
    });
    assertEquals(
        'Assertion failed: There are mismatched { or } in the pattern.',
        err.message);
  },

  testSimpleReplacement() {
    const fmt = new MessageFormat('New York in {SEASON} is nice.');
    assertEquals(
        'New York in the Summer is nice.',
        fmt.format({'SEASON': 'the Summer'}));
  },

  testSimpleSelect() {
    const fmt = new MessageFormat(
        '{GENDER, select,' +
        'male {His} ' +
        'female {Her} ' +
        'other {Its}}' +
        ' bicycle is {GENDER, select, male {blue} female {red} other {green}}.');

    assertEquals('His bicycle is blue.', fmt.format({'GENDER': 'male'}));
    assertEquals('Her bicycle is red.', fmt.format({'GENDER': 'female'}));
    assertEquals('Its bicycle is green.', fmt.format({'GENDER': 'other'}));
    assertEquals('Its bicycle is green.', fmt.format({'GENDER': 'whatever'}));
  },

  testSimplePlural() {
    const fmt = new MessageFormat(
        'I see {NUM_PEOPLE, plural, offset:1 ' +
        '=0 {no one at all in {PLACE}.} ' +
        '=1 {{PERSON} in {PLACE}.} ' +
        'one {{PERSON} and one other person in {PLACE}.} ' +
        'other {{PERSON} and # other people in {PLACE}.}}');

    assertEquals(
        'I see no one at all in Belgrade.',
        fmt.format({'NUM_PEOPLE': 0, 'PLACE': 'Belgrade'}));
    assertEquals(
        'I see Markus in Berlin.',
        fmt.format({'NUM_PEOPLE': 1, 'PERSON': 'Markus', 'PLACE': 'Berlin'}));
    assertEquals(
        'I see Mark and one other person in Athens.',
        fmt.format({'NUM_PEOPLE': 2, 'PERSON': 'Mark', 'PLACE': 'Athens'}));
    assertEquals(
        'I see Cibu and 99 other people in the cubes.',
        fmt.format(
            {'NUM_PEOPLE': 100, 'PERSON': 'Cibu', 'PLACE': 'the cubes'}));
  },

  testSimplePluralNoOffset() {
    const fmt = new MessageFormat(
        'I see {NUM_PEOPLE, plural, ' +
        '=0 {no one at all} ' +
        '=1 {{PERSON}} ' +
        'one {{PERSON} and one other person} ' +
        'other {{PERSON} and # other people}} in {PLACE}.');

    assertEquals(
        'I see no one at all in Belgrade.',
        fmt.format({'NUM_PEOPLE': 0, 'PLACE': 'Belgrade'}));
    assertEquals(
        'I see Markus in Berlin.',
        fmt.format({'NUM_PEOPLE': 1, 'PERSON': 'Markus', 'PLACE': 'Berlin'}));
    assertEquals(
        'I see Mark and 2 other people in Athens.',
        fmt.format({'NUM_PEOPLE': 2, 'PERSON': 'Mark', 'PLACE': 'Athens'}));
    assertEquals(
        'I see Cibu and 100 other people in the cubes.',
        fmt.format(
            {'NUM_PEOPLE': 100, 'PERSON': 'Cibu', 'PLACE': 'the cubes'}));
  },

  testSelectNestedInPlural() {
    const fmt = new MessageFormat(
        '{CIRCLES, plural, ' +
        'one {{GENDER, select, ' +
        '  female {{WHO} added you to her circle} ' +
        '  other  {{WHO} added you to his circle}}} ' +
        'other {{GENDER, select, ' +
        '  female {{WHO} added you to her # circles} ' +
        '  other  {{WHO} added you to his # circles}}}}');

    assertEquals(
        'Jelena added you to her circle',
        fmt.format({'GENDER': 'female', 'WHO': 'Jelena', 'CIRCLES': 1}));
    assertEquals(
        'Milan added you to his 1,234 circles',
        fmt.format({'GENDER': 'male', 'WHO': 'Milan', 'CIRCLES': 1234}));
  },

  testPluralNestedInSelect() {
    // Added offset just for testing purposes. It doesn't make sense
    // to have it otherwise.
    const fmt = new MessageFormat(
        '{GENDER, select, ' +
        'female {{NUM_GROUPS, plural, ' +
        '  one {{WHO} added you to her group} ' +
        '  other {{WHO} added you to her # groups}}} ' +
        'other {{NUM_GROUPS, plural, offset:1' +
        '  one {{WHO} added you to his group} ' +
        '  other {{WHO} added you to his # groups}}}}');

    assertEquals(
        'Jelena added you to her group',
        fmt.format({'GENDER': 'female', 'WHO': 'Jelena', 'NUM_GROUPS': 1}));
    assertEquals(
        'Milan added you to his 1,233 groups',
        fmt.format({'GENDER': 'male', 'WHO': 'Milan', 'NUM_GROUPS': 1234}));
  },

  testLiteralOpenCurlyBrace() {
    const fmt = new MessageFormat(
        'Anna\'s house' +
        ' has \'{0} and # in the roof\' and {NUM_COWS} cows.');
    assertEquals(
        'Anna\'s house has {0} and # in the roof and 5 cows.',
        fmt.format({'NUM_COWS': '5'}));
  },

  testLiteralClosedCurlyBrace() {
    const fmt = new MessageFormat(
        'Anna\'s house' +
        ' has \'{\'0\'} and # in the roof\' and {NUM_COWS} cows.');
    assertEquals(
        'Anna\'s house has {0} and # in the roof and 5 cows.',
        fmt.format({'NUM_COWS': '5'}));
    // Regression for: b/34764827
    assertEquals(
        'Anna\'s house has {0} and # in the roof and 8 cows.',
        fmt.format({'NUM_COWS': '8'}));
  },

  testLiteralPoundSign() {
    const fmt = new MessageFormat(
        'Anna\'s house' +
        ' has \'{0}\' and \'# in the roof\' and {NUM_COWS} cows.');
    assertEquals(
        'Anna\'s house has {0} and # in the roof and 5 cows.',
        fmt.format({'NUM_COWS': '5'}));
    // Regression for: b/34764827
    assertEquals(
        'Anna\'s house has {0} and # in the roof and 10 cows.',
        fmt.format({'NUM_COWS': '10'}));
  },

  testNoLiteralsForSingleQuotes() {
    const fmt = new MessageFormat(
        'Anna\'s house' +
        ' \'has {NUM_COWS} cows\'.');
    assertEquals(
        'Anna\'s house \'has 5 cows\'.', fmt.format({'NUM_COWS': '5'}));
  },

  testConsecutiveSingleQuotesAreReplacedWithOneSingleQuote() {
    const fmt = new MessageFormat('Anna\'\'s house a\'{\'\'\'\'b\'');
    assertEquals('Anna\'s house a{\'\'b', fmt.format({}));
  },

  testConsecutiveSingleQuotesBeforeSpecialCharDontCreateLiteral() {
    const fmt = new MessageFormat('a\'\'{NUM_COWS}\'b');
    assertEquals('a\'5\'b', fmt.format({'NUM_COWS': '5'}));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSerbianSimpleSelect() {
    stubs.set(pluralRules, 'select', pluralRules.beSelect_);

    const fmt = new MessageFormat(
        '{GENDER, select, ' +
        'female {Njen} other {Njegov}} bicikl je ' +
        '{GENDER, select, female {crven} other {plav}}.');

    assertEquals('Njegov bicikl je plav.', fmt.format({'GENDER': 'male'}));
    assertEquals('Njen bicikl je crven.', fmt.format({'GENDER': 'female'}));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSerbianSimplePlural() {
    stubs.set(pluralRules, 'select', pluralRules.beSelect_);

    const fmt = new MessageFormat(
        'Ja {NUM_PEOPLE, plural, offset:1 ' +
        '=0 {ne vidim nikoga} ' +
        '=1 {vidim {PERSON}} ' +
        'one {vidim {PERSON} i jos # osobu} ' +
        'few {vidim {PERSON} i jos # osobe} ' +
        'many {vidim {PERSON} i jos # osoba} ' +
        'other {{PERSON} i jos # osoba}} ' +
        'u {PLACE}.');

    assertEquals(
        'Ja ne vidim nikoga u Beogradu.',
        fmt.format({'NUM_PEOPLE': 0, 'PLACE': 'Beogradu'}));
    assertEquals(
        'Ja vidim Markusa u Berlinu.',
        fmt.format({'NUM_PEOPLE': 1, 'PERSON': 'Markusa', 'PLACE': 'Berlinu'}));
    assertEquals(
        'Ja vidim Marka i jos 1 osobu u Atini.',
        fmt.format({'NUM_PEOPLE': 2, 'PERSON': 'Marka', 'PLACE': 'Atini'}));
    assertEquals(
        'Ja vidim Petra i jos 3 osobe u muzeju.',
        fmt.format({'NUM_PEOPLE': 4, 'PERSON': 'Petra', 'PLACE': 'muzeju'}));
    assertEquals(
        'Ja vidim Cibua i jos 99 osoba u bazenu.',
        fmt.format({'NUM_PEOPLE': 100, 'PERSON': 'Cibua', 'PLACE': 'bazenu'}));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSerbianSimplePluralNoOffset() {
    stubs.set(pluralRules, 'select', pluralRules.beSelect_);

    const fmt = new MessageFormat(
        'Ja {NUM_PEOPLE, plural, ' +
        '=0 {ne vidim nikoga} ' +
        '=1 {vidim {PERSON}} ' +
        'one {vidim {PERSON} i jos # osobu} ' +
        'few {vidim {PERSON} i jos # osobe} ' +
        'many {vidim {PERSON} i jos # osoba} ' +
        'other {{PERSON} i jos # osoba}} ' +
        'u {PLACE}.');

    assertEquals(
        'Ja ne vidim nikoga u Beogradu.',
        fmt.format({'NUM_PEOPLE': 0, 'PLACE': 'Beogradu'}));
    assertEquals(
        'Ja vidim Markusa u Berlinu.',
        fmt.format({'NUM_PEOPLE': 1, 'PERSON': 'Markusa', 'PLACE': 'Berlinu'}));
    assertEquals(
        'Ja vidim Marka i jos 21 osobu u Atini.',
        fmt.format({'NUM_PEOPLE': 21, 'PERSON': 'Marka', 'PLACE': 'Atini'}));
    assertEquals(
        'Ja vidim Petra i jos 3 osobe u muzeju.',
        fmt.format({'NUM_PEOPLE': 3, 'PERSON': 'Petra', 'PLACE': 'muzeju'}));
    assertEquals(
        'Ja vidim Cibua i jos 100 osoba u bazenu.',
        fmt.format({'NUM_PEOPLE': 100, 'PERSON': 'Cibua', 'PLACE': 'bazenu'}));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSerbianSelectNestedInPlural() {
    stubs.set(pluralRules, 'select', pluralRules.beSelect_);
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_hr);

    const fmt = new MessageFormat(
        '{CIRCLES, plural, ' +
        'one {{GENDER, select, ' +
        '  female {{WHO} vas je dodala u njen # kruzok} ' +
        '  other  {{WHO} vas je dodao u njegov # kruzok}}} ' +
        'few {{GENDER, select, ' +
        '  female {{WHO} vas je dodala u njena # kruzoka} ' +
        '  other  {{WHO} vas je dodao u njegova # kruzoka}}} ' +
        'many {{GENDER, select, ' +
        '  female {{WHO} vas je dodala u njenih # kruzoka} ' +
        '  other  {{WHO} vas je dodao u njegovih # kruzoka}}} ' +
        'other {{GENDER, select, ' +
        '  female {{WHO} vas je dodala u njenih # kruzoka} ' +
        '  other  {{WHO} vas je dodao u njegovih # kruzoka}}}}');

    assertEquals(
        'Jelena vas je dodala u njen 21 kruzok',
        fmt.format({'GENDER': 'female', 'WHO': 'Jelena', 'CIRCLES': 21}));
    assertEquals(
        'Jelena vas je dodala u njena 3 kruzoka',
        fmt.format({'GENDER': 'female', 'WHO': 'Jelena', 'CIRCLES': 3}));
    assertEquals(
        'Jelena vas je dodala u njenih 5 kruzoka',
        fmt.format({'GENDER': 'female', 'WHO': 'Jelena', 'CIRCLES': 5}));
    assertEquals(
        'Milan vas je dodao u njegovih 1.235 kruzoka',
        fmt.format({'GENDER': 'male', 'WHO': 'Milan', 'CIRCLES': 1235}));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFallbackToOtherOptionInPlurals() {
    // Use Arabic plural rules since they have all six cases.
    // Only locale and numbers matter, the actual language of the message
    // does not.
    stubs.set(pluralRules, 'select', pluralRules.arSelect_);

    const fmt = new MessageFormat(
        '{NUM_MINUTES, plural, ' +
        'other {# minutes}}');

    // These numbers exercise all cases for the arabic plural rules.
    assertEquals('0 minutes', fmt.format({'NUM_MINUTES': 0}));
    assertEquals('1 minutes', fmt.format({'NUM_MINUTES': 1}));
    assertEquals('2 minutes', fmt.format({'NUM_MINUTES': 2}));
    assertEquals('3 minutes', fmt.format({'NUM_MINUTES': 3}));
    assertEquals('11 minutes', fmt.format({'NUM_MINUTES': 11}));
    assertEquals('1.5 minutes', fmt.format({'NUM_MINUTES': 1.5}));
  },

  testPoundShowsNumberMinusOffsetInAllCases() {
    const fmt = new MessageFormat(
        '{SOME_NUM, plural, offset:1 ' +
        '=0 {#} =1 {#} =2 {#}one {#} other {#}}');

    assertEquals('-1', fmt.format({'SOME_NUM': '0'}));
    assertEquals('0', fmt.format({'SOME_NUM': '1'}));
    assertEquals('1', fmt.format({'SOME_NUM': '2'}));
    assertEquals('20', fmt.format({'SOME_NUM': '21'}));
  },

  testSpecialCharactersInParamaterDontChangeFormat() {
    const fmt = new MessageFormat(
        '{SOME_NUM, plural,' +
        'other {# {GROUP}}}');

    // Test pound sign.
    assertEquals(
        '10 group#1', fmt.format({'SOME_NUM': '10', 'GROUP': 'group#1'}));
    // Test other special characters in parameters, like { and }.
    assertEquals('10 } {', fmt.format({'SOME_NUM': '10', 'GROUP': '} {'}));
  },

  testMissingOrInvalidPluralParameter() {
    const fmt = new MessageFormat(
        '{SOME_NUM, plural,' +
        'other {result}}');

    // Key name doesn't match A != SOME_NUM.
    assertEquals(
        'Undefined or invalid parameter - SOME_NUM', fmt.format({A: '10'}));

    // Value is not a number.
    assertEquals(
        'Undefined or invalid parameter - SOME_NUM',
        fmt.format({'SOME_NUM': 'Value'}));
  },

  testMissingSelectParameter() {
    const fmt = new MessageFormat(
        '{GENDER, select,' +
        'other {result}}');

    // Key name doesn't match A != GENDER.
    assertEquals('Undefined parameter - GENDER', fmt.format({A: 'female'}));
  },

  testMissingSimplePlaceholder() {
    const fmt = new MessageFormat('{result}');

    // Key name doesn't match A != result.
    assertEquals('Undefined parameter - result', fmt.format({A: 'none'}));
  },

  testPluralWithIgnorePound() {
    const fmt = new MessageFormat(
        '{SOME_NUM, plural,' +
        'other {# {GROUP}}}');

    // Test pound sign.
    assertEquals(
        '# group#1',
        fmt.formatIgnoringPound({'SOME_NUM': '10', 'GROUP': 'group#1'}));
    // Test other special characters in parameters, like { and }.
    assertEquals(
        '# } {', fmt.formatIgnoringPound({'SOME_NUM': '10', 'GROUP': '} {'}));
  },

  testSimplePluralWithIgnorePound() {
    const fmt = new MessageFormat(
        'I see {NUM_PEOPLE, plural, offset:1 ' +
        '=0 {no one at all in {PLACE}.} ' +
        '=1 {{PERSON} in {PLACE}.} ' +
        'one {{PERSON} and one other person in {PLACE}.} ' +
        'other {{PERSON} and # other people in {PLACE}.}}');

    assertEquals(
        'I see Cibu and # other people in the cubes.',
        fmt.formatIgnoringPound(
            {'NUM_PEOPLE': 100, 'PERSON': 'Cibu', 'PLACE': 'the cubes'}));
  },

  testSimpleOrdinal() {
    const fmt = new MessageFormat(
        '{NUM_FLOOR, selectordinal, ' +
        'one {Take the elevator to the #st floor.}' +
        'two {Take the elevator to the #nd floor.}' +
        'few {Take the elevator to the #rd floor.}' +
        'other {Take the elevator to the #th floor.}}');

    assertEquals(
        'Take the elevator to the 1st floor.', fmt.format({'NUM_FLOOR': 1}));
    assertEquals(
        'Take the elevator to the 2nd floor.', fmt.format({'NUM_FLOOR': 2}));
    assertEquals(
        'Take the elevator to the 3rd floor.', fmt.format({'NUM_FLOOR': 3}));
    assertEquals(
        'Take the elevator to the 4th floor.', fmt.format({'NUM_FLOOR': 4}));
    assertEquals(
        'Take the elevator to the 23rd floor.', fmt.format({'NUM_FLOOR': 23}));
    // Esoteric example.
    assertEquals(
        'Take the elevator to the 0th floor.', fmt.format({'NUM_FLOOR': 0}));
  },

  testOrdinalWithNegativeValue() {
    const fmt = new MessageFormat(
        '{NUM_FLOOR, selectordinal, ' +
        'one {Take the elevator to the #st floor.}' +
        'two {Take the elevator to the #nd floor.}' +
        'few {Take the elevator to the #rd floor.}' +
        'other {Take the elevator to the #th floor.}}');

    assertEquals(
        'Take the elevator to the -1st floor.', fmt.format({'NUM_FLOOR': -1}));
    assertEquals(
        'Take the elevator to the -2nd floor.', fmt.format({'NUM_FLOOR': -2}));
    assertEquals(
        'Take the elevator to the -3rd floor.', fmt.format({'NUM_FLOOR': -3}));
    assertEquals(
        'Take the elevator to the -4th floor.', fmt.format({'NUM_FLOOR': -4}));
  },

  testSimpleOrdinalWithIgnorePound() {
    const fmt = new MessageFormat(
        '{NUM_FLOOR, selectordinal, ' +
        'one {Take the elevator to the #st floor.}' +
        'two {Take the elevator to the #nd floor.}' +
        'few {Take the elevator to the #rd floor.}' +
        'other {Take the elevator to the #th floor.}}');

    assertEquals(
        'Take the elevator to the #th floor.',
        fmt.formatIgnoringPound({'NUM_FLOOR': 100}));
  },

  testMissingOrInvalidOrdinalParameter() {
    const fmt = new MessageFormat(
        '{SOME_NUM, selectordinal,' +
        'other {result}}');

    // Key name doesn't match A != SOME_NUM.
    assertEquals(
        'Undefined or invalid parameter - SOME_NUM', fmt.format({A: '10'}));

    // Value is not a number.
    assertEquals(
        'Undefined or invalid parameter - SOME_NUM',
        fmt.format({'SOME_NUM': 'Value'}));
  },
});
