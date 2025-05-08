/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.ListFormatTest');
goog.setTestOnly();

const ListSymbols = goog.require('goog.i18n.ListFormatSymbols');
const ListSymbolsExt = goog.require('goog.i18n.ListFormatSymbolsExt');
const LocaleFeature = goog.require('goog.i18n.LocaleFeature');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const testSuite = goog.require('goog.testing.testSuite');
const {ListFormat, ListFormatStyle, ListFormatType} = goog.require('goog.i18n.listFormat');


let propertyReplacer;

/* Set goog.i18n.LocaleFeature.USE_ECMASCRIPT_I18N_LISTFORMAT flag */
let testECMAScriptOptions = [];
if (Intl && Intl.ListFormat && Intl.ListFormat.supportedLocalesOf(['en'])) {
  testECMAScriptOptions.push(true);  // Test native mode first.
}
// For testing with Javascript data.
testECMAScriptOptions.push(false);

// List initialized at bottom of the file.
let nativeLocales;

/**
 * driveTests sets up each test with local, symbols, and native mode.
 * @param {string} locale
 * @param {!ListSymbols.ListFormatSymbols} symbols
 * @param {!Function} testCallbackFn
 */
function driveTests(locale, symbols, testCallbackFn) {
  ListSymbols.setListFormatSymbols(symbols);
  propertyReplacer.replace(goog, 'LOCALE', locale);
  assertTrue(goog.LOCALE == locale);

  const isSupportedNativeLocale = nativeLocales.includes(goog.LOCALE);

  for (let nativeMode of testECMAScriptOptions) {
    if (nativeMode && !isSupportedNativeLocale) {
      continue;
    }
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_LISTFORMAT', nativeMode);
    testCallbackFn(nativeMode);
  }
}

const vehiclesEn = ['Motorcycle', 'bus', 'car', 'submarine'];

const vehicles3 = ['Motorcycle', 'Bus', 'Car'];
const vehiclesUk = ['Мотоцикл', 'Автобус', 'Автомобіль', 'Літак'];
const vehiclesZh = ['摩托车', '公共汽车', '车'];


testSuite({
  setUpPage() {
    propertyReplacer = new PropertyReplacer();
  },

  tearDown() {},

  testEnOrAnd() {
    // Check AND / OR types for style LONG
    driveTests(
        'en', ListSymbols.ListFormatSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          const andExpected = [
            '', 'Motorcycle', 'Motorcycle and bus', 'Motorcycle, bus, and car',
            'Motorcycle, bus, car, and submarine'
          ];

          // Check both types together
          const listConj = new ListFormat();
          const listConjLong = new ListFormat({style: ListFormatStyle.LONG});
          const listDisj = new ListFormat({type: ListFormatType.OR});
          const listDisjLong = new ListFormat(
              {type: ListFormatType.OR, style: ListFormatStyle.LONG});
          const listDisjShort = new ListFormat(
              {type: ListFormatType.OR, style: ListFormatStyle.SHORT});
          const listDisjNarrow = new ListFormat(
              {type: ListFormatType.OR, style: ListFormatStyle.NARROW});
          for (let i = 0; i <= 4; i++) {
            let result = listConj.format(vehiclesEn.slice(0, i));
            assertEquals('Native=' + nativeMode, andExpected[i], result);

            result = listConjLong.format(vehiclesEn.slice(0, i));
            assertEquals('Native=' + nativeMode, andExpected[i], result);

            // 'or' vs 'and'
            result = listDisj.format(vehiclesEn.slice(0, i));
            let expected = andExpected[i].replace(' and ', ' or ');

            // All OR Styles are the same
            result = listDisjLong.format(vehiclesEn.slice(0, i));
            assertEquals('Native=' + nativeMode, expected, result);


            result = listDisjShort.format(vehiclesEn.slice(0, i));
            assertEquals('Native=' + nativeMode, expected, result);

            result = listDisjNarrow.format(vehiclesEn.slice(0, i));
            assertEquals('Native=' + nativeMode, expected, result);
          }
        });
  },

  testEnAndStyles() {
    // Test short and narrow styles for AND
    driveTests(
        'en', ListSymbols.ListFormatSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          const listConjShort = new ListFormat({style: ListFormatStyle.SHORT});
          const listConjNarrow = new ListFormat(
              {type: ListFormatType.AND, style: ListFormatStyle.NARROW});
          let result = listConjShort.format(vehiclesEn);
          let expected = 'Motorcycle, bus, car, & submarine';
          assertEquals('Native=' + nativeMode, expected, result);
          result = listConjNarrow.format(vehiclesEn);
          expected = 'Motorcycle, bus, car, submarine';
          assertEquals('Native=' + nativeMode, expected, result);
        });
  },

  testEnUnit() {
    driveTests(
        'en', ListSymbols.ListFormatSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Test UNIT type and style options
          const listUnitDefault = new ListFormat({type: ListFormatType.UNIT});
          const listUnitLong = new ListFormat(
              {type: ListFormatType.UNIT, style: ListFormatStyle.LONG});
          const listUnitShort = new ListFormat(
              {type: ListFormatType.UNIT, style: ListFormatStyle.SHORT});
          const listUnitNarrow = new ListFormat(
              {type: ListFormatType.UNIT, style: ListFormatStyle.NARROW});

          const unitExpected = new Map();
          unitExpected.set('Default', {
            'formatter': listUnitDefault,  // Same as LONG
            'expected': [
              'Motorcycle', 'Motorcycle, bus', 'Motorcycle, bus, car',
              'Motorcycle, bus, car, submarine'
            ]
          });
          unitExpected.set(ListFormatStyle.LONG, {
            'formatter': listUnitLong,
            'expected': [
              'Motorcycle', 'Motorcycle, bus', 'Motorcycle, bus, car',
              'Motorcycle, bus, car, submarine'
            ]
          });
          unitExpected.set(ListFormatStyle.SHORT, {
            'formatter': listUnitShort,
            'expected': [
              'Motorcycle', 'Motorcycle, bus', 'Motorcycle, bus, car',
              'Motorcycle, bus, car, submarine'
            ]
          });
          unitExpected.set(ListFormatStyle.NARROW, {
            'formatter': listUnitNarrow,
            'expected': [
              'Motorcycle', 'Motorcycle bus', 'Motorcycle bus car',
              'Motorcycle bus car submarine'
            ]
          });

          for (let [style, data] of unitExpected) {
            const fmt = data['formatter'];
            const expected = data['expected'];
            for (let index = 0; index < 4; index++) {
              const inlist = vehiclesEn.slice(0, index + 1);
              const result = fmt.format(inlist);
              assertEquals(
                  'Native=' + nativeMode + 'style=' + style, expected[index],
                  result);
            }
          }
        });
  },

  testUk() {
    // Ukranian language
    driveTests(
        'uk-UA', ListSymbolsExt.ListFormatSymbols_uk_UA,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          const andExpected = [
            '', 'Мотоцикл', 'Мотоцикл і Автобус',
            'Мотоцикл, Автобус і Автомобіль',
            'Мотоцикл, Автобус, Автомобіль і Літак'
          ];

          // Check both types together
          const listConj = new ListFormat();
          const listDisj = new ListFormat({type: ListFormatType.OR});
          for (let i = 0; i <= 4; i++) {
            let result = listConj.format(vehiclesUk.slice(0, i));
            assertEquals('Native=' + nativeMode, andExpected[i], result);

            result = listDisj.format(vehiclesUk.slice(0, i));
            let expected = andExpected[i].replace(' і ', ' або ');
            assertEquals('Native=' + nativeMode, expected, result);
          }
        });
  },

  testZhHans() {
    driveTests(
        'zh-Hans', ListSymbolsExt.ListFormatSymbols_zh_Hans,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Chinese, simplified script
          const listConj = new ListFormat();

          const expected1 = '摩托车、公共汽车和车';
          let result1 = listConj.format(vehiclesZh);
          assertEquals('Native=' + nativeMode, expected1, result1);

          const listDisj = new ListFormat({type: ListFormatType.OR});
          let result2 = listDisj.format(vehiclesZh);
          const expected2 = '摩托车、公共汽车或车';
          assertEquals('Native=' + nativeMode, expected2, result2);
        });
  },

  testArabic() {
    driveTests(
        'ar', ListSymbols.ListFormatSymbols_ar,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Standard Arabic
          const listConj = new ListFormat();
          let result3 = listConj.format(vehicles3);
          /* remove directional isolate U_2068 & U+2069 */
          result3 = result3.replaceAll('\u2068', '');
          result3 = result3.replaceAll('\u2069', '');
          assertEquals(
              'result3 Native=' + nativeMode, 'Motorcycle وBus وCar', result3);

          let result2 = listConj.format(vehicles3.slice(0, 2));
          result2 = result2.replaceAll('\u2068', '');
          result2 = result2.replaceAll('\u2069', '');
          assertEquals(
              'result2: Native=' + nativeMode, 'Motorcycle وBus', result2);
        });

  },

  testArabicOr() {
    driveTests(
        'ar', ListSymbols.ListFormatSymbols_ar,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          const listDisj = new ListFormat({type: ListFormatType.OR});
          let result3 = listDisj.format(vehicles3);
          let expectOr = 'Motorcycle أو Bus أو Car';
          // Remove Unicode directional isolates. Needed for
          // non-Arabic test in some browsers.
          result3 = result3.replaceAll('\u2068', '');
          result3 = result3.replaceAll('\u2069', '');
          assertEquals('Native=' + nativeMode, expectOr, result3);

          let result2 = listDisj.format(vehicles3.slice(0, 2));
          result2 = result2.replaceAll('\u2068', '');
          result2 = result2.replaceAll('\u2069', '');
          expectOr = 'Motorcycle أو Bus';
          assertEquals('Native=' + nativeMode, expectOr, result2);
        });
  },

  testMyanmarAnd() {
    driveTests(
        'my', ListSymbols.ListFormatSymbols_my,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Test Burmese language
          const listConj = new ListFormat();

          const result1 = listConj.format(vehicles3);

          // Note that Myanmar results differ somewhat between browsers.
          const matched = (result1 === 'Motorcycle Busနှင့် Car') ||
              (result1 === 'Motorcycle - Busနှင့် Car') ||
              (result1 === 'Motorcycle၊ Busနှင့် Car');
          assertTrue('Native=' + nativeMode + ': actual = ' + result1, matched);
        });
  },

  testMyanmarOr() {
    driveTests(
        'my', ListSymbols.ListFormatSymbols_my,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Test Burmese language
          const listDisj = new ListFormat({type: ListFormatType.OR});
          const result = listDisj.format(vehicles3);
          const matched = (result == 'Motorcycle၊ Bus သို့မဟုတ် Car') ||
              (result == 'Motorcycle Bus သို့မဟုတ် Car') ||
              (result == 'Motorcycle - Busသို့မဟုတ် Car') ||
              (result == 'Motorcycle - Bus သို့မဟုတ် Car');

          assertTrue('Native=' + nativeMode + ': actual = ' + result, matched);
        });
  },

  testMyanmarUnit() {
    driveTests(
        'my', ListSymbols.ListFormatSymbols_my,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Test Burmese language
          const listUnit = new ListFormat({type: ListFormatType.UNIT});
          const result = listUnit.format(vehicles3);
          const matched = (result == 'Motorcycle၊ Bus နှင့် Car') ||
              (result == 'Motorcycle Bus နှင့် Car') ||
              (result == 'Motorcycle- Busနှင့် Car') ||
              (result == 'Motorcycle - Bus နှင့် Car');

          assertTrue('Native=' + nativeMode + ': actual = ' + result, matched);
        });
  },
});


// All the locales correctly supported in all modern browsers.
nativeLocales = [
  'am',         'ar',         'ar-001',     'ar-AE',      'ar-BH',
  'ar-DJ',      'ar-DZ',      'ar-EG',      'ar-EH',      'ar-ER',
  'ar-IL',      'ar-IQ',      'ar-JO',      'ar-KM',      'ar-KW',
  'ar-LB',      'ar-LY',      'ar-MA',      'ar-MR',      'ar-OM',
  'ar-PS',      'ar-QA',      'ar-SA',      'ar-SD',      'ar-SO',
  'ar-SS',      'ar-SY',      'ar-TD',      'ar-TN',      'ar-YE',
  'bg',         'bg-BG',      'bn',         'bn-BD',      'bn-IN',
  'bs-Cyrl',    'bs-Cyrl-BA', 'ca',         'ca-AD',      'ca-ES',
  'ca-FR',      'ca-IT',      'cs',         'cs-CZ',      'da',
  'da-DK',      'da-GL',      'de',         'de-AT',      'de-BE',
  'de-CH',      'de-DE',      'de-IT',      'de-LI',      'de-LU',
  'el',         'el-CY',      'el-GR',      'en',         'en-001',
  'en-150',     'en-AE',      'en-AG',      'en-AI',      'en-AS',
  'en-AT',      'en-AU',      'en-BB',      'en-BE',      'en-BI',
  'en-BM',      'en-BS',      'en-BW',      'en-BZ',      'en-CA',
  'en-CC',      'en-CH',      'en-CK',      'en-CM',      'en-CX',
  'en-CY',      'en-DE',      'en-DG',      'en-DK',      'en-DM',
  'en-ER',      'en-FI',      'en-FJ',      'en-FK',      'en-FM',
  'en-GB',      'en-GD',      'en-GG',      'en-GH',      'en-GI',
  'en-GM',      'en-GU',      'en-GY',      'en-HK',      'en-IE',
  'en-IL',      'en-IM',      'en-IN',      'en-IO',      'en-JE',
  'en-JM',      'en-KE',      'en-KI',      'en-KN',      'en-KY',
  'en-LC',      'en-LR',      'en-LS',      'en-MG',      'en-MH',
  'en-MO',      'en-MP',      'en-MS',      'en-MT',      'en-MU',
  'en-MW',      'en-MY',      'en-NA',      'en-NF',      'en-NG',
  'en-NL',      'en-NR',      'en-NU',      'en-NZ',      'en-PG',
  'en-PH',      'en-PK',      'en-PN',      'en-PR',      'en-PW',
  'en-RW',      'en-SB',      'en-SC',      'en-SD',      'en-SE',
  'en-SG',      'en-SH',      'en-SI',      'en-SL',      'en-SS',
  'en-SX',      'en-SZ',      'en-TC',      'en-TK',      'en-TO',
  'en-TT',      'en-TV',      'en-TZ',      'en-UG',      'en-UM',
  'en-US',      'en-VC',      'en-VG',      'en-VI',      'en-VU',
  'en-WS',      'en-ZA',      'en-ZM',      'en-ZW',      'es',
  'es-419',     'es-AR',      'es-BO',      'es-BR',      'es-BZ',
  'es-CL',      'es-CO',      'es-CR',      'es-CU',      'es-DO',
  'es-EA',      'es-EC',      'es-ES',      'es-GQ',      'es-GT',
  'es-HN',      'es-IC',      'es-MX',      'es-NI',      'es-PA',
  'es-PE',      'es-PH',      'es-PR',      'es-PY',      'es-SV',
  'es-US',      'es-UY',      'es-VE',      'et',         'et-EE',
  'fa',         'fa-AF',      'fa-IR',      'fi',         'fi-FI',
  'fil',        'fil-PH',     'fr',         'fr-BE',      'fr-BF',
  'fr-BI',      'fr-BJ',      'fr-BL',      'fr-CA',      'fr-CD',
  'fr-CF',      'fr-CG',      'fr-CH',      'fr-CI',      'fr-CM',
  'fr-DJ',      'fr-DZ',      'fr-FR',      'fr-GA',      'fr-GF',
  'fr-GN',      'fr-GP',      'fr-GQ',      'fr-HT',      'fr-KM',
  'fr-LU',      'fr-MA',      'fr-MC',      'fr-MF',      'fr-MG',
  'fr-ML',      'fr-MQ',      'fr-MR',      'fr-MU',      'fr-NC',
  'fr-NE',      'fr-PF',      'fr-PM',      'fr-RE',      'fr-RW',
  'fr-SC',      'fr-SN',      'fr-SY',      'fr-TD',      'fr-TG',
  'fr-TN',      'fr-VU',      'fr-WF',      'fr-YT',      'gu',
  'gu-IN',      'he',         'he-IL',      'hi',         'hi-IN',
  'hr',         'hr-BA',      'hr-HR',      'hu',         'hu-HU',
  'id',         'id-ID',      'it',         'it-CH',      'it-IT',
  'it-SM',      'it-VA',      'ja',         'ja-JP',      'kn',
  'kn-IN',      'ko',         'ko-KP',      'ko-KR',      'lt',
  'lt-LT',      'lv',         'lv-LV',      'ml',         'ml-IN',
  'mr',         'mr-IN',      'ms',         'ms-BN',      'ms-ID',
  'ms-MY',      'ms-SG',      'nb',         'nl',         'nl-AW',
  'nl-BE',      'nl-BQ',      'nl-CW',      'nl-NL',      'nl-SR',
  'nl-SX',      'no',         'pl',         'pl-PL',      'pt',
  'pt-AO',      'pt-BR',      'pt-CH',      'pt-CV',      'pt-GQ',
  'pt-GW',      'pt-LU',      'pt-MO',      'pt-MZ',      'pt-PT',
  'pt-ST',      'pt-TL',      'ro',         'ro-MD',      'ro-RO',
  'ru',         'ru-BY',      'ru-KG',      'ru-KZ',      'ru-MD',
  'ru-RU',      'ru-UA',      'sk',         'sk-SK',      'sl',
  'sl-SI',      'sr',         'sr-Cyrl',    'sr-Cyrl-BA', 'sr-Cyrl-ME',
  'sr-Cyrl-RS', 'sr-Cyrl-XK', 'sr-Latn',    'sr-Latn-BA', 'sr-Latn-ME',
  'sr-Latn-RS', 'sr-Latn-XK', 'sv',         'sv-AX',      'sv-FI',
  'sv-SE',      'sw',         'sw-CD',      'sw-KE',      'sw-TZ',
  'sw-UG',      'ta',         'ta-IN',      'ta-LK',      'ta-MY',
  'ta-SG',      'te',         'te-IN',      'th',         'th-TH',
  'tr',         'tr-CY',      'tr-TR',      'uk',         'uk-UA',
  'vi',         'vi-VN',      'zh',         'zh-Hans',    'zh-Hans-CN',
  'zh-Hans-HK', 'zh-Hans-MO', 'zh-Hans-SG', 'zh-Hant',    'zh-Hant-HK',
  'zh-Hant-MO', 'zh-Hant-TW'
];
