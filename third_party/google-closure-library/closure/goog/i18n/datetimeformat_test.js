/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} swapping implementation using fully qualified name
 */
goog.module('goog.i18n.DateTimeFormatTest');
goog.setTestOnly();


const LocaleFeature = goog.require('goog.i18n.LocaleFeature');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');

// Note that exact formatted output equivalence between Closure and
// ECMAScript implementations is not required in all cases.
const replacer = new PropertyReplacer();

const DateDate = goog.require('goog.date.Date');
const DateTime = goog.require('goog.date.DateTime');
const DateTimeFormat = goog.require('goog.i18n.DateTimeFormat');
/** @suppress {extraRequire} */
const DateTimePatterns = goog.require('goog.i18n.DateTimePatterns');
const DateTimePatterns_ar_EG = goog.require('goog.i18n.DateTimePatterns_ar_EG');
const DateTimePatterns_bg = goog.require('goog.i18n.DateTimePatterns_bg');
const DateTimePatterns_de = goog.require('goog.i18n.DateTimePatterns_de');
const DateTimePatterns_en = goog.require('goog.i18n.DateTimePatterns_en');
const DateTimePatterns_en_XA = goog.require('goog.i18n.DateTimePatterns_en_XA');
const DateTimePatterns_fa = goog.require('goog.i18n.DateTimePatterns_fa');
const DateTimePatterns_fr = goog.require('goog.i18n.DateTimePatterns_fr');
const DateTimePatterns_ja = goog.require('goog.i18n.DateTimePatterns_ja');
const DateTimePatterns_sv = goog.require('goog.i18n.DateTimePatterns_sv');
const DateTimePatterns_zh_HK = goog.require('goog.i18n.DateTimePatterns_zh_HK');
const DateTimePatterns_zh_Hant_TW = goog.require('goog.i18n.DateTimePatterns_zh_Hant_TW');
/** @suppress {extraRequire} */
const DateTimeSymbols = goog.require('goog.i18n.DateTimeSymbols');
const DateTimeSymbols_ar = goog.require('goog.i18n.DateTimeSymbols_ar');
const DateTimeSymbols_ar_AE = goog.require('goog.i18n.DateTimeSymbols_ar_AE');
const DateTimeSymbols_ar_EG = goog.require('goog.i18n.DateTimeSymbols_ar_EG');
const DateTimeSymbols_ar_SA = goog.require('goog.i18n.DateTimeSymbols_ar_SA');
const DateTimeSymbols_bg = goog.require('goog.i18n.DateTimeSymbols_bg');
const DateTimeSymbols_bn = goog.require('goog.i18n.DateTimeSymbols_bn');
const DateTimeSymbols_bn_BD = goog.require('goog.i18n.DateTimeSymbols_bn_BD');
const DateTimeSymbols_de = goog.require('goog.i18n.DateTimeSymbols_de');
const DateTimeSymbols_en = goog.require('goog.i18n.DateTimeSymbols_en');
const DateTimeSymbols_en_GB = goog.require('goog.i18n.DateTimeSymbols_en_GB');
const DateTimeSymbols_en_IE = goog.require('goog.i18n.DateTimeSymbols_en_IE');
const DateTimeSymbols_en_IN = goog.require('goog.i18n.DateTimeSymbols_en_IN');
const DateTimeSymbols_en_US = goog.require('goog.i18n.DateTimeSymbols_en_US');
const DateTimeSymbols_en_XA = goog.require('goog.i18n.DateTimeSymbols_en_XA');
const DateTimeSymbols_fa = goog.require('goog.i18n.DateTimeSymbols_fa');
const DateTimeSymbols_fr = goog.require('goog.i18n.DateTimeSymbols_fr');
const DateTimeSymbols_fr_DJ = goog.require('goog.i18n.DateTimeSymbols_fr_DJ');
const DateTimeSymbols_he_IL = goog.require('goog.i18n.DateTimeSymbols_he_IL');
const DateTimeSymbols_ja = goog.require('goog.i18n.DateTimeSymbols_ja');
const DateTimeSymbols_ml = goog.require('goog.i18n.DateTimeSymbols_ml');
const DateTimeSymbols_mr = goog.require('goog.i18n.DateTimeSymbols_mr');
const DateTimeSymbols_my = goog.require('goog.i18n.DateTimeSymbols_my');
const DateTimeSymbols_ne = goog.require('goog.i18n.DateTimeSymbols_ne');
const DateTimeSymbols_ro_RO = goog.require('goog.i18n.DateTimeSymbols_ro_RO');
const DateTimeSymbols_sv = goog.require('goog.i18n.DateTimeSymbols_sv');
const DateTimeSymbols_zh_HK = goog.require('goog.i18n.DateTimeSymbols_zh_HK');
const DateTimeSymbols_zh_Hant_TW = goog.require('goog.i18n.DateTimeSymbols_zh_Hant_TW');
const DateTimeSymbols_zh_TW = goog.require('goog.i18n.DateTimeSymbols_zh_TW');
const TimeZone = goog.require('goog.i18n.TimeZone');

const {addI18nMapping, assertI18nEquals} = goog.require('goog.testing.i18n.asserts');
const {removeWhitespace} = goog.require('goog.testing.i18n.whitespace');

const {DayPeriods_zh_Hant, setDayPeriods} = goog.require('goog.i18n.DayPeriods');

const UtcDateTime = goog.require('goog.date.UtcDateTime');

const testSuite = goog.require('goog.testing.testSuite');

// Initial values
replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en);
replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);

// Helpers for native mode
/**
 * Changes status of ECMASCRIPT mode
 * @param {boolean} new_setting
 */
function setNativeMode(new_setting) {
  replacer.set(LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', new_setting);
}

// Sets up goog.USE_ECMASCRIPT_I18N_DATETIMEF flag in each function.
let testECMAScriptOptions = [false];
// Don't test native ECMASCript on IE11.
if (Intl.DateTimeFormat) {
  // Add test if the browser environment supports ECMAScript implementation.
  if (!goog.labs.userAgent.browser.isIE()) {
    testECMAScriptOptions.unshift(true);  // Test native before Javascript
  }
}

/**
 * Gets current setting for native ECMAScript mode
 * @return {boolean}
 */
function getNativeMode() {
  return LocaleFeature.USE_ECMASCRIPT_I18N_DATETIMEF;
}

/**
 * Returns string without RTL or LTR markers.
 * @param {string} input
 * @return {string}
 */
function removeDirectionMarkers(input) {
  return input.replace(/[\u200e\u200f]/g, '');
}

/**
 * Returns string without LTR markers.
 * @param {string} input
 * @return {string}
 */
function removeLtrMarkers(input) {
  return input.replace(/[\u200e]/g, '');
}

/**
 * Check result to deal with variants in formatting innative mode.
 * param {boolean} nativeMode
 * param {string} actual
 */
function checkNativeResult(nativeMode, expected, actual) {
  if (nativeMode) {
    // Normalize for timezone names
    expected = expected.replaceAll('UTC', 'GMT');
  }
  assertEquals('nativeMode=' + nativeMode, expected, actual);
}

/**
 * Helpers to make tests work regardless of the timeZone we're in.
 * Gets timezone string of the date.
 * @param {!DateDate} date
 * @return {string}
 */
function timezoneString(date) {
  const timeZone = TimeZone.createTimeZone(date.getTimezoneOffset());
  return timeZone.getShortName(date);
}

/**
 * Gets timezone id from the date.
 * @param {!DateDate} date
 * @return {string}
 * @suppress {checkTypes} suppression added to enable type checking
 */
function timezoneId(date) {
  const timeZone = TimeZone.createTimeZone(date.getTimezoneOffset());
  return timeZone.getTimeZoneId(date);
}

/**
 * Gets timezone RFC string from the date.
 * @param {!DateDate} date
 * @return {string}
 */
function timezoneStringRFC(date) {
  const timeZone = TimeZone.createTimeZone(date.getTimezoneOffset());
  return timeZone.getRFCTimeZoneString(date);
}

// Where could such data be found
// In js_i18n_data in http://go/i18n_dir, we have a bunch of files with names
// like TimeZoneConstant__<locale>.js
// We strongly discourage you to use them directly as those data can make
// your client code bloated. You should try to provide this data from server
// in a selective manner. In typical scenario, user's time zone is retrieved
// and only data for that time zone should be provided.
const americaLosAngelesData = {
  'transitions': [
    2770,   60, 7137,   0, 11506,  60, 16041,  0, 20410,  60, 24777,  0,
    29146,  60, 33513,  0, 35194,  60, 42249,  0, 45106,  60, 50985,  0,
    55354,  60, 59889,  0, 64090,  60, 68625,  0, 72994,  60, 77361,  0,
    81730,  60, 86097,  0, 90466,  60, 94833,  0, 99202,  60, 103569, 0,
    107938, 60, 112473, 0, 116674, 60, 121209, 0, 125578, 60, 129945, 0,
    134314, 60, 138681, 0, 143050, 60, 147417, 0, 151282, 60, 156153, 0,
    160018, 60, 165057, 0, 168754, 60, 173793, 0, 177490, 60, 182529, 0,
    186394, 60, 191265, 0, 195130, 60, 200001, 0, 203866, 60, 208905, 0,
    212602, 60, 217641, 0, 221338, 60, 226377, 0, 230242, 60, 235113, 0,
    238978, 60, 243849, 0, 247714, 60, 252585, 0, 256450, 60, 261489, 0,
    265186, 60, 270225, 0, 273922, 60, 278961, 0, 282826, 60, 287697, 0,
    291562, 60, 296433, 0, 300298, 60, 305337, 0, 309034, 60, 314073, 0,
    317770, 60, 322809, 0, 326002, 60, 331713, 0, 334738, 60, 340449, 0,
    343474, 60, 349185, 0, 352378, 60, 358089, 0, 361114, 60, 366825, 0,
    369850, 60, 375561, 0, 378586, 60, 384297, 0, 387322, 60, 393033, 0,
    396058, 60, 401769, 0, 404962, 60, 410673, 0, 413698, 60, 419409, 0,
    422434, 60, 428145, 0, 431170, 60, 436881, 0, 439906, 60, 445617, 0,
    448810, 60, 454521, 0, 457546, 60, 463257, 0, 466282, 60, 471993, 0,
    475018, 60, 480729, 0, 483754, 60, 489465, 0, 492490, 60, 498201, 0,
    501394, 60, 507105, 0, 510130, 60, 515841, 0, 518866, 60, 524577, 0,
    527602, 60, 533313, 0, 536338, 60, 542049, 0, 545242, 60, 550953, 0,
    553978, 60, 559689, 0, 562714, 60, 568425, 0, 571450, 60, 577161, 0,
    580186, 60, 585897, 0, 588922, 60, 594633, 0,
  ],
  'names': ['PST', 'Pacific Standard Time', 'PDT', 'Pacific Daylight Time'],
  'names_ext': {
    STD_LONG_NAME_GMT: 'GMT-08:00',
    STD_GENERIC_LOCATION: 'Los Angeles Time',
    DST_LONG_NAME_GMT: 'GMT-07:00',
    DST_GENERIC_LOCATION: 'Los Angeles Time',
  },
  'id': 'America/Los_Angeles',
  'std_offset': -480,
};

const europeBerlinData = {
  'transitions': [
    89953,  60, 94153,  0, 98521,  60, 102889, 0, 107257, 60, 111625, 0,
    115993, 60, 120361, 0, 124729, 60, 129265, 0, 133633, 60, 138001, 0,
    142369, 60, 146737, 0, 151105, 60, 155473, 0, 159841, 60, 164209, 0,
    168577, 60, 172945, 0, 177313, 60, 181849, 0, 186217, 60, 190585, 0,
    194953, 60, 199321, 0, 203689, 60, 208057, 0, 212425, 60, 216793, 0,
    221161, 60, 225529, 0, 230065, 60, 235105, 0, 238801, 60, 243841, 0,
    247537, 60, 252577, 0, 256273, 60, 261481, 0, 265009, 60, 270217, 0,
    273745, 60, 278953, 0, 282649, 60, 287689, 0, 291385, 60, 296425, 0,
    300121, 60, 305329, 0, 308857, 60, 314065, 0, 317593, 60, 322801, 0,
    326329, 60, 331537, 0, 335233, 60, 340273, 0, 343969, 60, 349009, 0,
    352705, 60, 357913, 0, 361441, 60, 366649, 0, 370177, 60, 375385, 0,
    379081, 60, 384121, 0, 387817, 60, 392857, 0, 396553, 60, 401593, 0,
    405289, 60, 410497, 0, 414025, 60, 419233, 0, 422761, 60, 427969, 0,
    431665, 60, 436705, 0, 440401, 60, 445441, 0, 449137, 60, 454345, 0,
    457873, 60, 463081, 0, 466609, 60, 471817, 0, 475513, 60, 480553, 0,
    484249, 60, 489289, 0, 492985, 60, 498025, 0, 501721, 60, 506929, 0,
    510457, 60, 515665, 0, 519193, 60, 524401, 0, 528097, 60, 533137, 0,
    536833, 60, 541873, 0, 545569, 60, 550777, 0, 554305, 60, 559513, 0,
    563041, 60, 568249, 0, 571777, 60, 576985, 0, 580681, 60, 585721, 0,
    589417, 60, 594457, 0,
  ],
  'names': [
    'MEZ',
    'Mitteleurop\u00e4ische Zeit',
    'MESZ',
    'Mitteleurop\u00e4ische Sommerzeit',
  ],
  'names_ext': {
    STD_LONG_NAME_GMT: 'GMT+01:00',
    STD_GENERIC_LOCATION: 'Deutschland Zeit',
    DST_LONG_NAME_GMT: 'GMT+02:00',
    DST_GENERIC_LOCATION: 'Deutschland Zeit',
  },
  'id': 'Europe/Berlin',
  'std_offset': 60,
};

/**
 * Creates a string by concatenating the week number for 7 successive days
 * @return {string}
 */
function weekInYearFor7Days() {
  const date = new Date(2013, 0, 1);  // January
  const fmt = new DateTimeFormat('w');
  let result = '';
  for (let i = 1; i <= 7; ++i) {
    date.setDate(i);
    result += fmt.format(date);
  }
  return result;
}

testSuite({
  getTestName: function() {
    return 'DateTimeFormat Tests';
  },

  setUpPage() {},

  setUp() {
    replacer.set(LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', false);
    setNativeMode(false);
    replacer.replace(goog, 'LOCALE', 'en');
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);
  },

  tearDown() {
    // We always revert to a known state
    replacer.replace(goog, 'LOCALE', 'en');
    DateTimeFormat.setEnforceAsciiDigits(false);

    replacer.reset();

    // Reset to non-native
    setNativeMode(false);
  },

  testHHmmss() {
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      const fmt = new DateTimeFormat('HH:mm:ss');
      assertEquals('13:10:10', fmt.format(date));
    }
  },

  testhhmmssa() {
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      setNativeMode(nativeMode);
      const fmt = new DateTimeFormat('h:mm:ss a');
      assertEquals('1:10:10 PM', fmt.format(date));
    }
  },

  testEEEMMMddyy() {
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const fmt = new DateTimeFormat('EEE, MMM d, yy');
      assertEquals('Do., Juli 27, 06', fmt.format(date));
    }
  },

  testEEEEMMMddyy() {
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const fmt = new DateTimeFormat('EEEE,MMMM dd, yyyy');
      assertEquals('Donnerstag,Juli 27, 2006', fmt.format(date));
    }
  },

  testyyyyMMddG() {
    const date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10, 250));
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      /** @suppress {checkTypes} suppression added to enable type checking */
      const timeZone = TimeZone.createTimeZone(420, DateTimeSymbols_de);
      const fmt = new DateTimeFormat('yyyy.MM.dd G \'at\' HH:mm:ss vvvv');
      assertEquals(
          '2006.07.27 n. Chr. at 06:10:10 Etc/GMT+7',
          fmt.format(date, timeZone));
    }
  },

  testyyyyyMMMMM() {
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      let date = new Date(2006, 6, 27, 13, 10, 10, 250);
      let fmt = new DateTimeFormat('yyyyy.MMMMM.dd GGG hh:mm aaa');
      assertEquals('02006.J.27 n. Chr. 01:10 PM', fmt.format(date));

      date = new Date(972, 11, 25, 13, 10, 10, 250);
      fmt = new DateTimeFormat('yyyyy.MMMMM.dd');
      assertEquals('00972.D.25', fmt.format(date));
    }
  },

  testQQQQyy() {
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      let date = new Date(2006, 0, 27, 13, 10, 10, 250);
      const fmt = new DateTimeFormat('QQQQ yy');
      assertEquals('1. Quartal 06', fmt.format(date));
      date = new Date(2006, 1, 27, 13, 10, 10, 250);
      assertEquals('1. Quartal 06', fmt.format(date));
      date = new Date(2006, 2, 27, 13, 10, 10, 250);
      assertEquals('1. Quartal 06', fmt.format(date));
      date = new Date(2006, 3, 27, 13, 10, 10, 250);
      assertEquals('2. Quartal 06', fmt.format(date));
      date = new Date(2006, 4, 27, 13, 10, 10, 250);
      assertEquals('2. Quartal 06', fmt.format(date));
      date = new Date(2006, 5, 27, 13, 10, 10, 250);
      assertEquals('2. Quartal 06', fmt.format(date));
      date = new Date(2006, 6, 27, 13, 10, 10, 250);
      assertEquals('3. Quartal 06', fmt.format(date));
      date = new Date(2006, 7, 27, 13, 10, 10, 250);
      assertEquals('3. Quartal 06', fmt.format(date));
      date = new Date(2006, 8, 27, 13, 10, 10, 250);
      assertEquals('3. Quartal 06', fmt.format(date));
      date = new Date(2006, 9, 27, 13, 10, 10, 250);
      assertEquals('4. Quartal 06', fmt.format(date));
      date = new Date(2006, 10, 27, 13, 10, 10, 250);
      assertEquals('4. Quartal 06', fmt.format(date));
      date = new Date(2006, 11, 27, 13, 10, 10, 250);
      assertEquals('4. Quartal 06', fmt.format(date));
    }
  },

  testQQyyyy() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
      let date = new Date(2006, 0, 27, 13, 10, 10, 250);
      const fmt = new DateTimeFormat('QQ yyyy');
      assertEquals('Q1 2006', fmt.format(date));
      date = new Date(2006, 1, 27, 13, 10, 10, 250);
      assertEquals('Q1 2006', fmt.format(date));
      date = new Date(2006, 2, 27, 13, 10, 10, 250);
      assertEquals('Q1 2006', fmt.format(date));
      date = new Date(2006, 3, 27, 13, 10, 10, 250);
      assertEquals('Q2 2006', fmt.format(date));
      date = new Date(2006, 4, 27, 13, 10, 10, 250);
      assertEquals('Q2 2006', fmt.format(date));
      date = new Date(2006, 5, 27, 13, 10, 10, 250);
      assertEquals('Q2 2006', fmt.format(date));
      date = new Date(2006, 6, 27, 13, 10, 10, 250);
      assertEquals('Q3 2006', fmt.format(date));
      date = new Date(2006, 7, 27, 13, 10, 10, 250);
      assertEquals('Q3 2006', fmt.format(date));
      date = new Date(2006, 8, 27, 13, 10, 10, 250);
      assertEquals('Q3 2006', fmt.format(date));
      date = new Date(2006, 9, 27, 13, 10, 10, 250);
      assertEquals('Q4 2006', fmt.format(date));
      date = new Date(2006, 10, 27, 13, 10, 10, 250);
      assertEquals('Q4 2006', fmt.format(date));
      date = new Date(2006, 11, 27, 13, 10, 10, 250);
      assertEquals('Q4 2006', fmt.format(date));
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMMddyyyyHHmmsszzz() {
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
      const fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zzz');
      assertEquals(
          '07/27/2006 13:10:10 ' + timezoneString(date), fmt.format(date));
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMMddyyyyHHmmssZ() {
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
      const fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss Z');

      assertEquals(
          '07/27/2006 13:10:10 ' + timezoneStringRFC(date), fmt.format(date));
    }
  },

  testPatternMonthDayMedium() {
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      const fmt = new DateTimeFormat(DateTimePatterns_de.MONTH_DAY_MEDIUM);
      assertEquals('27. Juli', fmt.format(date));
    }
  },

  testPatternYearMonthNarrow() {
    replacer.replace(goog, 'LOCALE', 'de');
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      let fmt = new DateTimeFormat(DateTimePatterns_de.YEAR_MONTH_SHORT);
      assertEquals('07.2006', fmt.format(date));
      fmt = new DateTimeFormat(DateTimePatterns_de.YEAR_MONTH_ABBR);
      assertEquals('Juli 2006', fmt.format(date));
    }
  },

  testPatternDayOfWeekMonthDayMedium() {
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);

    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog, 'LOCALE', 'en');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);
      let fmt = new DateTimeFormat(DateTimePatterns.WEEKDAY_MONTH_DAY_MEDIUM);
      assertEquals('Thu, Jul 27', fmt.format(date));

      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
      replacer.replace(goog, 'LOCALE', 'de');
      fmt = new DateTimeFormat(DateTimePatterns_de.WEEKDAY_MONTH_DAY_MEDIUM);
      assertEquals('Do., 27. Juli', fmt.format(date));
    }
  },

  testPatternDayOfWeekMonthDayYearMedium() {
    const date = new Date(2012, 5, 28, 13, 10, 10, 250);

    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog, 'LOCALE', 'en');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);
      let fmt =
          new DateTimeFormat(DateTimePatterns_en.WEEKDAY_MONTH_DAY_YEAR_MEDIUM);
      assertEquals('Thu, Jun 28, 2012', fmt.format(date));
      fmt = new DateTimeFormat(DateTimePatterns_en.MONTH_DAY_YEAR_MEDIUM);
      assertEquals('Jun 28, 2012', fmt.format(date));

      replacer.replace(goog, 'LOCALE', 'sv');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_sv);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_sv);
      fmt = new DateTimeFormat(DateTimePatterns.WEEKDAY_MONTH_DAY_YEAR_MEDIUM);
      assertEquals('tors, juni 28, 2012', fmt.format(date));
      fmt = new DateTimeFormat(DateTimePatterns.MONTH_DAY_YEAR_MEDIUM);
      assertEquals('juni 28, 2012', fmt.format(date));
    }
  },

  testMonthDayHourMinuteTimezone() {
    // For new data in CLDR 42.
    addI18nMapping(
        '[[Ĵûñ one] 28 one], [13:10 UTC-7 one two]',
        '[Ĵûñone]28,1:10[ÞṀone]UTC-7');

    addI18nMapping('6月28日下午1:10[UTC-7]', '6月28,1:10下午UTC-7');
    addI18nMapping('6月28日午夜12:00[UTC-7]', '6月28,12:00上午UTC-7');
    addI18nMapping('6月28日凌晨3:48[UTC-7]', '6月28,3:48上午UTC-7');

    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      // Include various locales.
      replacer.replace(goog, 'LOCALE', 'en-US');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);
      const date = new Date(2012, 5, 28, 13, 10, 10, 250);
      const fmt =
          new DateTimeFormat(DateTimePatterns.MONTH_DAY_TIME_ZONE_SHORT);
      assertI18nEquals('Jun 28, 1:10 PM UTC-7', fmt.format(date));

      replacer.replace(goog, 'LOCALE', 'sv');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_sv);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_sv);
      const fmtSv =
          new DateTimeFormat(DateTimePatterns_sv.MONTH_DAY_TIME_ZONE_SHORT);
      assertI18nEquals('28 juni 13:10 UTC-7', fmtSv.format(date));

      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_bg);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_bg);
      replacer.replace(goog, 'LOCALE', 'bg');
      const fmtBg =
          new DateTimeFormat(DateTimePatterns_bg.MONTH_DAY_TIME_ZONE_SHORT);
      assertI18nEquals('28.06, 13:10 ч. UTC-7', fmtBg.format(date));

      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_zh_HK);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_zh_HK);
      replacer.replace(goog, 'LOCALE', 'zh_HK');
      const fmtZhHk =
          new DateTimeFormat(DateTimePatterns_zh_HK.MONTH_DAY_TIME_ZONE_SHORT);
      assertI18nEquals('6月28日 下午1:10 [UTC-7]', fmtZhHk.format(date));

      // And with explicit timezone.
      const timeZone = TimeZone.createTimeZone(-600);
      assertI18nEquals(
          '6月29日 上午6:10 [UTC+10]', fmtZhHk.format(date, timeZone));

      if (!nativeMode) {
        // And some from the extended patterns. Not applicable in native mode.
        replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en_XA);
        replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en_XA);
        replacer.replace(goog, 'LOCALE', 'en_XA');

        const fmtEnXa =
            new DateTimeFormat(DateTimePatterns.MONTH_DAY_TIME_ZONE_SHORT);
        assertI18nEquals(
            'nativeMode=' + nativeMode,
            '[[Ĵûñ one] 28 one], [13:10 UTC-7 one two]', fmtEnXa.format(date));
      }

      replacer.replace(
          goog.i18n, 'DateTimePatterns', DateTimePatterns_zh_Hant_TW);
      replacer.replace(
          goog.i18n, 'DateTimeSymbols', DateTimeSymbols_zh_Hant_TW);
      replacer.replace(goog, 'LOCALE', 'zh_Hant_TW');

      // Set up for parts of the day in Chinese.
      setDayPeriods(DayPeriods_zh_Hant);

      const fmtZhHantTw =
          new DateTimeFormat(DateTimePatterns.MONTH_DAY_TIME_ZONE_SHORT);
      assertI18nEquals(
          'nativeMode = ' + nativeMode, '6月28日 下午1:10 [UTC-7]',
          fmtZhHantTw.format(date));
      const midnight = new Date(2012, 5, 28, 0, 0, 0);
      let result = fmtZhHantTw.format(midnight);
      assertI18nEquals(
          'nativeMode = ' + nativeMode, '6月28日 午夜12:00 [UTC-7]', result);

      const tooEarly = new Date(2012, 5, 28, 3, 48, 0);
      result = fmtZhHantTw.format(tooEarly);
      assertI18nEquals(
          'nativeMode = ' + nativeMode, '6月28日 凌晨3:48 [UTC-7]', result);
    }
  },

  testNightPeriodOverMidnight() {
    // Set up a variation on en with day periods including noon, midnight, and
    // "sleeping" time over midnight.
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);
    replacer.replace(goog, 'LOCALE', 'en');

    /**
     * Set up artificial 'night1' extending from 23:00 to 05:00 to test
     * handling of day periods including midnight.
     * {goog.i18n.DayPeriods}
     */
    const fakeDayPeriods_en = {
      midnight:
          {at: '00:00', formatNames: ['midnight'], periodName: 'midnight'},
      morning1: {
        from: '05:00',
        before: '08:00',
        formatNames: ['before breakfast'],
        periodName: 'morning1'
      },
      morning2: {
        from: '08:00',
        before: '12:00',
        formatNames: ['morning'],
        periodName: 'morning2'
      },
      noon: {at: '12:00', formatNames: ['noon'], periodName: 'noon'},
      afternoon1: {
        from: '12:00',
        before: '16:00',
        formatNames: ['early afternoon'],
        periodName: 'afternoon1'
      },
      afternoon2: {
        from: '16:00',
        before: '18:00',
        formatNames: ['dinner time'],
        periodName: 'afternoon2'
      },
      evening1: {
        from: '18:00',
        before: '23:00',
        formatNames: ['evening'],
        periodName: 'evening1'
      },
      night1: {
        from: '23:00',
        before: '05:00',
        formatNames: ['sleeping'],
        periodName: 'night1'
      },
    };
    setDayPeriods(fakeDayPeriods_en);

    const dateArray = [
      new Date(2022, 4, 24, 0, 0, 0), new Date(2022, 4, 24, 6, 30, 0),
      new Date(2022, 4, 24, 10, 30, 0), new Date(2022, 4, 24, 12, 0, 0),
      new Date(2022, 4, 24, 12, 1, 0), new Date(2022, 4, 24, 17, 0, 0),
      new Date(2022, 4, 24, 18, 37, 0), new Date(2022, 4, 24, 23, 17, 32),
      new Date(2022, 4, 24, 3, 17, 32)
    ];

    const expectedFlexPeriods = [
      'midnight', 'before breakfast', 'morning', 'noon', 'early afternoon',
      'dinner time', 'evening', 'sleeping', 'sleeping'
    ];
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const periodFormatter = new DateTimeFormat('B');

      for (let index = 0; index < dateArray.length; index++) {
        const result = periodFormatter.format(dateArray[index]);
        assertI18nEquals(
            'nativeMode =' + nativeMode + ' index=' + index,
            expectedFlexPeriods[index], result);
      }

      // Try with 'b' pattern
      const expectedBPeriods =
          ['midnight', 'AM', 'AM', 'noon', 'PM', 'PM', 'PM', 'PM', 'AM'];
      const bFormatter = new DateTimeFormat('b');

      for (let index = 0; index < dateArray.length; index++) {
        const result = bFormatter.format(dateArray[index]);
        assertI18nEquals('index=' + index, expectedBPeriods[index], result);
      }
    }
  },

  testNoFlexibleDayPeriodData() {
    // Check that fallback with no dayPeriod for 'b' and 'B' results in
    // AM/PM format.
    // The string pattern will force JavaScript mode only, not ECMAScript.
    // Note that this will fail if a de locale pattern adds 'B' or 'b'
    // as a format symbol.
    const midnight = new Date(2022, 4, 25, 0, 0, 0);
    const noon = new Date(2022, 4, 25, 12, 0, 0);
    const morning = new Date(2022, 4, 25, 9, 7, 17);
    const evening = new Date(2022, 4, 25, 18, 29, 0);

    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    setDayPeriods(null);

    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const fmtFlexible = new DateTimeFormat('HH B');
      const fmtNoonMidnight = new DateTimeFormat('HH b');

      let result;
      result = fmtFlexible.format(midnight);
      assertI18nEquals(result, '00 AM');
      result = fmtFlexible.format(noon);
      assertI18nEquals(result, '12 PM');
      result = fmtFlexible.format(morning);
      assertI18nEquals(result, '09 AM');
      result = fmtFlexible.format(evening);
      assertI18nEquals(result, '18 PM');

      result = fmtNoonMidnight.format(midnight);
      assertI18nEquals(result, '00 AM');
      result = fmtNoonMidnight.format(noon);
      assertI18nEquals(result, '12 PM');
      result = fmtNoonMidnight.format(morning);
      assertI18nEquals(result, '09 AM');
      result = fmtNoonMidnight.format(evening);
      assertI18nEquals(result, '18 PM');
    }
  },

  testQuote() {
    const date = new Date(2006, 6, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      let fmt = new DateTimeFormat('HH \'o\'\'clock\'');
      assertI18nEquals('13 o\'clock', fmt.format(date));
      fmt = new DateTimeFormat('HH \'oclock\'');
      assertI18nEquals('13 oclock', fmt.format(date));
      fmt = new DateTimeFormat('HH \'\'');
      assertI18nEquals('13 \'', fmt.format(date));
    }
  },

  testFractionalSeconds() {
    for (let nativeMode of testECMAScriptOptions) {
      let date = new Date(2006, 6, 27, 13, 10, 10, 256);
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      let fmt = new DateTimeFormat('s:S');
      assertI18nEquals('10:3', fmt.format(date));
      fmt = new DateTimeFormat('s:SS');
      assertI18nEquals('10:26', fmt.format(date));
      fmt = new DateTimeFormat('s:SSS');
      assertI18nEquals('10:256', fmt.format(date));
      fmt = new DateTimeFormat('s:SSSS');
      assertI18nEquals('10:2560', fmt.format(date));
      fmt = new DateTimeFormat('s:SSSSS');
      assertI18nEquals('10:25600', fmt.format(date));

      date = new Date(1960, 6, 27, 13, 10, 10, 256);
      fmt = new DateTimeFormat('s:S');
      assertI18nEquals('10:3', fmt.format(date));
      fmt = new DateTimeFormat('s:SS');
      assertI18nEquals('10:26', fmt.format(date));
      fmt = new DateTimeFormat('s:SSS');
      assertI18nEquals('10:256', fmt.format(date));
      fmt = new DateTimeFormat('s:SSSS');
      assertI18nEquals('10:2560', fmt.format(date));
      fmt = new DateTimeFormat('s:SSSSS');
      assertI18nEquals('10:25600', fmt.format(date));
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPredefinedFormatter() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');

      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
      const date = new Date(2006, 7, 4, 13, 49, 24, 0);

      let fmt = new DateTimeFormat(DateTimeFormat.Format.FULL_DATE);
      let result = removeLtrMarkers(fmt.format(date));
      assertI18nEquals('Freitag, 4. August 2006', result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.LONG_DATE);
      result = removeLtrMarkers(fmt.format(date));
      assertI18nEquals('4. August 2006', result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATE);
      result = removeLtrMarkers(fmt.format(date));
      assertI18nEquals('nativeMode = ' + nativeMode, '04.08.2006', result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      result = removeLtrMarkers(fmt.format(date));
      let expected = '04.08.06';
      assertI18nEquals(expected, result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.FULL_TIME);
      result = fmt.format(date);

      // This timezone result does not change with the type of time (FULL..SHORT).
      let timezoneResult = timezoneString(date);
      const theTime = '13:49:24';
      expected = theTime + ' ' + timezoneResult;

      if (nativeMode) {
        const expected2 = theTime + ' Nordamerikanische Westküsten-Sommerzeit';
        assertI18nEquals('nativeMode = ' + nativeMode, expected2, result);
      } else {
        assertI18nEquals(expected, result);
      }

      fmt = new DateTimeFormat(DateTimeFormat.Format.LONG_TIME);
      checkNativeResult(nativeMode, expected, fmt.format(date));

      fmt = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_TIME);
      result = removeLtrMarkers(fmt.format(date));
      assertI18nEquals('nativeMode = ' + nativeMode, theTime, result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_TIME);
      result = removeLtrMarkers(fmt.format(date));
      assertI18nEquals('nativeMode=' + nativeMode, '13:49', result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.FULL_DATETIME);
      result = removeDirectionMarkers(fmt.format(date));
      expected = nativeMode ?
          'Freitag, 4. August 2006 um 13:49:24 Nordamerikanische Westküsten-Sommerzeit' :
          'Freitag, 4. August 2006 um 13:49:24 UTC-7';
      checkNativeResult(nativeMode, expected, result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.LONG_DATETIME);
      expected = '4. August 2006 um 13:49:24 ' + timezoneResult;
      result = removeDirectionMarkers(fmt.format(date));
      checkNativeResult(nativeMode, expected, result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATETIME);
      expected = '04.08.2006, 13:49:24';
      assertI18nEquals('nativeMode=' + nativeMode, expected, fmt.format(date));

      fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATETIME);
      expected = '04.08.06, 13:49';
      assertI18nEquals('nativeMode=' + nativeMode, expected, fmt.format(date));
    }
  },

  testMMddyyyyHHmmssZSimpleTimeZone() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      const date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10));
      const timeZone = TimeZone.createTimeZone(480);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss Z');
      assertI18nEquals('07/27/2006 05:10:10 -0800', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZ');
      assertI18nEquals('07/27/2006 05:10:10 -0800', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZZ');
      assertI18nEquals('07/27/2006 05:10:10 -0800', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZZZ');

      const result = fmt.format(date, timeZone);
      let expected = '07/27/2006 05:10:10 GMT-08:00';
      assertI18nEquals('nativeMode=' + nativeMode, expected, result);
    }
  },

  testMMddyyyyHHmmssZCommonTimeZone() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      let date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10));
      const timeZone = TimeZone.createTimeZone(americaLosAngelesData);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss Z');
      assertI18nEquals('07/27/2006 06:10:10 -0700', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZ');
      assertI18nEquals('07/27/2006 06:10:10 -0700', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZZ');
      assertI18nEquals('07/27/2006 06:10:10 -0700', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZZZ');
      assertI18nEquals(
          '07/27/2006 06:10:10 GMT-07:00', fmt.format(date, timeZone));

      date = new Date(Date.UTC(2006, 1, 27, 13, 10, 10));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss Z');
      assertI18nEquals('02/27/2006 05:10:10 -0800', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZ');
      assertI18nEquals('02/27/2006 05:10:10 -0800', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZZ');
      assertI18nEquals('02/27/2006 05:10:10 -0800', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss ZZZZ');
      assertI18nEquals(
          '02/27/2006 05:10:10 GMT-08:00', fmt.format(date, timeZone));
    }
  },

  testMMddyyyyHHmmsszSimpleTimeZone() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      const date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10));
      const timeZone = TimeZone.createTimeZone(420);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss z');
      assertI18nEquals('07/27/2006 06:10:10 UTC-7', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zz');
      assertI18nEquals('07/27/2006 06:10:10 UTC-7', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zzz');
      assertI18nEquals('07/27/2006 06:10:10 UTC-7', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zzzz');
      assertI18nEquals('07/27/2006 06:10:10 UTC-7', fmt.format(date, timeZone));
    }
  },

  testMMddyyyyHHmmsszCommonTimeZone() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      let date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10));
      let timeZone = TimeZone.createTimeZone(americaLosAngelesData);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss z');
      assertI18nEquals('07/27/2006 06:10:10 PDT', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zz');
      assertI18nEquals('07/27/2006 06:10:10 PDT', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zzz');
      assertI18nEquals('07/27/2006 06:10:10 PDT', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zzzz');
      assertI18nEquals(
          '07/27/2006 06:10:10 Pacific Daylight Time',
          fmt.format(date, timeZone));
      date = new Date(Date.UTC(2006, 1, 27, 13, 10, 10));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss z');
      assertI18nEquals('02/27/2006 05:10:10 PST', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zz');
      assertI18nEquals('02/27/2006 05:10:10 PST', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zzz');
      assertI18nEquals('02/27/2006 05:10:10 PST', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zzzz');
      assertI18nEquals(
          '02/27/2006 05:10:10 Pacific Standard Time',
          fmt.format(date, timeZone));

      timeZone = TimeZone.createTimeZone(europeBerlinData);
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss z');
      assertI18nEquals('02/27/2006 14:10:10 MEZ', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss zzzz');
      assertI18nEquals(
          '02/27/2006 14:10:10 Mitteleurop\u00e4ische Zeit',
          fmt.format(date, timeZone));
    }
  },

  testMMddyyyyHHmmssvCommonTimeZone() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      const date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10));
      const timeZone = TimeZone.createTimeZone(americaLosAngelesData);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss v');
      assertI18nEquals(
          '07/27/2006 06:10:10 America/Los_Angeles',
          fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss vv');
      assertI18nEquals(
          '07/27/2006 06:10:10 America/Los_Angeles',
          fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss vvv');
      assertI18nEquals(
          '07/27/2006 06:10:10 America/Los_Angeles',
          fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss vvvv');
      assertI18nEquals(
          '07/27/2006 06:10:10 America/Los_Angeles',
          fmt.format(date, timeZone));
    }
  },

  testMMddyyyyHHmmssvSimpleTimeZone() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      const date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10));
      const timeZone = TimeZone.createTimeZone(420);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss v');
      assertI18nEquals(
          '07/27/2006 06:10:10 Etc/GMT+7', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss vv');
      assertI18nEquals(
          '07/27/2006 06:10:10 Etc/GMT+7', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss vvv');
      assertI18nEquals(
          '07/27/2006 06:10:10 Etc/GMT+7', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss vvvv');
      assertI18nEquals(
          '07/27/2006 06:10:10 Etc/GMT+7', fmt.format(date, timeZone));
    }
  },

  testMMddyyyyHHmmssVCommonTimeZone() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      const date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10));
      const timeZone = TimeZone.createTimeZone(americaLosAngelesData);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss V');
      assertI18nEquals(
          '07/27/2006 06:10:10 America/Los_Angeles',
          fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss VV');
      assertI18nEquals(
          '07/27/2006 06:10:10 America/Los_Angeles',
          fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss VVV');
      assertI18nEquals(
          '07/27/2006 06:10:10 Los Angeles Time', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss VVVV');
      assertI18nEquals(
          '07/27/2006 06:10:10 Los Angeles Time', fmt.format(date, timeZone));
    }
  },

  testMMddyyyyHHmmssVSimpleTimeZone() {
    replacer.replace(goog, 'LOCALE', 'de');
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const date = new Date(Date.UTC(2006, 6, 27, 13, 10, 10));
      const timeZone = TimeZone.createTimeZone(420);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss V');
      assertI18nEquals(
          '07/27/2006 06:10:10 Etc/GMT+7', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss VV');
      assertI18nEquals(
          '07/27/2006 06:10:10 Etc/GMT+7', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss VVV');
      assertI18nEquals(
          '07/27/2006 06:10:10 GMT-07:00', fmt.format(date, timeZone));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss VVVV');
      assertI18nEquals(
          '07/27/2006 06:10:10 GMT-07:00', fmt.format(date, timeZone));
    }
  },

  test_yyyyMMddG() {
    replacer.replace(goog, 'LOCALE', 'de');
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

    const date = new Date(Date.UTC(2006, 6, 27, 20, 10, 10));

    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      let timeZone = TimeZone.createTimeZone(420);
      let fmt = new DateTimeFormat('yyyy.MM.dd G \'at\' HH:mm:ss vvvv');
      assertI18nEquals(
          '2006.07.27 n. Chr. at 13:10:10 Etc/GMT+7',
          fmt.format(date, timeZone));

      timeZone = TimeZone.createTimeZone(americaLosAngelesData);
      fmt = new DateTimeFormat('yyyy.MM.dd G \'at\' HH:mm:ss vvvv');
      assertI18nEquals(
          '2006.07.27 n. Chr. at 13:10:10 America/Los_Angeles',
          fmt.format(date, timeZone));
    }
  },

  test_daylightTimeTransition() {
    replacer.replace(goog, 'LOCALE', 'de');
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

    // US PST transition to PDT on 2006/4/2/ 2:00am, jump to 2006/4/2 3:00am,
    // That's UTC time 2006/4/2 10:00am
    const timeZone = TimeZone.createTimeZone(americaLosAngelesData);
    let date = new Date(Date.UTC(2006, 4 - 1, 2, 9, 59, 0));
    let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss z');
    assertI18nEquals('04/02/2006 01:59:00 PST', fmt.format(date, timeZone));
    date = new Date(Date.UTC(2006, 4 - 1, 2, 10, 1, 0));
    fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss z');
    assertI18nEquals('04/02/2006 03:01:00 PDT', fmt.format(date, timeZone));
    date = new Date(Date.UTC(2006, 4 - 1, 2, 10, 0, 0));
    fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss z');
    assertI18nEquals('04/02/2006 03:00:00 PDT', fmt.format(date, timeZone));
  },

  test_timeDisplayOnDaylighTimeTransition() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);

      // US PST transition to PDT on 2006/4/2/ 2:00am, jump to 2006/4/2 3:00am,
      let date = new Date(Date.UTC(2006, 4 - 1, 2, 2, 30, 0));
      const timeZone = TimeZone.createTimeZone(0);
      let fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss Z');
      assertI18nEquals('04/02/2006 02:30:00 +0000', fmt.format(date, timeZone));

      // US PDT transition to PST on 2006/10/29/ 2:00am, jump back to PDT
      // 2006/4/2 1:00am,
      date = new Date(Date.UTC(2006, 10 - 1, 29, 1, 30, 0));
      fmt = new DateTimeFormat('MM/dd/yyyy HH:mm:ss Z');
      assertI18nEquals('10/29/2006 01:30:00 +0000', fmt.format(date, timeZone));
    }
  },

  testTimeDisplayOnDaylightTimeTransitionDayChange() {
    // NOTE: this test is a regression test only if the test browser has an OS
    // timezone of PST. While the test should still work in other timezones, it
    // does not serve as a regression test in them.
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog, 'LOCALE', 'de');

      // Time is 2015/11/01 3:00:01am after US PDT -> PST. 11:00:01am UTC.
      const date = new Date(Date.UTC(2015, 11 - 1, 1, 11, 0, 1));
      // Convert to GMT-12, across DST transition.
      // The date should also change, but does not change when subtracting 4
      // hours from PST/PDT due to the extra hour from switching DST.
      const timeZone = TimeZone.createTimeZone(12 * 60);
      const fmt = new DateTimeFormat('yyyy/MM/dd HH:mm:ss Z');
      // Regression test: this once returned 2015/11/01 instead.
      assertI18nEquals('2015/10/31 23:00:01 -1200', fmt.format(date, timeZone));
    }
  },

  test_nativeDigits_fa() {
    const date = new Date(2006, 7 - 1, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'fa');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_fa);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fa);

      const timeZone = TimeZone.createTimeZone(420);
      let fmt = new DateTimeFormat('y/MM/dd H:mm:ss٫SS');
      assertI18nEquals('۲۰۰۶/۰۷/۲۷ ۱۳:۱۰:۱۰٫۲۵', fmt.format(date));

      // Make sure standardized timezone formats don't use native digits
      fmt = new DateTimeFormat('Z');
      assertI18nEquals('-0700', fmt.format(date, timeZone));
    }
  },

  test_nativeDigits_ar() {
    const date = new Date(2006, 7 - 1, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'ar_EG');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_ar_EG);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ar_EG);

      const timeZone = TimeZone.createTimeZone(420);
      let fmt = new DateTimeFormat('y/MM/dd H:mm:ss٫SS');
      assertI18nEquals('٢٠٠٦/٠٧/٢٧ ١٣:١٠:١٠٫٢٥', fmt.format(date));

      fmt = new DateTimeFormat(11);
      let result = fmt.format(date);
      const hasAsciiDigit = /[0-9]/.test(result);
      assertFalse('No ASCII digits = ' + result, hasAsciiDigit);

      // Make sure standardized timezone formats don't use native digits
      fmt = new DateTimeFormat('Z');
      assertI18nEquals('-0700', fmt.format(date, timeZone));
    }
  },

  test_enforceAsciiDigits_ar() {
    const date = new Date(2006, 7 - 1, 27, 13, 10, 10, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      replacer.replace(goog, 'LOCALE', 'ar_EG');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_ar_EG);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ar_EG);

      DateTimeFormat.setEnforceAsciiDigits(true);
      const timeZone = TimeZone.createTimeZone(420);
      let fmt = new DateTimeFormat('y/MM/dd H:mm:ss٫SS');
      assertI18nEquals('2006/07/27 13:10:10٫25', fmt.format(date));

      fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATETIME);
      let result = fmt.format(date);
      const hasArabicDigit = /[\u0660-\u0669]/.test(result);
      assertFalse('Expect only ASCII = ' + result, hasArabicDigit);

      // Remove LTR, RTL markers
      result = result.replace(/\u200e/g, '').replace(/\u200f/g, '');

      // Make sure standardized timezone formats don't use native digits
      fmt = new DateTimeFormat('Z');
      assertI18nEquals('-0700', fmt.format(date, timeZone));

      // Check with another locale.
      replacer.replace(goog, 'LOCALE', 'my');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_my);
      fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATETIME);
      result = fmt.format(date);
      const hasMyanmarDigit = /[\u1040-\u1049]/.test(result);
      assertFalse('No Myamnar digit = ' + result, hasMyanmarDigit);
    }
  },

  // Making sure that the date-time combination is not a simple concatenation
  test_dateTimeConcatenation() {
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);
    replacer.replace(goog, 'LOCALE', 'en');
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      const date = new Date(Date.UTC(2006, 4 - 1, 2, 2, 30, 0));
      const timeZone = TimeZone.createTimeZone(americaLosAngelesData);

      let fmt = new DateTimeFormat(DateTimeFormat.Format.FULL_DATETIME);

      let result = fmt.format(date, timeZone);
      let expect =
          'Saturday, April 1, 2006 at 6:30:00 PM Pacific Standard Time';

      assertI18nEquals('nativeMode=' + nativeMode, expect, result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.LONG_DATETIME);
      result = fmt.format(date, timeZone);
      expect = 'April 1, 2006 at 6:30:00 PM PST';
      assertI18nEquals(expect, result);

      fmt = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATETIME);
      const expect1 = removeWhitespace('Apr 1, 2006, 6:30:00 PM');
      const expect2 = removeWhitespace('Apr 1, 2006 at 6:30:00 PM');
      result = removeWhitespace(removeLtrMarkers(fmt.format(date, timeZone)));

      assertTrue(
          'MEDIUM_DATETIME result = ' + result,
          (expect1 === result) || (expect2 === result));
      // let matched =
      //     /Apr 1, 2006(,| at)? 6:30:00\sPM/.test(result);  // Optional comma
      // assertTrue('MEDIUM_DATETIME result = ' + result,
      //            matched);

      fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATETIME);
      expect = '4/1/06, 6:30 PM';
      assertI18nEquals(
          'SHORT_DATETIME = ' + result, expect, fmt.format(date, timeZone));
    }
  },

  testNotUsingGlobalSymbols() {
    const date = new Date(2013, 10, 15);


    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog, 'LOCALE', 'fr');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_fr);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fr);
      const fmtFr = new DateTimeFormat(DateTimeFormat.Format.FULL_DATE);

      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
      const fmtDe = new DateTimeFormat(DateTimeFormat.Format.FULL_DATE);

      // The two formatters should return different results (French & German)
      // Remove LTR marks if present.
      const frResult = fmtFr.format(date).replace(/\u200c/g, '');
      assertI18nEquals('vendredi 15 novembre 2013', frResult);
      const deResult = fmtDe.format(date).replace(/\u200c/g, '');
      assertI18nEquals('Freitag, 15. November 2013', deResult);
    }
  },

  testConstructorSymbols() {
    const date = new Date(2013, 10, 15);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog, 'LOCALE', 'fr');
      const fmtFr = new DateTimeFormat(
          DateTimeFormat.Format.FULL_DATE, DateTimeSymbols_fr);

      replacer.replace(goog, 'LOCALE', 'de');
      const fmtDe = new DateTimeFormat(
          DateTimeFormat.Format.FULL_DATE, DateTimeSymbols_de);

      // The two formatters should return different results (French & German)
      // Remove LTR marks if present.
      let expected = '‎vendredi 15 novembre 2013'.replace(/\u200e/g, '');
      const frResult =
          fmtFr.format(date).replace(/\u200c/g, '').replace(/\u200e/g, '');
      assertI18nEquals(expected, frResult);
      expected = 'Freitag, 15. November 2013'.replace(/\u200e/g, '');
      const deResult =
          fmtDe.format(date).replace(/\u200c/g, '').replace(/\u200e/g, '');
      assertI18nEquals(expected, deResult);
    }
  },

  testQuotedPattern() {
    // Regression test for b/29990921.
    const date = new Date(2013, 10, 15);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);

      // Literal apostrophe
      let fmt = new DateTimeFormat('MMM \'\'yy');
      assertI18nEquals('Nov \'13', fmt.format(date));
      // Quoted text
      fmt = new DateTimeFormat('MMM dd\'th\' yyyy');
      assertI18nEquals('Nov 15th 2013', fmt.format(date));
      // Quoted text (only opening apostrophe)
      fmt = new DateTimeFormat('MMM dd\'th yyyy');
      assertI18nEquals('Nov 15th yyyy', fmt.format(date));
      // Quoted text with literal apostrophe
      fmt = new DateTimeFormat('MMM dd\'th\'\'\'');
      assertI18nEquals('Nov 15th\'', fmt.format(date));
      // Quoted text with literal apostrophe (only opening apostrophe)
      fmt = new DateTimeFormat('MMM dd\'th\'\'');
      assertI18nEquals('Nov 15th\'', fmt.format(date));
    }
  },

  testSupportForWeekInYear() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const date = new Date(2013, 1, 25);

      replacer.replace(goog, 'LOCALE', 'fr');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_fr);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fr);
      let fmt = new DateTimeFormat('\'week\' w');
      assertI18nEquals('week 9', fmt.format(date));
      fmt = new DateTimeFormat('\'week\' ww');
      assertI18nEquals('week 09', fmt.format(date));

      // Make sure it uses native digits when needed
      replacer.replace(goog, 'LOCALE', 'fa');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_fa);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fa);
      fmt = new DateTimeFormat('\'week\' w');
      assertI18nEquals('week ۹', fmt.format(date));
      fmt = new DateTimeFormat('\'week\' ww');
      assertI18nEquals('week ۰۹', fmt.format(date));
    }
  },

  testSupportYearOfWeek() {
    replacer.replace(goog, 'LOCALE', 'fr');
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const date = new Date(2005, 0, 2);

      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_fr);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fr);
      let fmt = new DateTimeFormat('YYYY');
      assertI18nEquals('2004', fmt.format(date));
      fmt = new DateTimeFormat('YY');
      assertI18nEquals('04', fmt.format(date));
    }
  },

  testSupportForYearAndEra() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog, 'LOCALE', 'en');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_en);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);
      const date = new Date(2013, 1, 25);
      let fmt = new DateTimeFormat(DateTimePatterns.YEAR_FULL_WITH_ERA);

      assertI18nEquals('2013 AD', fmt.format(date));

      date.setFullYear(213);
      assertI18nEquals('213 AD', fmt.format(date));

      date.setFullYear(11);
      assertI18nEquals('11 AD', fmt.format(date));

      date.setFullYear(-213);
      assertI18nEquals('213 BC', fmt.format(date));

      replacer.replace(goog, 'LOCALE', 'de');
      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_de);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_de);
      fmt = new DateTimeFormat(DateTimePatterns_de.YEAR_FULL_WITH_ERA);

      date.setFullYear(2013);
      assertI18nEquals('2013 n. Chr.', fmt.format(date));

      date.setFullYear(213);
      assertI18nEquals('213 n. Chr.', fmt.format(date));

      date.setFullYear(11);
      assertI18nEquals('11 n. Chr.', fmt.format(date));

      date.setFullYear(-213);
      assertI18nEquals('213 v. Chr.', fmt.format(date));

      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_ja);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ja);
      fmt = new DateTimeFormat(DateTimePatterns_ja.YEAR_FULL_WITH_ERA);

      date.setFullYear(2013);
      assertI18nEquals('西暦2013年', fmt.format(date));

      date.setFullYear(213);
      assertI18nEquals('西暦213年', fmt.format(date));

      date.setFullYear(11);
      assertI18nEquals('西暦11年', fmt.format(date));

      date.setFullYear(-213);
      assertI18nEquals('紀元前213年', fmt.format(date));
    }
  },

  // Expected results from ICU4J v51. One entry will change in v52.
  // These cover all combinations of FIRSTDAYOFWEEK / FIRSTWEEKCUTOFFDAY in use.
  testWeekInYearI18n() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_bn_BD);
      assertI18nEquals('bn_BD', '১১১১১২২', weekInYearFor7Days());

      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en_IE);
      assertI18nEquals('en_IE', '1111112', weekInYearFor7Days());

      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fr_DJ);
      assertI18nEquals('fr_DJ', '1111222', weekInYearFor7Days());

      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_he_IL);
      assertI18nEquals('he_IL', '1111122', weekInYearFor7Days());
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ar_SA);
      assertI18nEquals('ar_SA', '١١١١١٢٢', weekInYearFor7Days());
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ar_AE);
      assertI18nEquals('ar_AE', '1111222', weekInYearFor7Days());
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en_IN);
      assertI18nEquals('en_IN', '1111122', weekInYearFor7Days());
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en_GB);
      assertI18nEquals('en_GB', '1111112', weekInYearFor7Days());
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en_US);
      assertI18nEquals('en_US', '1111122', weekInYearFor7Days());
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ro_RO);
      assertI18nEquals('ro_RO', '1111112', weekInYearFor7Days());
    }
  },

  // Regression for b/11567443 (no method 'getHours' when formatting a
  // goog.date.Date)
  test_variousDateTypes() {
    replacer.replace(goog, 'LOCALE', 'fr');
    replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_fr);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fr);


    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      const fmt = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATETIME);

      const date = new Date(2006, 6, 27, 13, 10, 42, 250);
      let expected = '27 juil. 2006, 13:10:42';
      let alternate = '27 juil. 2006 à 13:10:42';
      let result = removeLtrMarkers(fmt.format(date));

      // Some browsers use the alternate form.
      if (nativeMode) {
        assertTrue(
            'Native 27 juil = ' + result,
            expected === result || alternate === result);
      } else {
        // Polyfill mode
        assertI18nEquals(expected, result);
      }

      const gdatetime = new DateTime(2006, 6, 27, 13, 10, 42, 250);
      expected = '27 juil. 2006, 13:10:42';
      alternate = '27 juil. 2006 à 13:10:42';
      result = removeLtrMarkers(fmt.format(gdatetime));

      if (nativeMode) {
        assertTrue(
            'gdatetime MEDIUM_DATETIME juil. result = ' + result,
            expected === result || alternate === result);
      } else {
        assertI18nEquals(expected, result);
      }

      const gdate = new DateDate(2006, 6, 27);
      const fmtDate = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATE);

      expected = '27 juil. 2006';
      alternate = removeDirectionMarkers('27/07/2006');
      result = removeDirectionMarkers(fmtDate.format(gdatetime));

      assertTrue(
          'Native mode = ' + nativeMode + ' gdate MEDIUM_DATE = ' + result,
          result === expected || result === alternate);

      if (!nativeMode) {
        try {
          fmt.format(gdate);
          fail('Should have thrown exception.');
        } catch (e) {
        }
      } else {
        // Native mode returns midnight without explicit time set.
        // Make better match to remove commas.
        const result = removeDirectionMarkers(fmt.format(gdate))
                           .replace(/[\u002c\u060c]/g, '');
        expected = '27 juil. 2006 0:00:00';
        const alternate = '27 juil. 2006 à 00:00:00';
        const alternate2 = '27 juil. 2006 à 0:00:00';
        const alternate3 = '27 juil. 2006 00:00:00';
        assertTrue(
            'Midnight result = ' + result,
            result === expected || result === alternate ||
                result === alternate2 || result === alternate3);
      }
    }
  },

  testExceptionWhenFormattingNull() {
    const fmt = new DateTimeFormat('M/d/y');
    try {
      fmt.format(null);
      fail('Should have thrown exception.');
    } catch (e) {
    }
  },

  testNativeModeEnglishStdPatterns() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(goog, 'LOCALE', 'en-US');
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      const fmt = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATETIME);

      assertEquals(10, DateTimeFormat.Format.MEDIUM_DATETIME);

      const date = new Date(2006, 6, 27, 13, 10, 42, 250);
      let result = fmt.format(date);
      let expected = 'Jul 27, 2006, 1:10:42 PM';
      result = removeLtrMarkers(result);
      // TODO: Fix whitespace match
      let matched =
          /Jul 27, 2006(,| at)? 1:10:42\sPM/.test(result);  // Optional comma
      assertTrue('Expected match = ' + result, matched);

      const gdatetime = new DateTime(2006, 6, 27, 13, 10, 42, 250);

      result = removeLtrMarkers(fmt.format(gdatetime));
      matched = /Jul 27, 2006(,| at)? 1:10:42 PM/.test(result);  // Optional comma
      if (matched) {
        assertTrue(matched);
      } else {
        assertI18nEquals('Jul 27, 2006, 1:10:42 PM', result);
      }

      const fmtDate = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATE);
      result = removeDirectionMarkers(fmtDate.format(gdatetime));
      expected = 'Jul 27, 2006';
      assertI18nEquals(expected, result);

      const fmt0 = new DateTimeFormat(DateTimeFormat.Format.FULL_DATE);
      result = removeDirectionMarkers(fmt0.format(gdatetime));
      assertI18nEquals('Thursday, July 27, 2006', result);

      const fmt1 = new DateTimeFormat(DateTimeFormat.Format.SHORT_TIME);
      result = removeDirectionMarkers(fmt1.format(gdatetime));
      assertI18nEquals('1:10 PM', result);

      const fmt2 = new DateTimeFormat(DateTimeFormat.Format.FULL_DATETIME);
      result = removeDirectionMarkers(fmt2.format(gdatetime));

      if (nativeMode) {
        expected = 'Thursday, July 27, 2006 at 1:10:42 PM Pacific Daylight Time';
      } else {
        expected = 'Thursday, July 27, 2006 at 1:10:42 PM UTC-7';
      }
      assertI18nEquals('nativeMode=' + nativeMode, expected, result);

      const fmt3 = new DateTimeFormat(DateTimeFormat.Format.LONG_DATE);
      result = removeDirectionMarkers(fmt3.format(gdatetime));
      assertI18nEquals('July 27, 2006', result);
    }
  },

  test_FrenchPatternStrings() {
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      replacer.replace(goog.i18n, 'DateTimePatterns', DateTimePatterns_fr);
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fr);
      replacer.replace(goog, 'LOCALE', 'fr');

      const fmt = new DateTimeFormat(DateTimePatterns_fr.YEAR_FULL);

      const date = new Date(2006, 6, 27, 13, 10, 42, 250);
      let result = fmt.format(date);
      assertI18nEquals('2006', result);

      const fmt1 = new DateTimeFormat(DateTimePatterns_fr.YEAR_FULL_WITH_ERA);
      result = fmt1.format(date);
      assertI18nEquals('2006 ap. J.-C.', result);

      const fmt2 = new DateTimeFormat(DateTimePatterns_fr.MONTH_DAY_SHORT);
      result = fmt2.format(date);
      assertI18nEquals('27/07', result);

      const fmt3 =
          new DateTimeFormat(DateTimePatterns_fr.MONTH_DAY_TIME_ZONE_SHORT);
      result = fmt3.format(date);
      assertI18nEquals('27 juil., 13:10 UTC-7', result);
    }
  },

  test_NativeModeWithUnsupportedLocale() {
    setNativeMode(true);
    if (goog.labs.userAgent.browser.isIE() ||
        goog.global.Intl == undefined ||
        goog.global.Intl.DateTimeFormat == undefined) {
      return;
    }

    // Sumerian will default to English.
    replacer.replace(goog, 'LOCALE', 'sux');

    const nativeSetting = getNativeMode();
    assert(nativeSetting);

    const fmt = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATETIME);

    const date = new Date(2006, 6, 27, 13, 10, 42, 250);
    let result = removeLtrMarkers(fmt.format(date)).replace(/2006,/g, '2006');

    // Safari includes 'at ' as part of the formatted result
    if (goog.labs.userAgent.browser.isSafari()) {
      addI18nMapping('Jul 27, 2006 1:10:42 PM', 'Jul 27, 2006 at 1:10:42 PM');
    }
    assertI18nEquals(
        'MEDIUM_DATETIME result = ', 'Jul 27, 2006 1:10:42 PM', result);

    const fmt1 = new DateTimeFormat(DateTimePatterns.YEAR_FULL);

    result = fmt1.format(date);
    assertI18nEquals('2006', result);
  },

  testNonAsciiDigitsNative() {
    const date = new Date(2006, 6, 27, 13, 10, 42, 250);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      // Arabic with ASCII
      replacer.replace(goog, 'LOCALE', 'ar');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ar);
      const ar = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      let expected = removeDirectionMarkers('27‏/7‏/2006');
      let result = removeDirectionMarkers(ar.format(date));
      assertI18nEquals('ar', expected, result);

      // Egyptian Arabic
      replacer.replace(goog, 'LOCALE', 'ar-EG');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ar_EG);
      const ar_EG = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      expected = '٢٧‏/٧‏/٢٠٠٦';
      let expected2 = '٢٧‏/٠٧‏/٢٠٠٦';  // Zero digit in month
      result = ar_EG.format(date);
      result = removeDirectionMarkers(result);
      assertTrue(
          'ar-EG',
          removeDirectionMarkers(expected) == removeDirectionMarkers(result) ||
              removeDirectionMarkers(expected2) ==
                  removeDirectionMarkers(result));

      // Bengali
      replacer.replace(goog, 'LOCALE', 'bn');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_bn);
      const bn = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      expected = '২৭/৭/০৬';
      result = bn.format(date);
      assertI18nEquals('bn', expected, result);

      replacer.replace(goog, 'LOCALE', 'bn_BD');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_bn);
      const bn_BD = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      result = bn_BD.format(date);
      assertI18nEquals('bn_BD', expected, result);

      // Persian / Farsi
      replacer.replace(goog, 'LOCALE', 'fa');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fa);
      const fa = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      expected =
          removeDirectionMarkers('۲۰۰۶/۷/۲۷');  // Different from Arabic digits
      result = removeDirectionMarkers(fa.format(date));
      assertI18nEquals('Farsi digits result = ' + result, expected, result);

      // Malayalam
      replacer.replace(goog, 'LOCALE', 'ml');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ml);
      const ml = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      expected = '27/7/06';
      result = ml.format(date);
      assertI18nEquals('Malayalam digits result = ' + result, expected, result);

      // Marathi
      replacer.replace(goog, 'LOCALE', 'mr');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_mr);
      const mr = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      expected = '२७/७/०६';
      result = mr.format(date);
      assertI18nEquals('Marathi digits result = ' + result, expected, result);

      // Myanmar
      replacer.replace(goog, 'LOCALE', 'my');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_my);
      const my = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      result = my.format(date);
      expected = '၂၇-၀၇-၀၆';
      let alternate = '၇/၂၇/၀၆';  // Chrome native gregorian
      let alternate2 = '၂၇/၇/၀၆';  // Chrome native gregorian
      assertTrue(
          'Myanmar digits = ' + result,
          expected === result || alternate === result || alternate2 == result);

      // Nepali
      replacer.replace(goog, 'LOCALE', 'ne');
      replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ne);
      const ne = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      result = ne.format(date);
      expected = '०६/७/२७';
      alternate = '७/२७/०६';  // Chrome native gregorian
      assertTrue(
          'Nepali digits = ' + result,
          expected === result || alternate == result);
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testWebMapsDirectionsCase() {
    // Verify that UTC and locale times are formatted for the correct timezone.

    replacer.replace(goog, 'LOCALE', 'en');

    const utcTime = UtcDateTime.fromIsoString('2010-01-01');
    const localeTime = new Date('2010-01-01');
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      let fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_TIME);
      let result = fmt.format(utcTime);
      assertI18nEquals('12:00 AM', result);

      // Formatter needs to have its timzone changed.
      // Change to non-UTC time
      result = fmt.format(localeTime);
      assertI18nEquals('4:00 PM', result);

      // Change back to UTC time
      result = fmt.format(utcTime);
      assertI18nEquals('12:00 AM', result);

      // Another change to non-UTC time
      result = fmt.format(localeTime);
      assertI18nEquals('4:00 PM', result);
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testShortDate() {
    const date = new Date(2012, 4, 8);
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_en);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);

      const fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE,
                                   DateTimeSymbols);
      let result = fmt.format(date);
      assertI18nEquals('5/8/12', result);

      const fmt2 = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATETIME,
                                   DateTimeSymbols);
      result = fmt2.format(date);
      assertI18nEquals('5/8/12, 12:00 AM', result);
    }
  },

  testNoLocaleSet() {
    const date = new Date(2012, 4, 8);
    replacer.replace(goog, 'LOCALE', '');
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
      assertTrue(fmt !== null);
      const result = fmt.format(date);
      assertI18nEquals('5/8/12', result);
    }
  },

  testWeekdayMonthDay_en() {
    const date = new Date(2022, 4, 9);
    replacer.replace(goog, 'LOCALE', '');
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const fmt =
          new DateTimeFormat(DateTimeFormat.Format.WEEKDAY_MONTH_DAY_FULL);
      assertTrue(fmt !== null);
      const result = fmt.format(date);
      const expected = 'Monday, May 9';
      assertI18nEquals('Native=' + nativeMode, expected, result);
    }
  },

  testWeekdayMonthDay_zhHantTw() {
    const date = new Date(2022, 4, 9);
    replacer.replace(goog, 'LOCALE', 'zh_Hant_TW');
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_zh_Hant_TW);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const fmt =
          new DateTimeFormat(DateTimeFormat.Format.WEEKDAY_MONTH_DAY_FULL);
      assertTrue(fmt !== null);
      const result = fmt.format(date);
      const expected = '5月9日 星期一';
      assertI18nEquals('Native=' + nativeMode, expected, result);
    }
  },

  testZhTwLocale() {
    // Test for b/208532468 round trip with zh_TW with flexible time periods
    const date = new Date(0, 0, 0, 17);
    replacer.replace(goog, 'LOCALE', 'zh_TW');
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_zh_TW);
    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_TIME);
      assertTrue(fmt !== null);
      const result = fmt.format(date);
      const expected = '下午5:00';
      assertI18nEquals('Native=' + nativeMode, expected, result);
    }
  },

  testZhTwFlexPeriodswithTimeStyleLong() {
    // Test for b/208532468 round trip with zh_TW with flexible time periods
    replacer.replace(goog, 'LOCALE', 'zh_Hant_TW');
    replacer.replace(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_zh_Hant_TW);
    setDayPeriods(DayPeriods_zh_Hant);

    const dateArray = [
      new Date(2022, 4, 24, 3, 4, 5),
      new Date(2022, 4, 24, 7, 30, 0),
      new Date(2022, 4, 24, 9, 17),
      new Date(2022, 4, 24, 12, 0, 0),
      new Date(2022, 4, 24, 13, 59, 0),
      new Date(2022, 4, 24, 17, 17, 0),
      new Date(2022, 4, 24, 21, 57),
      new Date(2022, 4, 24, 23, 1, 0),
      new Date(2022, 4, 24, 0, 0, 0),
    ];
    // Variations expected in brower versions
    const expectedLongTime = [
      ['凌晨3:04:05', '上午3:04:05'],
      ['清晨7:30:00', '上午7:30:00'],
      ['上午9:17:00'],
      ['中午12:00:00', '下午12:00:00'],
      ['下午1:59:00'],
      ['下午5:17:00'],
      ['晚上9:57:00', '下午9:57:00'],
      ['晚上11:01:00', '下午11:01:00'],
      ['午夜12:00:00', '上午12:00:00', '凌晨12:00:00'],
    ];

    for (let nativeMode of testECMAScriptOptions) {
      replacer.replace(
          LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
      const fmt = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_TIME);
      assertNotNullNorUndefined(fmt);

      for (let index = 0; index < expectedLongTime.length; index++) {
        const result = fmt.format(dateArray[index]);
        const expected = expectedLongTime[index];
        const timesMatch = expected.includes(result);
        assertTrue(
            'Native=' + nativeMode + ' For time ' + dateArray[index] +
                ' expected ' + expectedLongTime[index] + ' got ' + result,
            timesMatch);
      }
    }
  },
});
