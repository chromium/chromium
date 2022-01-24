/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.i18n.LocaleFeature');
goog.module.declareLegacyNamespace();

/**
 * @fileoverview Provides flag for using ECMAScript 402 features vs.
 * native JavaScript Closure implementations for I18N purposes.
 */

/**
 * @define {boolean} ECMASCRIPT_INTL_OPT_OUT
 * A global flag that an application can set to avoid using native
 * ECMAScript Intl implementation in any browser or Android implementations.
 * This may be necessary for applications that cannot use the regular
 * setting of goog.LOCALE or that must provide the Javascript data and
 * to create formatted output exactly the same on both client and server.
 *
 * Default value is false. Applications can set this to true so
 * compilation will opt out of the native mode.
 */
exports.ECMASCRIPT_INTL_OPT_OUT =
    goog.define('goog.i18n.ECMASCRIPT_INTL_OPT_OUT', false);

/**
 * @define {boolean} ECMASCRIPT_COMMON_LOCALES
 * A set of locales supported by all modern browsers in ECMASCRIPT Intl.
 * Common across all of the modern browsers and Android implementations
 * available in 2019 and later.
 */
exports.ECMASCRIPT_COMMON_LOCALES_2019 =
    (goog.LOCALE == 'am' || goog.LOCALE == 'ar' || goog.LOCALE == 'bg' ||
     goog.LOCALE == 'bn' || goog.LOCALE == 'ca' || goog.LOCALE == 'cs' ||
     goog.LOCALE == 'da' || goog.LOCALE == 'de' || goog.LOCALE == 'el' ||
     goog.LOCALE == 'en' || goog.LOCALE == 'es' || goog.LOCALE == 'et' ||
     goog.LOCALE == 'fa' || goog.LOCALE == 'fi' || goog.LOCALE == 'fil' ||
     goog.LOCALE == 'fr' || goog.LOCALE == 'gu' || goog.LOCALE == 'he' ||
     goog.LOCALE == 'hi' || goog.LOCALE == 'hr' || goog.LOCALE == 'hu' ||
     goog.LOCALE == 'id' || goog.LOCALE == 'it' || goog.LOCALE == 'ja' ||
     goog.LOCALE == 'kn' || goog.LOCALE == 'ko' || goog.LOCALE == 'lt' ||
     goog.LOCALE == 'lv' || goog.LOCALE == 'ml' || goog.LOCALE == 'mr' ||
     goog.LOCALE == 'ms' || goog.LOCALE == 'nl' || goog.LOCALE == 'pl' ||
     goog.LOCALE == 'ro' || goog.LOCALE == 'ru' || goog.LOCALE == 'sk' ||
     goog.LOCALE == 'sl' || goog.LOCALE == 'sr' || goog.LOCALE == 'sv' ||
     goog.LOCALE == 'sw' || goog.LOCALE == 'ta' || goog.LOCALE == 'te' ||
     goog.LOCALE == 'th' || goog.LOCALE == 'tr' || goog.LOCALE == 'uk' ||
     goog.LOCALE == 'vi' || goog.LOCALE == 'en_GB' || goog.LOCALE == 'en-GB' ||
     goog.LOCALE == 'es_419' || goog.LOCALE == 'es-419' ||
     goog.LOCALE == 'pt_BR' || goog.LOCALE == 'pt-BR' ||
     goog.LOCALE == 'pt_PT' || goog.LOCALE == 'pt-PT' ||
     goog.LOCALE == 'zh_CN' || goog.LOCALE == 'zh-CN' ||
     goog.LOCALE == 'zh_TW' || goog.LOCALE == 'zh-TW');

/**
 * @define {boolean} USE_ECMASCRIPT_I18N Evaluated at compile to select
 * ECMAScript Intl object (when true) or JavaScript implementation (false) for
 * I18N purposes.  This set of locales is common across all of the modern
 * browsers and Android implementations available in 2019.
 */
exports.USE_ECMASCRIPT_I18N =
    (goog.FEATURESET_YEAR >= 2019 && exports.ECMASCRIPT_COMMON_LOCALES_2019 &&
     !exports.ECMASCRIPT_INTL_OPT_OUT);

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_RDTF is evaluated to enable
 * ECMAScript support for Intl.RelativeTimeFormat support in
 * browsers based on the locale. Browsers that are considered include:
 * Chrome, Firefox, Edge, and Safari.
 * As of January 2021, RelativeTimeFormat is supported in Chrome,
 * Edge, Firefox, and Safari.
 *
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/RelativeTimeFormat
 */
exports.USE_ECMASCRIPT_I18N_RDTF =
    (goog.FEATURESET_YEAR >= 2021 && exports.ECMASCRIPT_COMMON_LOCALES_2019);

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_NUMFORMAT is evaluted to enable
 * ECMAScript support for Intl.NumberFormat support in
 * browsers based on the locale. As of January 2021, NumberFormat is
 * supported in Chrome, Edge, Firefox, and Safari.
 *
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/NumberFormat
 */
exports.USE_ECMASCRIPT_I18N_NUMFORMAT =
    (goog.FEATURESET_YEAR >= 2021 && exports.ECMASCRIPT_COMMON_LOCALES_2019 &&
     !exports.ECMASCRIPT_INTL_OPT_OUT);

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_PLURALRULES is evaluated to enable
 * ECMAScript support for Intl.PluralRules support in
 * browsers based on the locale. Browsers that are considered include:
 * Chrome, Firefox, Edge, and Safari.
 * PluralRules are supported in Chrome, Edge, Firefox, and Safari.
 *
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/PluralRules/
 */
exports.USE_ECMASCRIPT_I18N_PLURALRULES =
    (!exports.ECMASCRIPT_INTL_OPT_OUT && goog.FEATURESET_YEAR >= 2021 &&
     exports.ECMASCRIPT_COMMON_LOCALES_2019);
