/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @fileoverview Number formatting symbols.
 *
 * File generated from CLDR ver. 39
 *
 * To reduce the file size (which may cause issues in some JS
 * developing environments), this file will only contain locales
 * that are frequently used by web applications. This is defined as
 * proto/closure_locales_data.txt and will change (most likely addition)
 * over time.  Rest of the data can be found in another file named
 * "numberformatsymbolsext.js", which will be generated at
 * the same time together with this file.
 *
 * @suppress {const,useOfGoogProvide}
 */

// clang-format off

goog.provide('goog.i18n.NumberFormatSymbols');
goog.provide('goog.i18n.NumberFormatSymbols_af');
goog.provide('goog.i18n.NumberFormatSymbols_am');
goog.provide('goog.i18n.NumberFormatSymbols_ar');
goog.provide('goog.i18n.NumberFormatSymbols_ar_DZ');
goog.provide('goog.i18n.NumberFormatSymbols_ar_EG');
goog.provide('goog.i18n.NumberFormatSymbols_ar_EG_u_nu_latn');
goog.provide('goog.i18n.NumberFormatSymbols_az');
goog.provide('goog.i18n.NumberFormatSymbols_be');
goog.provide('goog.i18n.NumberFormatSymbols_bg');
goog.provide('goog.i18n.NumberFormatSymbols_bn');
goog.provide('goog.i18n.NumberFormatSymbols_bn_u_nu_latn');
goog.provide('goog.i18n.NumberFormatSymbols_br');
goog.provide('goog.i18n.NumberFormatSymbols_bs');
goog.provide('goog.i18n.NumberFormatSymbols_ca');
goog.provide('goog.i18n.NumberFormatSymbols_chr');
goog.provide('goog.i18n.NumberFormatSymbols_cs');
goog.provide('goog.i18n.NumberFormatSymbols_cy');
goog.provide('goog.i18n.NumberFormatSymbols_da');
goog.provide('goog.i18n.NumberFormatSymbols_de');
goog.provide('goog.i18n.NumberFormatSymbols_de_AT');
goog.provide('goog.i18n.NumberFormatSymbols_de_CH');
goog.provide('goog.i18n.NumberFormatSymbols_el');
goog.provide('goog.i18n.NumberFormatSymbols_en');
goog.provide('goog.i18n.NumberFormatSymbols_en_AU');
goog.provide('goog.i18n.NumberFormatSymbols_en_CA');
goog.provide('goog.i18n.NumberFormatSymbols_en_GB');
goog.provide('goog.i18n.NumberFormatSymbols_en_IE');
goog.provide('goog.i18n.NumberFormatSymbols_en_IN');
goog.provide('goog.i18n.NumberFormatSymbols_en_SG');
goog.provide('goog.i18n.NumberFormatSymbols_en_US');
goog.provide('goog.i18n.NumberFormatSymbols_en_ZA');
goog.provide('goog.i18n.NumberFormatSymbols_es');
goog.provide('goog.i18n.NumberFormatSymbols_es_419');
goog.provide('goog.i18n.NumberFormatSymbols_es_ES');
goog.provide('goog.i18n.NumberFormatSymbols_es_MX');
goog.provide('goog.i18n.NumberFormatSymbols_es_US');
goog.provide('goog.i18n.NumberFormatSymbols_et');
goog.provide('goog.i18n.NumberFormatSymbols_eu');
goog.provide('goog.i18n.NumberFormatSymbols_fa');
goog.provide('goog.i18n.NumberFormatSymbols_fa_u_nu_latn');
goog.provide('goog.i18n.NumberFormatSymbols_fi');
goog.provide('goog.i18n.NumberFormatSymbols_fil');
goog.provide('goog.i18n.NumberFormatSymbols_fr');
goog.provide('goog.i18n.NumberFormatSymbols_fr_CA');
goog.provide('goog.i18n.NumberFormatSymbols_ga');
goog.provide('goog.i18n.NumberFormatSymbols_gl');
goog.provide('goog.i18n.NumberFormatSymbols_gsw');
goog.provide('goog.i18n.NumberFormatSymbols_gu');
goog.provide('goog.i18n.NumberFormatSymbols_haw');
goog.provide('goog.i18n.NumberFormatSymbols_he');
goog.provide('goog.i18n.NumberFormatSymbols_hi');
goog.provide('goog.i18n.NumberFormatSymbols_hr');
goog.provide('goog.i18n.NumberFormatSymbols_hu');
goog.provide('goog.i18n.NumberFormatSymbols_hy');
goog.provide('goog.i18n.NumberFormatSymbols_id');
goog.provide('goog.i18n.NumberFormatSymbols_in');
goog.provide('goog.i18n.NumberFormatSymbols_is');
goog.provide('goog.i18n.NumberFormatSymbols_it');
goog.provide('goog.i18n.NumberFormatSymbols_iw');
goog.provide('goog.i18n.NumberFormatSymbols_ja');
goog.provide('goog.i18n.NumberFormatSymbols_ka');
goog.provide('goog.i18n.NumberFormatSymbols_kk');
goog.provide('goog.i18n.NumberFormatSymbols_km');
goog.provide('goog.i18n.NumberFormatSymbols_kn');
goog.provide('goog.i18n.NumberFormatSymbols_ko');
goog.provide('goog.i18n.NumberFormatSymbols_ky');
goog.provide('goog.i18n.NumberFormatSymbols_ln');
goog.provide('goog.i18n.NumberFormatSymbols_lo');
goog.provide('goog.i18n.NumberFormatSymbols_lt');
goog.provide('goog.i18n.NumberFormatSymbols_lv');
goog.provide('goog.i18n.NumberFormatSymbols_mk');
goog.provide('goog.i18n.NumberFormatSymbols_ml');
goog.provide('goog.i18n.NumberFormatSymbols_mn');
goog.provide('goog.i18n.NumberFormatSymbols_mo');
goog.provide('goog.i18n.NumberFormatSymbols_mr');
goog.provide('goog.i18n.NumberFormatSymbols_mr_u_nu_latn');
goog.provide('goog.i18n.NumberFormatSymbols_ms');
goog.provide('goog.i18n.NumberFormatSymbols_mt');
goog.provide('goog.i18n.NumberFormatSymbols_my');
goog.provide('goog.i18n.NumberFormatSymbols_my_u_nu_latn');
goog.provide('goog.i18n.NumberFormatSymbols_nb');
goog.provide('goog.i18n.NumberFormatSymbols_ne');
goog.provide('goog.i18n.NumberFormatSymbols_ne_u_nu_latn');
goog.provide('goog.i18n.NumberFormatSymbols_nl');
goog.provide('goog.i18n.NumberFormatSymbols_no');
goog.provide('goog.i18n.NumberFormatSymbols_no_NO');
goog.provide('goog.i18n.NumberFormatSymbols_or');
goog.provide('goog.i18n.NumberFormatSymbols_pa');
goog.provide('goog.i18n.NumberFormatSymbols_pl');
goog.provide('goog.i18n.NumberFormatSymbols_pt');
goog.provide('goog.i18n.NumberFormatSymbols_pt_BR');
goog.provide('goog.i18n.NumberFormatSymbols_pt_PT');
goog.provide('goog.i18n.NumberFormatSymbols_ro');
goog.provide('goog.i18n.NumberFormatSymbols_ru');
goog.provide('goog.i18n.NumberFormatSymbols_sh');
goog.provide('goog.i18n.NumberFormatSymbols_si');
goog.provide('goog.i18n.NumberFormatSymbols_sk');
goog.provide('goog.i18n.NumberFormatSymbols_sl');
goog.provide('goog.i18n.NumberFormatSymbols_sq');
goog.provide('goog.i18n.NumberFormatSymbols_sr');
goog.provide('goog.i18n.NumberFormatSymbols_sr_Latn');
goog.provide('goog.i18n.NumberFormatSymbols_sv');
goog.provide('goog.i18n.NumberFormatSymbols_sw');
goog.provide('goog.i18n.NumberFormatSymbols_ta');
goog.provide('goog.i18n.NumberFormatSymbols_te');
goog.provide('goog.i18n.NumberFormatSymbols_th');
goog.provide('goog.i18n.NumberFormatSymbols_tl');
goog.provide('goog.i18n.NumberFormatSymbols_tr');
goog.provide('goog.i18n.NumberFormatSymbols_u_nu_latn');
goog.provide('goog.i18n.NumberFormatSymbols_uk');
goog.provide('goog.i18n.NumberFormatSymbols_ur');
goog.provide('goog.i18n.NumberFormatSymbols_uz');
goog.provide('goog.i18n.NumberFormatSymbols_vi');
goog.provide('goog.i18n.NumberFormatSymbols_zh');
goog.provide('goog.i18n.NumberFormatSymbols_zh_CN');
goog.provide('goog.i18n.NumberFormatSymbols_zh_HK');
goog.provide('goog.i18n.NumberFormatSymbols_zh_TW');
goog.provide('goog.i18n.NumberFormatSymbols_zu');

goog.requireType('goog.i18n.NumberFormatSymbolsType');


/**
 * Number formatting symbols for locale af.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_af = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'ZAR'
};


/**
 * Number formatting symbols for locale am.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_am = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'ETB'
};


/**
 * Number formatting symbols for locale ar.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ar = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '‎%‎',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '‎+',
  MINUS_SIGN: '‎-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'ليس رقمًا',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'EGP'
};


/**
 * Number formatting symbols for locale ar_DZ.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ar_DZ = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '‎%‎',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '‎+',
  MINUS_SIGN: '‎-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'ليس رقمًا',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'DZD'
};


/**
 * Number formatting symbols for locale ar_EG.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ar_EG = {
  DECIMAL_SEP: '٫',
  GROUP_SEP: '٬',
  PERCENT: '٪؜',
  ZERO_DIGIT: '٠',
  PLUS_SIGN: '؜+',
  MINUS_SIGN: '؜-',
  EXP_SYMBOL: 'اس',
  PERMILL: '؉',
  INFINITY: '∞',
  NAN: 'ليس رقم',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EGP'
};


/**
 * Number formatting symbols for locale ar_EG_u_nu_latn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ar_EG_u_nu_latn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '‎%‎',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '‎+',
  MINUS_SIGN: '‎-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'ليس رقمًا',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'EGP'
};


/**
 * Number formatting symbols for locale az.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_az = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'AZN'
};


/**
 * Number formatting symbols for locale be.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_be = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'BYN'
};


/**
 * Number formatting symbols for locale bg.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_bg = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '0.00 ¤',
  DEF_CURRENCY_CODE: 'BGN'
};


/**
 * Number formatting symbols for locale bn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_bn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '০',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##,##0.00¤',
  DEF_CURRENCY_CODE: 'BDT'
};


/**
 * Number formatting symbols for locale bn_u_nu_latn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_bn_u_nu_latn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##,##0%',
  CURRENCY_PATTERN: '#,##,##0.00¤',
  DEF_CURRENCY_CODE: 'BDT'
};


/**
 * Number formatting symbols for locale br.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_br = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale bs.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_bs = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'BAM'
};


/**
 * Number formatting symbols for locale ca.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ca = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale chr.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_chr = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'USD'
};


/**
 * Number formatting symbols for locale cs.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_cs = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'CZK'
};


/**
 * Number formatting symbols for locale cy.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_cy = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'GBP'
};


/**
 * Number formatting symbols for locale da.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_da = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'DKK'
};


/**
 * Number formatting symbols for locale de.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_de = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale de_AT.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_de_AT = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale de_CH.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_de_CH = {
  DECIMAL_SEP: '.',
  GROUP_SEP: '’',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00;¤-#,##0.00',
  DEF_CURRENCY_CODE: 'CHF'
};


/**
 * Number formatting symbols for locale el.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_el = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'e',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale en.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'USD'
};


/**
 * Number formatting symbols for locale en_AU.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en_AU = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'e',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'AUD'
};


/**
 * Number formatting symbols for locale en_CA.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en_CA = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'e',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'CAD'
};


/**
 * Number formatting symbols for locale en_GB.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en_GB = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'GBP'
};


/**
 * Number formatting symbols for locale en_IE.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en_IE = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale en_IN.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en_IN = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##,##0%',
  CURRENCY_PATTERN: '¤#,##,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale en_SG.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en_SG = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'SGD'
};


/**
 * Number formatting symbols for locale en_US.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en_US = goog.i18n.NumberFormatSymbols_en;


/**
 * Number formatting symbols for locale en_ZA.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_en_ZA = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'ZAR'
};


/**
 * Number formatting symbols for locale es.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_es = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale es_419.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_es_419 = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'MXN'
};


/**
 * Number formatting symbols for locale es_ES.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_es_ES = goog.i18n.NumberFormatSymbols_es;


/**
 * Number formatting symbols for locale es_MX.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_es_MX = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'MXN'
};


/**
 * Number formatting symbols for locale es_US.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_es_US = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'USD'
};


/**
 * Number formatting symbols for locale et.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_et = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: '×10^',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale eu.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_eu = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '% #,##0',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale fa.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_fa = {
  DECIMAL_SEP: '٫',
  GROUP_SEP: '٬',
  PERCENT: '٪',
  ZERO_DIGIT: '۰',
  PLUS_SIGN: '‎+',
  MINUS_SIGN: '‎−',
  EXP_SYMBOL: '×۱۰^',
  PERMILL: '؉',
  INFINITY: '∞',
  NAN: 'ناعدد',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '‎¤#,##0.00',
  DEF_CURRENCY_CODE: 'IRR'
};


/**
 * Number formatting symbols for locale fa_u_nu_latn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_fa_u_nu_latn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '‎+',
  MINUS_SIGN: '‎−',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'ناعدد',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '‎¤ #,##0.00',
  DEF_CURRENCY_CODE: 'IRR'
};


/**
 * Number formatting symbols for locale fi.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_fi = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'epäluku',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale fil.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_fil = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'PHP'
};


/**
 * Number formatting symbols for locale fr.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_fr = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale fr_CA.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_fr_CA = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'CAD'
};


/**
 * Number formatting symbols for locale ga.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ga = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'Nuimh',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale gl.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_gl = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale gsw.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_gsw = {
  DECIMAL_SEP: '.',
  GROUP_SEP: '’',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'CHF'
};


/**
 * Number formatting symbols for locale gu.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_gu = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '[#E0]',
  PERCENT_PATTERN: '#,##,##0%',
  CURRENCY_PATTERN: '¤#,##,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale haw.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_haw = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'USD'
};


/**
 * Number formatting symbols for locale he.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_he = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '‎+',
  MINUS_SIGN: '‎-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '‏#,##0.00 ¤;‏-#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'ILS'
};


/**
 * Number formatting symbols for locale hi.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_hi = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '[#E0]',
  PERCENT_PATTERN: '#,##,##0%',
  CURRENCY_PATTERN: '¤#,##,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale hr.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_hr = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'HRK'
};


/**
 * Number formatting symbols for locale hu.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_hu = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'HUF'
};


/**
 * Number formatting symbols for locale hy.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_hy = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'ՈչԹ',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'AMD'
};


/**
 * Number formatting symbols for locale id.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_id = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'IDR'
};


/**
 * Number formatting symbols for locale in.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_in = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'IDR'
};


/**
 * Number formatting symbols for locale is.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_is = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'ISK'
};


/**
 * Number formatting symbols for locale it.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_it = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale iw.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_iw = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '‎+',
  MINUS_SIGN: '‎-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '‏#,##0.00 ¤;‏-#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'ILS'
};


/**
 * Number formatting symbols for locale ja.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ja = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'JPY'
};


/**
 * Number formatting symbols for locale ka.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ka = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'არ არის რიცხვი',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'GEL'
};


/**
 * Number formatting symbols for locale kk.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_kk = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'сан емес',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'KZT'
};


/**
 * Number formatting symbols for locale km.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_km = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00¤',
  DEF_CURRENCY_CODE: 'KHR'
};


/**
 * Number formatting symbols for locale kn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_kn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale ko.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ko = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'KRW'
};


/**
 * Number formatting symbols for locale ky.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ky = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'сан эмес',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'KGS'
};


/**
 * Number formatting symbols for locale ln.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ln = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'CDF'
};


/**
 * Number formatting symbols for locale lo.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_lo = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'ບໍ່​ແມ່ນ​ໂຕ​ເລກ',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00;¤-#,##0.00',
  DEF_CURRENCY_CODE: 'LAK'
};


/**
 * Number formatting symbols for locale lt.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_lt = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: '×10^',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale lv.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_lv = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NS',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale mk.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_mk = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'MKD'
};


/**
 * Number formatting symbols for locale ml.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ml = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale mn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_mn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'MNT'
};


/**
 * Number formatting symbols for locale mo.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_mo = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'MDL'
};


/**
 * Number formatting symbols for locale mr.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_mr = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '०',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '[#E0]',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale mr_u_nu_latn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_mr_u_nu_latn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '[#E0]',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale ms.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ms = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'MYR'
};


/**
 * Number formatting symbols for locale mt.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_mt = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale my.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_my = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '၀',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'ဂဏန်းမဟုတ်သော',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'MMK'
};


/**
 * Number formatting symbols for locale my_u_nu_latn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_my_u_nu_latn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'ဂဏန်းမဟုတ်သော',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'MMK'
};


/**
 * Number formatting symbols for locale nb.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_nb = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'NOK'
};


/**
 * Number formatting symbols for locale ne.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ne = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '०',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##,##0%',
  CURRENCY_PATTERN: '¤ #,##,##0.00',
  DEF_CURRENCY_CODE: 'NPR'
};


/**
 * Number formatting symbols for locale ne_u_nu_latn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ne_u_nu_latn = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##,##0%',
  CURRENCY_PATTERN: '¤ #,##,##0.00',
  DEF_CURRENCY_CODE: 'NPR'
};


/**
 * Number formatting symbols for locale nl.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_nl = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00;¤ -#,##0.00',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale no.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_no = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'NOK'
};


/**
 * Number formatting symbols for locale no_NO.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_no_NO = goog.i18n.NumberFormatSymbols_no;


/**
 * Number formatting symbols for locale or.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_or = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale pa.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_pa = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '[#E0]',
  PERCENT_PATTERN: '#,##,##0%',
  CURRENCY_PATTERN: '¤ #,##,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale pl.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_pl = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'PLN'
};


/**
 * Number formatting symbols for locale pt.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_pt = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'BRL'
};


/**
 * Number formatting symbols for locale pt_BR.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_pt_BR = goog.i18n.NumberFormatSymbols_pt;


/**
 * Number formatting symbols for locale pt_PT.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_pt_PT = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale ro.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ro = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'RON'
};


/**
 * Number formatting symbols for locale ru.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ru = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'не число',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'RUB'
};


/**
 * Number formatting symbols for locale sh.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_sh = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'RSD'
};


/**
 * Number formatting symbols for locale si.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_si = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'LKR'
};


/**
 * Number formatting symbols for locale sk.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_sk = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'e',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale sl.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_sl = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: 'e',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'EUR'
};


/**
 * Number formatting symbols for locale sq.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_sq = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'ALL'
};


/**
 * Number formatting symbols for locale sr.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_sr = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'RSD'
};


/**
 * Number formatting symbols for locale sr_Latn.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_sr_Latn = goog.i18n.NumberFormatSymbols_sr;


/**
 * Number formatting symbols for locale sv.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_sv = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '−',
  EXP_SYMBOL: '×10^',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0 %',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'SEK'
};


/**
 * Number formatting symbols for locale sw.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_sw = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'TZS'
};


/**
 * Number formatting symbols for locale ta.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ta = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##,##0%',
  CURRENCY_PATTERN: '¤ #,##,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale te.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_te = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##,##0.00',
  DEF_CURRENCY_CODE: 'INR'
};


/**
 * Number formatting symbols for locale th.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_th = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'THB'
};


/**
 * Number formatting symbols for locale tl.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_tl = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'PHP'
};


/**
 * Number formatting symbols for locale tr.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_tr = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '%#,##0',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'TRY'
};


/**
 * Number formatting symbols for locale uk.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_uk = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'Е',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'UAH'
};


/**
 * Number formatting symbols for locale ur.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_ur = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '‎+',
  MINUS_SIGN: '‎-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤ #,##0.00',
  DEF_CURRENCY_CODE: 'PKR'
};


/**
 * Number formatting symbols for locale uz.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_uz = {
  DECIMAL_SEP: ',',
  GROUP_SEP: ' ',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'son emas',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'UZS'
};


/**
 * Number formatting symbols for locale vi.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_vi = {
  DECIMAL_SEP: ',',
  GROUP_SEP: '.',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '#,##0.00 ¤',
  DEF_CURRENCY_CODE: 'VND'
};


/**
 * Number formatting symbols for locale zh.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_zh = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'CNY'
};


/**
 * Number formatting symbols for locale zh_CN.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_zh_CN = goog.i18n.NumberFormatSymbols_zh;


/**
 * Number formatting symbols for locale zh_HK.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_zh_HK = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: '非數值',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'HKD'
};


/**
 * Number formatting symbols for locale zh_TW.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_zh_TW = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: '非數值',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'TWD'
};


/**
 * Number formatting symbols for locale zu.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols_zu = {
  DECIMAL_SEP: '.',
  GROUP_SEP: ',',
  PERCENT: '%',
  ZERO_DIGIT: '0',
  PLUS_SIGN: '+',
  MINUS_SIGN: '-',
  EXP_SYMBOL: 'E',
  PERMILL: '‰',
  INFINITY: '∞',
  NAN: 'NaN',
  DECIMAL_PATTERN: '#,##0.###',
  SCIENTIFIC_PATTERN: '#E0',
  PERCENT_PATTERN: '#,##0%',
  CURRENCY_PATTERN: '¤#,##0.00',
  DEF_CURRENCY_CODE: 'ZAR'
};


/**
 * Selected number formatting symbols by locale.
 * @const {!goog.i18n.NumberFormatSymbolsType.Type}
 */
goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en;
goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en;

switch (goog.LOCALE) {
  case 'af':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_af;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_af;
    break;
  case 'am':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_am;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_am;
    break;
  case 'ar':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ar;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ar;
    break;
  case 'ar_DZ':
  case 'ar-DZ':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ar_DZ;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ar_DZ;
    break;
  case 'ar_EG':
  case 'ar-EG':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ar_EG;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ar_EG_u_nu_latn;
    break;
  case 'az':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_az;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_az;
    break;
  case 'be':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_be;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_be;
    break;
  case 'bg':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_bg;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_bg;
    break;
  case 'bn':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_bn;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_bn_u_nu_latn;
    break;
  case 'br':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_br;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_br;
    break;
  case 'bs':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_bs;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_bs;
    break;
  case 'ca':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ca;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ca;
    break;
  case 'chr':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_chr;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_chr;
    break;
  case 'cs':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_cs;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_cs;
    break;
  case 'cy':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_cy;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_cy;
    break;
  case 'da':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_da;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_da;
    break;
  case 'de':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_de;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_de;
    break;
  case 'de_AT':
  case 'de-AT':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_de_AT;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_de_AT;
    break;
  case 'de_CH':
  case 'de-CH':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_de_CH;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_de_CH;
    break;
  case 'el':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_el;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_el;
    break;
  case 'en':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en;
    break;
  case 'en_AU':
  case 'en-AU':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en_AU;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en_AU;
    break;
  case 'en_CA':
  case 'en-CA':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en_CA;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en_CA;
    break;
  case 'en_GB':
  case 'en-GB':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en_GB;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en_GB;
    break;
  case 'en_IE':
  case 'en-IE':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en_IE;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en_IE;
    break;
  case 'en_IN':
  case 'en-IN':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en_IN;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en_IN;
    break;
  case 'en_SG':
  case 'en-SG':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en_SG;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en_SG;
    break;
  case 'en_US':
  case 'en-US':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en_US;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en_US;
    break;
  case 'en_ZA':
  case 'en-ZA':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_en_ZA;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_en_ZA;
    break;
  case 'es':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_es;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_es;
    break;
  case 'es_419':
  case 'es-419':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_es_419;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_es_419;
    break;
  case 'es_ES':
  case 'es-ES':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_es_ES;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_es_ES;
    break;
  case 'es_MX':
  case 'es-MX':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_es_MX;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_es_MX;
    break;
  case 'es_US':
  case 'es-US':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_es_US;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_es_US;
    break;
  case 'et':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_et;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_et;
    break;
  case 'eu':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_eu;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_eu;
    break;
  case 'fa':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_fa;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_fa_u_nu_latn;
    break;
  case 'fi':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_fi;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_fi;
    break;
  case 'fil':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_fil;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_fil;
    break;
  case 'fr':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_fr;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_fr;
    break;
  case 'fr_CA':
  case 'fr-CA':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_fr_CA;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_fr_CA;
    break;
  case 'ga':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ga;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ga;
    break;
  case 'gl':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_gl;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_gl;
    break;
  case 'gsw':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_gsw;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_gsw;
    break;
  case 'gu':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_gu;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_gu;
    break;
  case 'haw':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_haw;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_haw;
    break;
  case 'he':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_he;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_he;
    break;
  case 'hi':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_hi;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_hi;
    break;
  case 'hr':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_hr;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_hr;
    break;
  case 'hu':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_hu;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_hu;
    break;
  case 'hy':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_hy;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_hy;
    break;
  case 'id':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_id;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_id;
    break;
  case 'in':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_in;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_in;
    break;
  case 'is':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_is;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_is;
    break;
  case 'it':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_it;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_it;
    break;
  case 'iw':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_iw;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_iw;
    break;
  case 'ja':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ja;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ja;
    break;
  case 'ka':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ka;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ka;
    break;
  case 'kk':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_kk;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_kk;
    break;
  case 'km':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_km;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_km;
    break;
  case 'kn':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_kn;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_kn;
    break;
  case 'ko':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ko;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ko;
    break;
  case 'ky':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ky;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ky;
    break;
  case 'ln':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ln;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ln;
    break;
  case 'lo':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_lo;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_lo;
    break;
  case 'lt':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_lt;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_lt;
    break;
  case 'lv':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_lv;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_lv;
    break;
  case 'mk':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_mk;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_mk;
    break;
  case 'ml':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ml;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ml;
    break;
  case 'mn':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_mn;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_mn;
    break;
  case 'mo':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_mo;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_mo;
    break;
  case 'mr':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_mr;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_mr_u_nu_latn;
    break;
  case 'ms':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ms;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ms;
    break;
  case 'mt':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_mt;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_mt;
    break;
  case 'my':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_my;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_my_u_nu_latn;
    break;
  case 'nb':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_nb;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_nb;
    break;
  case 'ne':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ne;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ne_u_nu_latn;
    break;
  case 'nl':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_nl;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_nl;
    break;
  case 'no':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_no;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_no;
    break;
  case 'no_NO':
  case 'no-NO':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_no_NO;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_no_NO;
    break;
  case 'or':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_or;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_or;
    break;
  case 'pa':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_pa;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_pa;
    break;
  case 'pl':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_pl;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_pl;
    break;
  case 'pt':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_pt;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_pt;
    break;
  case 'pt_BR':
  case 'pt-BR':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_pt_BR;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_pt_BR;
    break;
  case 'pt_PT':
  case 'pt-PT':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_pt_PT;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_pt_PT;
    break;
  case 'ro':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ro;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ro;
    break;
  case 'ru':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ru;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ru;
    break;
  case 'sh':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_sh;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_sh;
    break;
  case 'si':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_si;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_si;
    break;
  case 'sk':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_sk;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_sk;
    break;
  case 'sl':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_sl;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_sl;
    break;
  case 'sq':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_sq;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_sq;
    break;
  case 'sr':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_sr;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_sr;
    break;
  case 'sr_Latn':
  case 'sr-Latn':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_sr_Latn;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_sr_Latn;
    break;
  case 'sv':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_sv;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_sv;
    break;
  case 'sw':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_sw;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_sw;
    break;
  case 'ta':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ta;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ta;
    break;
  case 'te':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_te;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_te;
    break;
  case 'th':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_th;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_th;
    break;
  case 'tl':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_tl;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_tl;
    break;
  case 'tr':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_tr;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_tr;
    break;
  case 'uk':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_uk;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_uk;
    break;
  case 'ur':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_ur;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_ur;
    break;
  case 'uz':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_uz;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_uz;
    break;
  case 'vi':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_vi;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_vi;
    break;
  case 'zh':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_zh;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_zh;
    break;
  case 'zh_CN':
  case 'zh-CN':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_zh_CN;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_zh_CN;
    break;
  case 'zh_HK':
  case 'zh-HK':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_zh_HK;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_zh_HK;
    break;
  case 'zh_TW':
  case 'zh-TW':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_zh_TW;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_zh_TW;
    break;
  case 'zu':
    goog.i18n.NumberFormatSymbols = goog.i18n.NumberFormatSymbols_zu;
    goog.i18n.NumberFormatSymbols_u_nu_latn = goog.i18n.NumberFormatSymbols_zu;
    break;
}
