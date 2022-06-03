/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.locale.countryLanguageNamesTest');
goog.setTestOnly();

const locale = goog.require('goog.locale');
const testSuite = goog.require('goog.testing.testSuite');

let LocaleNameConstants_en;

testSuite({
  setUpPage() {
    // Test data from
    // //googledata/i18n/js_locale_data/LocaleNameConstants__de.js
    const LocaleNameConstants_de = {
      LANGUAGE: {
        'cad': 'Caddo',
        'fr': 'Franz\u00f6sisch',
        'fr_CA': 'Canadian French',
        'fr_CH': 'Swiss French',
        'zh': 'Chinesisch',
        'zh_Hans': 'Chinesisch (vereinfacht)',
        'zh_Hant': 'Chinesisch (traditionell)',
      },
      COUNTRY: {'CN': 'China', 'ES': 'Spanien', 'FR': 'Frankreich'},
    };
    registerLocalNameConstants(LocaleNameConstants_de, 'de');

    // Test data from
    // //googledata/i18n/js_locale_data/LocaleNameConstants__en.js
    LocaleNameConstants_en = {
      LANGUAGE: {
        'cad': 'Caddo',
        'fr': 'French',
        'fr_CA': 'Canadian French',
        'fr_CH': 'Swiss French',
        'zh': 'Chinese',
        'zh_Hans': 'Simplified Chinese',
        'zh_Hant': 'Traditional Chinese',
      },
      COUNTRY: {'CN': 'China', 'ES': 'Spain', 'FR': 'France'},
    };
    registerLocalNameConstants(LocaleNameConstants_en, 'en');

    locale.setLocale('de');
  },

  testLoadLoacleSymbols() {
    const result = locale.getLocalizedCountryName('fr-FR');
    assertEquals('Frankreich', result);
  },

  testGetNativeCountryName() {
    let result = locale.getNativeCountryName('de-DE');
    assertEquals('Deutschland', result);

    result = locale.getNativeCountryName('de_DE');
    assertEquals('Deutschland', result);

    result = locale.getNativeCountryName('und');
    assertEquals('und', result);

    result = locale.getNativeCountryName('de-CH');
    assertEquals('Schweiz', result);

    result = locale.getNativeCountryName('fr-CH');
    assertEquals('Suisse', result);

    result = locale.getNativeCountryName('it-CH');
    assertEquals('Svizzera', result);
  },

  testGetLocalizedCountryName() {
    let result = locale.getLocalizedCountryName('es-ES');
    assertEquals('Spanien', result);

    result = locale.getLocalizedCountryName('es-ES', LocaleNameConstants_en);
    assertEquals('Spain', result);

    result = locale.getLocalizedCountryName('zh-CN-cmn');
    assertEquals('China', result);

    result = locale.getLocalizedCountryName('zh_CN_cmn');
    assertEquals('China', result);

    // 'und' is a non-existing locale, default behavior is to
    // return the locale name itself if no mapping is found.
    result = locale.getLocalizedCountryName('und');
    assertEquals('und', result);
  },

  testGetLocalizedRegionNameFromRegionCode() {
    let result = locale.getLocalizedRegionNameFromRegionCode('ES');
    assertEquals('Spanien', result);

    result = locale.getLocalizedRegionNameFromRegionCode(
        'ES', LocaleNameConstants_en);
    assertEquals('Spain', result);

    result = locale.getLocalizedRegionNameFromRegionCode('CN');
    assertEquals('China', result);

    // 'XX' is a non-existing country code, default behavior is to
    // return the code itself if no mapping is found.
    result = locale.getLocalizedRegionNameFromRegionCode('XX');
    assertEquals('XX', result);
  },

  testGetNativeLanguageName() {
    let result = locale.getNativeLanguageName('fr');
    assertEquals('fran\u00E7ais', result);

    result = locale.getNativeLanguageName('fr-latn-FR');
    assertEquals('fran\u00E7ais', result);

    result = locale.getNativeLanguageName('fr_FR');
    assertEquals('fran\u00E7ais', result);

    result = locale.getNativeLanguageName('error');
    assertEquals('error', result);
  },

  testGetLocalizedLanguageName() {
    let result = locale.getLocalizedLanguageName('fr');
    assertEquals('Franz\u00F6sisch', result);

    result = locale.getLocalizedLanguageName('fr', LocaleNameConstants_en);
    assertEquals('French', result);

    result = locale.getLocalizedLanguageName('fr-latn-FR');
    assertEquals('Franz\u00F6sisch', result);

    result = locale.getLocalizedLanguageName('fr_FR');
    assertEquals('Franz\u00F6sisch', result);

    result = locale.getLocalizedLanguageName('cad');
    assertEquals('Caddo', result);

    result = locale.getLocalizedLanguageName('error');
    assertEquals('error', result);

    result = locale.getLocalizedLanguageName('zh_Hans', LocaleNameConstants_en);
    assertEquals('Simplified Chinese', result);
  },

  testGetLocalizedLanguageNameForGivenSymbolset() {
    let result = locale.getLocalizedCountryName('fr-FR');
    assertEquals('Frankreich', result);

    result = locale.getLocalizedCountryName('fr-FR', LocaleNameConstants_en);
    assertEquals('France', result);

    result = locale.getLocalizedCountryName('fr-FR');
    assertEquals('Frankreich', result);
  },

  /**
   * Valid combination of sub tags:
   *  1)  LanguageSubtag'-'RegionSubtag
   *  2)  LanguageSubtag'-'ScriptSubtag'-'RegionSubtag
   *  3)  LanguageSubtag'-'RegionSubtag'-'VariantSubtag
   *  4)  LanguageSubtag'-'ScriptSubTag'-'RegionSubtag'-'VariantSubtag
   */
  testGetRegionSubTag() {
    let result = locale.getRegionSubTag('de-CH');
    assertEquals('CH', result);

    result = locale.getRegionSubTag('de-latn-CH');
    assertEquals('CH', result);

    result = locale.getRegionSubTag('de_latn_CH');
    assertEquals('CH', result);

    result = locale.getRegionSubTag('de-CH-xxx');
    assertEquals('CH', result);

    result = locale.getRegionSubTag('de-latn-CH-xxx');
    assertEquals('CH', result);

    result = locale.getRegionSubTag('es-latn-419-xxx');
    assertEquals('419', result);

    result = locale.getRegionSubTag('es_latn_419_xxx');
    assertEquals('419', result);

    // No region sub tag present
    result = locale.getRegionSubTag('de');
    assertEquals('', result);
  },

  testGetLanguageSubTag() {
    let result = locale.getLanguageSubTag('de');
    assertEquals('de', result);

    result = locale.getLanguageSubTag('de-DE');
    assertEquals('de', result);

    result = locale.getLanguageSubTag('de-latn-DE-xxx');
    assertEquals('de', result);

    result = locale.getLanguageSubTag('nds');
    assertEquals('nds', result);

    result = locale.getLanguageSubTag('nds-DE');
    assertEquals('nds', result);
  },

  testGetScriptSubTag() {
    let result = locale.getScriptSubTag('fr');
    assertEquals('', result);

    result = locale.getScriptSubTag('fr-Latn');
    assertEquals('Latn', result);

    result = locale.getScriptSubTag('fr-Arab-AA');
    assertEquals('Arab', result);

    result = locale.getScriptSubTag('de-Latin-DE');
    assertEquals('', result);

    result = locale.getScriptSubTag('srn-Ar-DE');
    assertEquals('', result);
  },
});
