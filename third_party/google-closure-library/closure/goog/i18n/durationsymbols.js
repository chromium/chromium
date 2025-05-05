/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @fileoverview Duration formatting symbols.
 *
 * File generated from CLDR ver. 43
 *
 * To reduce the file size (which may cause issues in some JS
 * developing environments), this file will only contain locales
 * that are frequently used by web applications. This is defined as
 * proto/closure_locales_data.txt and will change (most likely addition)
 * over time.  Rest of the data can be found in another file named
 * "durationsymbolsext.js", which will be generated at
 * the same time together with this file.
 */

// clang-format off

goog.module('goog.i18n.DurationSymbols');
const DurationSymbolTypes = goog.require('goog.i18n.DurationSymbolTypes');

/** @type {!DurationSymbolTypes.DurationSymbols} */
let defaultSymbols;

/**
 * Returns the default DurationSymbols.
 * @return {!DurationSymbolTypes.DurationSymbols}
 */
exports.getDurationSymbols = function() {
  return defaultSymbols;
};

/**
 * Sets the default DurationSymbols if locale is in durationsymbolsext.js.
 * @param {!DurationSymbolTypes.DurationSymbols} symbols
 */
exports.setDurationSymbols = function(symbols) {
  defaultSymbols = symbols;
};


/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_af =  {
  DAY: {
    LONG: "one{# dag}other{# dae}",
    SHORT: "one{# dag}other{# dae}",
    NARROW: "one{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "one{# uur}other{# uur}",
    SHORT: "one{# u.}other{# u.}",
    NARROW: "one{# u.}other{# u.}",
  },
  MINUTE: {
    LONG: "one{# minuut}other{# minute}",
    SHORT: "one{# min.}other{# min.}",
    NARROW: "one{# min.}other{# min.}",
  },
  MONTH: {
    LONG: "one{# maand}other{# maande}",
    SHORT: "one{# md.}other{# md.}",
    NARROW: "one{# md.}other{# md.}",
  },
  SECOND: {
    LONG: "one{# sekonde}other{# sekondes}",
    SHORT: "one{# s.}other{# s.}",
    NARROW: "one{# s.}other{# s.}",
  },
  WEEK: {
    LONG: "one{# week}other{# weke}",
    SHORT: "one{# w.}other{# w.}",
    NARROW: "one{# w.}other{# w.}",
  },
  YEAR: {
    LONG: "one{# jaar}other{# jaar}",
    SHORT: "one{# j.}other{# j.}",
    NARROW: "one{# j.}other{# j.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_am =  {
  DAY: {
    LONG: "one{# ቀናት}other{# ቀናት}",
    SHORT: "one{# ቀናት}other{# ቀናት}",
    NARROW: "one{# ቀ}other{# ቀ}",
  },
  HOUR: {
    LONG: "one{# ሰዓት}other{# ሰዓቶች}",
    SHORT: "one{# ሰዓ}other{# ሰዓ}",
    NARROW: "one{# ሰ}other{# ሰ}",
  },
  MINUTE: {
    LONG: "one{# ደቂቃ}other{# ደቂቃዎች}",
    SHORT: "one{# ደቂ}other{# ደቂቃ}",
    NARROW: "one{# ደ}other{# ደ}",
  },
  MONTH: {
    LONG: "one{# ወር}other{# ወራት}",
    SHORT: "one{# ወራት}other{# ወራት}",
    NARROW: "one{# ወር}other{# ወር}",
  },
  SECOND: {
    LONG: "one{# ሰከንድ}other{# ሰከንዶች}",
    SHORT: "one{# ሰከ}other{# ሰከ}",
    NARROW: "one{# ሰ}other{# ሰ}",
  },
  WEEK: {
    LONG: "one{# ሳምንት}other{# ሳምንታት}",
    SHORT: "one{# ሳምንት}other{# ሳምንታት}",
    NARROW: "one{# ሳምንት}other{# ሳምንት}",
  },
  YEAR: {
    LONG: "one{# ዓመት}other{# ዓመታት}",
    SHORT: "one{# ዓመት}other{# ዓመታት}",
    NARROW: "one{# ዓመት}other{# ዓ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ar =  {
  DAY: {
    LONG: "zero{# يوم}one{يوم}two{يومان}few{# أيام}many{# يومًا}other{# يوم}",
    SHORT: "zero{# يوم}one{يوم}two{يومان}few{# أيام}many{# يومًا}other{# يوم}",
    NARROW: "zero{# ي}one{# ي}two{# ي}few{# ي}many{# ي}other{# ي}",
  },
  HOUR: {
    LONG: "zero{# ساعة}one{ساعة}two{ساعتان}few{# ساعات}many{# ساعة}other{# ساعة}",
    SHORT: "zero{# س}one{# س}two{# س}few{# س}many{# س}other{# س}",
    NARROW: "zero{# س}one{# س}two{# س}few{# س}many{# س}other{# س}",
  },
  MINUTE: {
    LONG: "zero{# دقيقة}one{دقيقة}two{دقيقتان}few{# دقائق}many{# دقيقة}other{# دقيقة}",
    SHORT: "zero{# د}one{# د}two{# د}few{# د}many{# د}other{# د}",
    NARROW: "zero{# د}one{# د}two{# د}few{# د}many{# د}other{# د}",
  },
  MONTH: {
    LONG: "zero{# شهر}one{شهر}two{شهران}few{# أشهر}many{# شهرًا}other{# شهر}",
    SHORT: "zero{# شهر}one{شهر}two{شهران}few{# أشهر}many{# شهرًا}other{# شهر}",
    NARROW: "zero{# شهر}one{شهر}two{شهران}few{# أشهر}many{# شهرًا}other{# شهر}",
  },
  SECOND: {
    LONG: "zero{# ثانية}one{ثانية}two{ثانيتان}few{# ثوان}many{# ثانية}other{# ثانية}",
    SHORT: "zero{# ث}one{# ث}two{# ث}few{# ث}many{# ث}other{# ث}",
    NARROW: "zero{# ث}one{# ث}two{# ث}few{# ث}many{# ث}other{# ث}",
  },
  WEEK: {
    LONG: "zero{# أسبوع}one{أسبوع}two{أسبوعان}few{# أسابيع}many{# أسبوعًا}other{# أسبوع}",
    SHORT: "zero{# أسبوع}one{أسبوع}two{أسبوعان}few{# أسابيع}many{# أسبوعًا}other{# أسبوع}",
    NARROW: "zero{# أ}one{# أ}two{# أ}few{# أ}many{# أ}other{# أ}",
  },
  YEAR: {
    LONG: "zero{# سنة}one{سنة}two{سنتان}few{# سنوات}many{# سنة}other{# سنة}",
    SHORT: "zero{# سنة}one{سنة واحدة}two{سنتان}few{# سنوات}many{# سنة}other{# سنة}",
    NARROW: "zero{# سنة}one{# سنة}two{# سنة}few{# سنة}many{# سنة}other{# سنة}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ar_DZ = exports.DurationSymbols_ar;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ar_EG = exports.DurationSymbols_ar;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_az =  {
  DAY: {
    LONG: "one{# gün}other{# gün}",
    SHORT: "one{# gün}other{# gün}",
    NARROW: "one{# gün}other{# gün}",
  },
  HOUR: {
    LONG: "one{# saat}other{# saat}",
    SHORT: "one{# saat}other{# saat}",
    NARROW: "one{# saat}other{# saat}",
  },
  MINUTE: {
    LONG: "one{# dəqiqə}other{# dəqiqə}",
    SHORT: "one{# dəq}other{# dəq}",
    NARROW: "one{# dəq}other{# dəq}",
  },
  MONTH: {
    LONG: "one{# ay}other{# ay}",
    SHORT: "one{# ay}other{# ay}",
    NARROW: "one{# ay}other{# ay}",
  },
  SECOND: {
    LONG: "one{# saniyə}other{# saniyə}",
    SHORT: "one{# san}other{# san}",
    NARROW: "one{# san}other{# san}",
  },
  WEEK: {
    LONG: "one{# həftə}other{# həftə}",
    SHORT: "one{# hft}other{# hft}",
    NARROW: "one{# hft}other{# hft}",
  },
  YEAR: {
    LONG: "one{# il}other{# il}",
    SHORT: "one{# il}other{# il}",
    NARROW: "one{# il}other{# il}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_be =  {
  DAY: {
    LONG: "one{# суткі}few{# сутак}many{# сутак}other{# сутак}",
    SHORT: "one{# сут}few{# сут}many{# сут}other{# сут}",
    NARROW: "one{# сут}few{# сут}many{# сут}other{# сут}",
  },
  HOUR: {
    LONG: "one{# гадзіна}few{# гадзіны}many{# гадзін}other{# гадзіны}",
    SHORT: "one{# гадз}few{# гадз}many{# гадз}other{# гадз}",
    NARROW: "one{# гадз}few{# гадз}many{# гадз}other{# гадз}",
  },
  MINUTE: {
    LONG: "one{# хвіліна}few{# хвіліны}many{# хвілін}other{# хвіліны}",
    SHORT: "one{# хв}few{# хв}many{# хв}other{# хв}",
    NARROW: "one{# хв}few{# хв}many{# хв}other{# хв}",
  },
  MONTH: {
    LONG: "one{# месяц}few{# месяца}many{# месяцаў}other{# месяца}",
    SHORT: "one{# мес.}few{# мес.}many{# мес.}other{# мес.}",
    NARROW: "one{# мес.}few{# мес.}many{# мес.}other{# мес.}",
  },
  SECOND: {
    LONG: "one{# секунда}few{# секунды}many{# секунд}other{# секунды}",
    SHORT: "one{# с}few{# с}many{# с}other{# с}",
    NARROW: "one{# с}few{# с}many{# с}other{# с}",
  },
  WEEK: {
    LONG: "one{# тыдзень}few{# тыдні}many{# тыдняў}other{# тыдня}",
    SHORT: "one{# тыдз.}few{# тыдз.}many{# тыдз.}other{# тыдз.}",
    NARROW: "one{# тыдз.}few{# тыдз.}many{# тыдз.}other{# тыдз.}",
  },
  YEAR: {
    LONG: "one{# год}few{# гады}many{# гадоў}other{# года}",
    SHORT: "one{# г.}few{# г.}many{# г.}other{# г.}",
    NARROW: "one{# г.}few{# г.}many{# г.}other{# г.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_bg =  {
  DAY: {
    LONG: "one{# ден}other{# дни}",
    SHORT: "one{# д}other{# д}",
    NARROW: "one{# д}other{# д}",
  },
  HOUR: {
    LONG: "one{# час}other{# часа}",
    SHORT: "one{# ч}other{# ч}",
    NARROW: "one{# ч}other{# ч}",
  },
  MINUTE: {
    LONG: "one{# минута}other{# минути}",
    SHORT: "one{# мин}other{# мин}",
    NARROW: "one{# мин}other{# мин}",
  },
  MONTH: {
    LONG: "one{# месец}other{# месеца}",
    SHORT: "one{# мес.}other{# мес.}",
    NARROW: "one{# мес.}other{# мес.}",
  },
  SECOND: {
    LONG: "one{# секунда}other{# секунди}",
    SHORT: "one{# сек}other{# сек}",
    NARROW: "one{# с}other{# с}",
  },
  WEEK: {
    LONG: "one{# седмица}other{# седмици}",
    SHORT: "one{# седм.}other{# седм.}",
    NARROW: "one{# седм.}other{# седм.}",
  },
  YEAR: {
    LONG: "one{# година}other{# години}",
    SHORT: "one{# год.}other{# год.}",
    NARROW: "one{# г.}other{# г.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_bn =  {
  DAY: {
    LONG: "one{# দিন}other{# দিন}",
    SHORT: "one{# দিন}other{# দিন}",
    NARROW: "one{# দিন}other{# দিন}",
  },
  HOUR: {
    LONG: "one{# ঘন্টা}other{# ঘন্টা}",
    SHORT: "one{# ঘন্টা}other{# ঘন্টা}",
    NARROW: "one{# ঘঃ}other{# ঘঃ}",
  },
  MINUTE: {
    LONG: "one{# মিনিট}other{# মিনিট}",
    SHORT: "one{# মিনিট}other{# মিনিট}",
    NARROW: "one{# মিঃ}other{# মিঃ}",
  },
  MONTH: {
    LONG: "one{# মাস}other{# মাস}",
    SHORT: "one{# মাস}other{# মাস}",
    NARROW: "one{# মাস}other{# মাস}",
  },
  SECOND: {
    LONG: "one{# সেকেন্ড}other{# সেকেন্ড}",
    SHORT: "one{# সেকেন্ড}other{# সেকেন্ড}",
    NARROW: "one{# সেঃ}other{# সেঃ}",
  },
  WEEK: {
    LONG: "one{# সপ্তাহ}other{# সপ্তাহ}",
    SHORT: "one{# সপ্তাহ}other{# সপ্তাহ}",
    NARROW: "one{# সপ্তাহ}other{# সপ্তাহ}",
  },
  YEAR: {
    LONG: "one{# বছর}other{# বছর}",
    SHORT: "one{# বছর}other{# বছর}",
    NARROW: "one{# বছর}other{# বছর}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_br =  {
  DAY: {
    LONG: "one{# deiz}two{# zeiz}few{# deiz}many{# a zeizioù}other{# deiz}",
    SHORT: "one{# d}two{# d}few{# d}many{# d}other{# d}",
    NARROW: "one{#d}two{#d}few{#d}many{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# eur}two{# eur}few{# eur}many{# a eurioù}other{# eur}",
    SHORT: "one{# h}two{# h}few{# h}many{# h}other{# h}",
    NARROW: "one{#h}two{#h}few{#h}many{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# munut}two{# vunut}few{# munut}many{# a vunutoù}other{# munut}",
    SHORT: "one{# min}two{# min}few{# min}many{# min}other{# min}",
    NARROW: "one{#min}two{#min}few{#min}many{#min}other{#min}",
  },
  MONTH: {
    LONG: "one{# miz}two{# viz}few{# miz}many{# a vizioù}other{# miz}",
    SHORT: "one{# m.}two{# m.}few{# m.}many{# m.}other{# m.}",
    NARROW: "one{#m}two{#m}few{#m}many{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# eilenn}two{# eilenn}few{# eilenn}many{# a eilennoù}other{# eilenn}",
    SHORT: "one{# s}two{# s}few{# s}many{# s}other{# s}",
    NARROW: "one{#s}two{#s}few{#s}many{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# sizhun}two{# sizhun}few{# sizhun}many{# a sizhunioù}other{# sizhun}",
    SHORT: "one{# sizh.}two{# sizh.}few{# sizh.}many{# sizh.}other{# sizh.}",
    NARROW: "one{#sizh.}two{#sizh.}few{#sizh.}many{#sizh.}other{#sizh.}",
  },
  YEAR: {
    LONG: "one{# bloaz}two{# vloaz}few{# bloaz}many{# a vloazioù}other{# vloaz}",
    SHORT: "one{# bl.}two{# bl.}few{# bl.}many{# bl.}other{# bl.}",
    NARROW: "one{#b}two{#b}few{#b}many{#b}other{#b}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_bs =  {
  DAY: {
    LONG: "one{# dan}few{# dana}other{# dana}",
    SHORT: "one{# dan}few{# dana}other{# dana}",
    NARROW: "one{# d.}few{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "one{# sat}few{# sata}other{# sati}",
    SHORT: "one{# h}few{# h}other{# h}",
    NARROW: "one{# h}few{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minuta}few{# minute}other{# minuta}",
    SHORT: "one{# min.}few{# min.}other{# min.}",
    NARROW: "one{# m}few{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# mjesec}few{# mjeseca}other{# mjeseci}",
    SHORT: "one{# mj.}few{# mj.}other{# mj.}",
    NARROW: "one{# mj.}few{# mj.}other{# mj.}",
  },
  SECOND: {
    LONG: "one{# sekunda}few{# sekunde}other{# sekundi}",
    SHORT: "one{# sek.}few{# sek.}other{# sek.}",
    NARROW: "one{# s}few{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# sedmica}few{# sedmice}other{# sedmica}",
    SHORT: "one{# sedm.}few{# sedm.}other{# sedm.}",
    NARROW: "one{# sedm.}few{# sedm.}other{# sedm.}",
  },
  YEAR: {
    LONG: "one{# godina}few{# godine}other{# godina}",
    SHORT: "one{# god.}few{# god.}other{# god.}",
    NARROW: "one{# god.}few{# god.}other{# god.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ca =  {
  DAY: {
    LONG: "one{# dia}other{# dies}",
    SHORT: "one{# dia}other{# dies}",
    NARROW: "one{# d}other{# d}",
  },
  HOUR: {
    LONG: "one{# hora}other{# hores}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minut}other{# minuts}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{# min}other{# min}",
  },
  MONTH: {
    LONG: "one{# mes}other{# mesos}",
    SHORT: "one{# mes}other{# m}",
    NARROW: "one{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# segon}other{# segons}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# setmana}other{# setmanes}",
    SHORT: "one{# setm.}other{# setm.}",
    NARROW: "one{# setm.}other{# setm.}",
  },
  YEAR: {
    LONG: "one{# any}other{# anys}",
    SHORT: "one{# any}other{# anys}",
    NARROW: "one{# any}other{# anys}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_chr =  {
  DAY: {
    LONG: "one{# ᎢᎦ}other{# ᎯᎸᏍᎩ ᏧᏒᎯᏓ}",
    SHORT: "one{# ᎢᎦ}other{# ᏧᏒᎯᏓ}",
    NARROW: "one{#Ꭲ}other{#Ꭲ}",
  },
  HOUR: {
    LONG: "one{# ᏑᏟᎶᏓ}other{# ᎢᏳᏟᎶᏓ}",
    SHORT: "one{# ᏑᏟ}other{# ᏑᏟ}",
    NARROW: "one{#Ꮡ}other{#Ꮡ}",
  },
  MINUTE: {
    LONG: "one{# ᎢᏯᏔᏬᏍᏔᏅ}other{# ᎢᏯᏔᏬᏍᏔᏅ}",
    SHORT: "one{# ᎢᏯᏔ}other{# ᎢᏯᏔ}",
    NARROW: "one{#Ꭲ}other{#Ꭲ}",
  },
  MONTH: {
    LONG: "one{# ᎧᎸᎢ}other{# ᏗᎧᎸᎢ}",
    SHORT: "one{# ᎧᎸᎢ}other{# ᏗᎧᎸᎢ}",
    NARROW: "one{#Ꭷ}other{#Ꭷ}",
  },
  SECOND: {
    LONG: "one{# ᎠᏎᏢ}other{# ᏗᏎᏢ}",
    SHORT: "one{# ᎠᏎᏢ}other{# ᎠᏎᏢ}",
    NARROW: "one{#ᎠᏎ}other{#ᎠᏎ}",
  },
  WEEK: {
    LONG: "one{# ᏒᎾᏙᏓᏆᏍᏗ}other{# ᎢᏳᎾᏙᏓᏆᏍᏗ}",
    SHORT: "one{# ᏒᎾ}other{# ᎢᏳᎾ}",
    NARROW: "one{#Ꮢ}other{#Ꮢ}",
  },
  YEAR: {
    LONG: "one{# ᎤᏕᏘᏴᏌᏗᏒᎢ}other{# ᏧᏕᏘᏴᏌᏗᏒᎢ}",
    SHORT: "one{# ᎤᏕ}other{# ᏧᏕ}",
    NARROW: "one{#Ꭴ}other{#Ꭴ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_cs =  {
  DAY: {
    LONG: "one{# den}few{# dny}many{# dne}other{# dnů}",
    SHORT: "one{# den}few{# dny}many{# dne}other{# dnů}",
    NARROW: "one{# d.}few{# d.}many{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "one{# hodina}few{# hodiny}many{# hodiny}other{# hodin}",
    SHORT: "one{# h}few{# h}many{# h}other{# h}",
    NARROW: "one{# h}few{# h}many{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minuta}few{# minuty}many{# minuty}other{# minut}",
    SHORT: "one{# min}few{# min}many{# min}other{# min}",
    NARROW: "one{# m}few{# m}many{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# měsíc}few{# měsíce}many{# měsíce}other{# měsíců}",
    SHORT: "one{# měs.}few{# měs.}many{# měs.}other{# měs.}",
    NARROW: "one{# m.}few{# m.}many{# m.}other{# m.}",
  },
  SECOND: {
    LONG: "one{# sekunda}few{# sekundy}many{# sekundy}other{# sekund}",
    SHORT: "one{# s}few{# s}many{# s}other{# s}",
    NARROW: "one{# s}few{# s}many{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# týden}few{# týdny}many{# týdne}other{# týdnů}",
    SHORT: "one{# týd.}few{# týd.}many{# týd.}other{# týd.}",
    NARROW: "one{# t.}few{# t.}many{# t.}other{# t.}",
  },
  YEAR: {
    LONG: "one{# rok}few{# roky}many{# roku}other{# let}",
    SHORT: "one{# rok}few{# roky}many{# roku}other{# let}",
    NARROW: "one{# r.}few{# r.}many{# r.}other{# l.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_cy =  {
  DAY: {
    LONG: "zero{# diwrnod}one{# diwrnod}two{# ddiwrnod}few{# diwrnod}many{# diwrnod}other{# diwrnod}",
    SHORT: "zero{# diwrnod}one{# diwrnod}two{# ddiwrnod}few{# diwrnod}many{# diwrnod}other{# diwrnod}",
    NARROW: "zero{#d}one{#d}two{#d}few{#d}many{#d}other{#d}",
  },
  HOUR: {
    LONG: "zero{# awr}one{# awr}two{# awr}few{# awr}many{# awr}other{# awr}",
    SHORT: "zero{# awr}one{# awr}two{# awr}few{# awr}many{# awr}other{# awr}",
    NARROW: "zero{# awr}one{# awr}two{# awr}few{# awr}many{# awr}other{# awr}",
  },
  MINUTE: {
    LONG: "zero{# munud}one{# munud}two{# funud}few{# munud}many{# munud}other{# munud}",
    SHORT: "zero{# mun}one{# mun}two{# mun}few{# mun}many{# mun}other{# mun}",
    NARROW: "zero{#mun}one{#mun}two{#mun}few{#mun}many{#mun}other{#mun}",
  },
  MONTH: {
    LONG: "zero{# mis}one{# mis}two{# fis}few{# mis}many{# mis}other{# mis}",
    SHORT: "zero{# mis}one{# mis}two{# fis}few{# mis}many{# mis}other{# mis}",
    NARROW: "zero{#m}one{#m}two{#m}few{#m}many{#m}other{#m}",
  },
  SECOND: {
    LONG: "zero{# eiliad}one{# eiliad}two{# eiliad}few{# eiliad}many{# eiliad}other{# eiliad}",
    SHORT: "zero{# eil}one{# eil}two{# eil}few{# eil}many{# eil}other{# eil}",
    NARROW: "zero{# eil}one{# eil}two{# eil}few{# eil}many{# eil}other{# eil}",
  },
  WEEK: {
    LONG: "zero{# wythnos}one{# wythnos}two{# wythnos}few{# wythnos}many{# wythnos}other{# wythnos}",
    SHORT: "zero{# ws}one{# ws}two{# ws}few{# ws}many{# ws}other{# ws}",
    NARROW: "zero{# ws}one{#w}two{# ws}few{# ws}many{# ws}other{#w}",
  },
  YEAR: {
    LONG: "zero{# mlynedd}one{# flwyddyn}two{# flynedd}few{# blynedd}many{# blynedd}other{# mlynedd}",
    SHORT: "zero{# bl}one{# bl}two{# bl}few{# bl}many{# bl}other{# bl}",
    NARROW: "zero{#bl}one{#bl}two{#bl}few{#bl}many{#bl}other{#bl}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_da =  {
  DAY: {
    LONG: "one{# dag}other{# dage}",
    SHORT: "one{# dag}other{# dage}",
    NARROW: "one{# d}other{# d}",
  },
  HOUR: {
    LONG: "one{# time}other{# timer}",
    SHORT: "one{# t.}other{# t.}",
    NARROW: "one{# t}other{# t}",
  },
  MINUTE: {
    LONG: "one{# minut}other{# minutter}",
    SHORT: "one{# min.}other{# min.}",
    NARROW: "one{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# måned}other{# måneder}",
    SHORT: "one{# md.}other{# mdr.}",
    NARROW: "one{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# sekund}other{# sekunder}",
    SHORT: "one{# sek.}other{# sek.}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# uge}other{# uger}",
    SHORT: "one{# uge}other{# uger}",
    NARROW: "one{# u}other{# u}",
  },
  YEAR: {
    LONG: "one{# år}other{# år}",
    SHORT: "one{# år}other{# år}",
    NARROW: "one{# år}other{# år}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_de =  {
  DAY: {
    LONG: "one{# Tag}other{# Tage}",
    SHORT: "one{# Tg.}other{# Tg.}",
    NARROW: "one{# T}other{# T}",
  },
  HOUR: {
    LONG: "one{# Stunde}other{# Stunden}",
    SHORT: "one{# Std.}other{# Std.}",
    NARROW: "one{# Std.}other{# Std.}",
  },
  MINUTE: {
    LONG: "one{# Minute}other{# Minuten}",
    SHORT: "one{# Min.}other{# Min.}",
    NARROW: "one{# Min.}other{# Min.}",
  },
  MONTH: {
    LONG: "one{# Monat}other{# Monate}",
    SHORT: "one{# Mon.}other{# Mon.}",
    NARROW: "one{# M}other{# M}",
  },
  SECOND: {
    LONG: "one{# Sekunde}other{# Sekunden}",
    SHORT: "one{# Sek.}other{# Sek.}",
    NARROW: "one{# Sek.}other{# Sek.}",
  },
  WEEK: {
    LONG: "one{# Woche}other{# Wochen}",
    SHORT: "one{# Wo.}other{# Wo.}",
    NARROW: "one{# W}other{# W}",
  },
  YEAR: {
    LONG: "one{# Jahr}other{# Jahre}",
    SHORT: "one{# J}other{# J}",
    NARROW: "one{# J}other{# J}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_de_AT = exports.DurationSymbols_de;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_de_CH = exports.DurationSymbols_de;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_el =  {
  DAY: {
    LONG: "one{# ημέρα}other{# ημέρες}",
    SHORT: "one{# ημέρα}other{# ημέρες}",
    NARROW: "one{# η}other{# η}",
  },
  HOUR: {
    LONG: "one{# ώρα}other{# ώρες}",
    SHORT: "one{# ώ.}other{# ώ.}",
    NARROW: "one{# ώ}other{# ώ}",
  },
  MINUTE: {
    LONG: "one{# λεπτό}other{# λεπτά}",
    SHORT: "one{# λ.}other{# λ.}",
    NARROW: "one{# λ}other{# λ}",
  },
  MONTH: {
    LONG: "one{# μήνας}other{# μήνες}",
    SHORT: "one{# μήν.}other{# μήν.}",
    NARROW: "one{# μ}other{# μ}",
  },
  SECOND: {
    LONG: "one{# δευτερόλεπτο}other{# δευτερόλεπτα}",
    SHORT: "one{# δευτ.}other{# δευτ.}",
    NARROW: "one{# δ}other{# δ}",
  },
  WEEK: {
    LONG: "one{# εβδομάδα}other{# εβδομάδες}",
    SHORT: "one{# εβδ.}other{# εβδ.}",
    NARROW: "one{# ε}other{# ε}",
  },
  YEAR: {
    LONG: "one{# έτος}other{# έτη}",
    SHORT: "one{# έτ.}other{# έτ.}",
    NARROW: "one{# έ}other{# έ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en =  {
  DAY: {
    LONG: "one{# day}other{# days}",
    SHORT: "one{# day}other{# days}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hour}other{# hours}",
    SHORT: "one{# hr}other{# hr}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# month}other{# months}",
    SHORT: "one{# mth}other{# mths}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# second}other{# seconds}",
    SHORT: "one{# sec}other{# sec}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# week}other{# weeks}",
    SHORT: "one{# wk}other{# wks}",
    NARROW: "one{#w}other{#w}",
  },
  YEAR: {
    LONG: "one{# year}other{# years}",
    SHORT: "one{# yr}other{# yrs}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en_AU =  {
  DAY: {
    LONG: "one{# day}other{# days}",
    SHORT: "one{# day}other{# days}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hour}other{# hours}",
    SHORT: "one{# hr}other{# hrs}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min.}other{# mins}",
    NARROW: "one{#min.}other{#min.}",
  },
  MONTH: {
    LONG: "one{# month}other{# months}",
    SHORT: "one{# mth}other{# mths}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# second}other{# seconds}",
    SHORT: "one{# sec.}other{# secs}",
    NARROW: "one{#s.}other{#s.}",
  },
  WEEK: {
    LONG: "one{# week}other{# weeks}",
    SHORT: "one{# wk}other{# wks}",
    NARROW: "one{#w}other{#w}",
  },
  YEAR: {
    LONG: "one{# year}other{# years}",
    SHORT: "one{# yr}other{# yrs}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en_CA =  {
  DAY: {
    LONG: "one{# day}other{# days}",
    SHORT: "one{# day}other{# days}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hour}other{# hours}",
    SHORT: "one{# hr}other{# hrs}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# mins}",
    NARROW: "one{#min}other{#min}",
  },
  MONTH: {
    LONG: "one{# month}other{# months}",
    SHORT: "one{# mo}other{# mos}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# second}other{# seconds}",
    SHORT: "one{# sec}other{# secs}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# week}other{# weeks}",
    SHORT: "one{# wk}other{# wks}",
    NARROW: "one{#w}other{#w}",
  },
  YEAR: {
    LONG: "one{# year}other{# years}",
    SHORT: "one{# yr}other{# yrs}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en_GB =  {
  DAY: {
    LONG: "one{# day}other{# days}",
    SHORT: "one{# day}other{# days}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hour}other{# hours}",
    SHORT: "one{# hr}other{# hrs}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# mins}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# month}other{# months}",
    SHORT: "one{# mth}other{# mths}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# second}other{# seconds}",
    SHORT: "one{# sec}other{# secs}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# week}other{# weeks}",
    SHORT: "one{# wk}other{# wks}",
    NARROW: "one{#w}other{#w}",
  },
  YEAR: {
    LONG: "one{# year}other{# years}",
    SHORT: "one{# yr}other{# yrs}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en_IE =  {
  DAY: {
    LONG: "one{# day}other{# days}",
    SHORT: "one{# day}other{# days}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hour}other{# hours}",
    SHORT: "one{# hr}other{# hrs}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# mins}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# month}other{# months}",
    SHORT: "one{# mth}other{# mths}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# second}other{# seconds}",
    SHORT: "one{# sec}other{# secs}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# week}other{# weeks}",
    SHORT: "one{# wk}other{# wks}",
    NARROW: "one{#w}other{#w}",
  },
  YEAR: {
    LONG: "one{# year}other{# years}",
    SHORT: "one{# yr}other{# yrs}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en_IN =  {
  DAY: {
    LONG: "one{# day}other{# days}",
    SHORT: "one{# day}other{# days}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hour}other{# hours}",
    SHORT: "one{# hr}other{# hrs}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# mins}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# month}other{# months}",
    SHORT: "one{# mth}other{# mths}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# second}other{# seconds}",
    SHORT: "one{# sec}other{# secs}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# week}other{# weeks}",
    SHORT: "one{# wk}other{# wks}",
    NARROW: "one{#w}other{#w}",
  },
  YEAR: {
    LONG: "one{# year}other{# years}",
    SHORT: "one{# yr}other{# yrs}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en_SG =  {
  DAY: {
    LONG: "one{# day}other{# days}",
    SHORT: "one{# day}other{# days}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hour}other{# hours}",
    SHORT: "one{# hr}other{# hrs}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# mins}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# month}other{# months}",
    SHORT: "one{# mth}other{# mths}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# second}other{# seconds}",
    SHORT: "one{# sec}other{# secs}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# week}other{# weeks}",
    SHORT: "one{# wk}other{# wks}",
    NARROW: "one{#w}other{#w}",
  },
  YEAR: {
    LONG: "one{# year}other{# years}",
    SHORT: "one{# yr}other{# yrs}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en_US = exports.DurationSymbols_en;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_en_ZA =  {
  DAY: {
    LONG: "one{# day}other{# days}",
    SHORT: "one{# day}other{# days}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hour}other{# hours}",
    SHORT: "one{# hr}other{# hrs}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# mins}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# month}other{# months}",
    SHORT: "one{# mth}other{# mths}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# second}other{# seconds}",
    SHORT: "one{# sec}other{# secs}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# week}other{# weeks}",
    SHORT: "one{# wk}other{# wks}",
    NARROW: "one{#w}other{#w}",
  },
  YEAR: {
    LONG: "one{# year}other{# years}",
    SHORT: "one{# yr}other{# yrs}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_es =  {
  DAY: {
    LONG: "one{# día}other{# días}",
    SHORT: "one{# d}other{# d}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hora}other{# horas}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# minutos}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#min}other{#min}",
  },
  MONTH: {
    LONG: "one{# mes}other{# meses}",
    SHORT: "one{# m.}other{# m.}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# segundos}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# semana}other{# semanas}",
    SHORT: "one{# sem.}other{# sem.}",
    NARROW: "one{#sem}other{#sem}",
  },
  YEAR: {
    LONG: "one{# año}other{# años}",
    SHORT: "one{# a}other{# a}",
    NARROW: "one{#a}other{#a}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_es_419 =  {
  DAY: {
    LONG: "one{# día}other{# días}",
    SHORT: "one{# d.}other{# dd.}",
    NARROW: "one{#d.}other{#dd.}",
  },
  HOUR: {
    LONG: "one{# hora}other{# horas}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# minutos}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# mes}other{# meses}",
    SHORT: "one{# m.}other{# mm.}",
    NARROW: "one{#m.}other{#mm.}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# segundos}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# semana}other{# semanas}",
    SHORT: "one{# sem.}other{# sems.}",
    NARROW: "one{#sem.}other{#sems.}",
  },
  YEAR: {
    LONG: "one{# año}other{# años}",
    SHORT: "one{# a.}other{# aa.}",
    NARROW: "one{#a.}other{#aa.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_es_ES = exports.DurationSymbols_es;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_es_MX =  {
  DAY: {
    LONG: "one{# día}other{# días}",
    SHORT: "one{# día}other{# días}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hora}other{# horas}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# minutos}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# mes}other{# meses}",
    SHORT: "one{# m}other{# m}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# segundos}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# semana}other{# semanas}",
    SHORT: "one{# sem}other{# sem}",
    NARROW: "one{#sem}other{#sem}",
  },
  YEAR: {
    LONG: "one{# año}other{# años}",
    SHORT: "one{# a}other{# a}",
    NARROW: "one{#a}other{#a}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_es_US =  {
  DAY: {
    LONG: "one{# día}other{# días}",
    SHORT: "one{# día}other{# días}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# hora}other{# horas}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# minutos}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# mes}other{# meses}",
    SHORT: "one{# m}other{# mm.}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# segundos}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# semana}other{# semanas}",
    SHORT: "one{# sem.}other{# sems.}",
    NARROW: "one{#sem.}other{#sems.}",
  },
  YEAR: {
    LONG: "one{# año}other{# años}",
    SHORT: "one{# a}other{# aa.}",
    NARROW: "one{#a}other{#a}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_et =  {
  DAY: {
    LONG: "one{# ööpäev}other{# ööpäeva}",
    SHORT: "one{# päev}other{# päeva}",
    NARROW: "one{# p}other{# p}",
  },
  HOUR: {
    LONG: "one{# tund}other{# tundi}",
    SHORT: "one{# t}other{# t}",
    NARROW: "one{# t}other{# t}",
  },
  MINUTE: {
    LONG: "one{# minut}other{# minutit}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{# min}other{# min}",
  },
  MONTH: {
    LONG: "one{# kuu}other{# kuud}",
    SHORT: "one{# kuu}other{# kuud}",
    NARROW: "one{# k}other{# k}",
  },
  SECOND: {
    LONG: "one{# sekund}other{# sekundit}",
    SHORT: "one{# sek}other{# sek}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# nädal}other{# nädalat}",
    SHORT: "one{# näd}other{# näd}",
    NARROW: "one{# n}other{# n}",
  },
  YEAR: {
    LONG: "one{# aasta}other{# aastat}",
    SHORT: "one{# a}other{# a}",
    NARROW: "one{# a}other{# a}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_eu =  {
  DAY: {
    LONG: "one{# egun}other{# egun}",
    SHORT: "one{# egun}other{# egun}",
    NARROW: "one{# e.}other{# e.}",
  },
  HOUR: {
    LONG: "one{# ordu}other{# ordu}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minutu}other{# minutu}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{# min}other{# min}",
  },
  MONTH: {
    LONG: "one{# hilabete}other{# hilabete}",
    SHORT: "one{# hilabete}other{# hilabete}",
    NARROW: "one{# hil}other{# hil}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# segundo}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# aste}other{# aste}",
    SHORT: "one{# aste}other{# aste}",
    NARROW: "one{# aste}other{# aste}",
  },
  YEAR: {
    LONG: "one{# urte}other{# urte}",
    SHORT: "one{# urte}other{# urte}",
    NARROW: "one{# u.}other{# u.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_fa =  {
  DAY: {
    LONG: "one{# روز}other{# روز}",
    SHORT: "one{# روز}other{# روز}",
    NARROW: "one{# روز}other{# روز}",
  },
  HOUR: {
    LONG: "one{# ساعت}other{# ساعت}",
    SHORT: "one{# ساعت}other{# ساعت}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# دقیقه}other{# دقیقه}",
    SHORT: "one{# دقیقه}other{# دقیقه}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# ماه}other{# ماه}",
    SHORT: "one{# ماه}other{# ماه}",
    NARROW: "one{# ماه}other{# ماه}",
  },
  SECOND: {
    LONG: "one{# ثانیه}other{# ثانیه}",
    SHORT: "one{# ثانیه}other{# ثانیه}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# هفته}other{# هفته}",
    SHORT: "one{# هفته}other{# هفته}",
    NARROW: "one{# هفته}other{# هفته}",
  },
  YEAR: {
    LONG: "one{# سال}other{# سال}",
    SHORT: "one{# سال}other{# سال}",
    NARROW: "one{# سال}other{# سال}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_fi =  {
  DAY: {
    LONG: "one{# päivä}other{# päivää}",
    SHORT: "one{# pv}other{# pv}",
    NARROW: "one{#pv}other{#pv}",
  },
  HOUR: {
    LONG: "one{# tunti}other{# tuntia}",
    SHORT: "one{# t}other{# t}",
    NARROW: "one{#t}other{#t}",
  },
  MINUTE: {
    LONG: "one{# minuutti}other{# minuuttia}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#min}other{#min}",
  },
  MONTH: {
    LONG: "one{# kuukausi}other{# kuukautta}",
    SHORT: "one{# kk}other{# kk}",
    NARROW: "one{#kk}other{#kk}",
  },
  SECOND: {
    LONG: "one{# sekunti}other{# sekuntia}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# viikko}other{# viikkoa}",
    SHORT: "one{# vk}other{# vk}",
    NARROW: "one{#vk}other{#vk}",
  },
  YEAR: {
    LONG: "one{# vuosi}other{# vuotta}",
    SHORT: "one{# v}other{# v}",
    NARROW: "one{#v}other{#v}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_fil =  {
  DAY: {
    LONG: "one{# araw}other{# na araw}",
    SHORT: "one{# araw}other{# araw}",
    NARROW: "one{# araw}other{# na araw}",
  },
  HOUR: {
    LONG: "one{# oras}other{# na oras}",
    SHORT: "one{# oras}other{# na oras}",
    NARROW: "one{# oras}other{# oras}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# na minuto}",
    SHORT: "one{# min.}other{# min.}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# buwan}other{# buwan}",
    SHORT: "one{# buwan}other{# buwan}",
    NARROW: "one{#buwan}other{# buwan}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# na segundo}",
    SHORT: "one{# seg.}other{# seg.}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# linggo}other{# na linggo}",
    SHORT: "one{# linggo}other{# na linggo}",
    NARROW: "one{#linggo}other{#linggo}",
  },
  YEAR: {
    LONG: "one{# taon}other{# na taon}",
    SHORT: "one{# taon}other{# taon}",
    NARROW: "one{#taon}other{#taon}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_fr =  {
  DAY: {
    LONG: "one{# jour}other{# jours}",
    SHORT: "one{# j}other{# j}",
    NARROW: "one{#j}other{#j}",
  },
  HOUR: {
    LONG: "one{# heure}other{# heures}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#min}other{#min}",
  },
  MONTH: {
    LONG: "one{# mois}other{# mois}",
    SHORT: "one{# m.}other{# m.}",
    NARROW: "one{#m.}other{#m.}",
  },
  SECOND: {
    LONG: "one{# seconde}other{# secondes}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# semaine}other{# semaines}",
    SHORT: "one{# sem.}other{# sem.}",
    NARROW: "one{#sem.}other{#sem.}",
  },
  YEAR: {
    LONG: "one{# an}other{# ans}",
    SHORT: "one{# an}other{# ans}",
    NARROW: "one{#a}other{#a}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_fr_CA =  {
  DAY: {
    LONG: "one{# jour}other{# jours}",
    SHORT: "one{# j}other{# j}",
    NARROW: "one{#j}other{#j}",
  },
  HOUR: {
    LONG: "one{# heure}other{# heures}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minute}other{# minutes}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# mois}other{# mois}",
    SHORT: "one{# m.}other{# m.}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# seconde}other{# secondes}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# semaine}other{# semaines}",
    SHORT: "one{# sem.}other{# sem.}",
    NARROW: "one{#sem}other{#sem}",
  },
  YEAR: {
    LONG: "one{# an}other{# ans}",
    SHORT: "one{# an}other{# ans}",
    NARROW: "one{#a}other{#a}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ga =  {
  DAY: {
    LONG: "one{# lá}two{# lá}few{# lá}many{# lá}other{# lá}",
    SHORT: "one{# lá}two{# lá}few{# lá}many{# lá}other{# lá}",
    NARROW: "one{#l}two{#l}few{#l}many{#l}other{#l}",
  },
  HOUR: {
    LONG: "one{# uair}two{# uair}few{# huaire}many{# n-uaire}other{# uair}",
    SHORT: "one{# u}two{# u}few{# u}many{# u}other{# u}",
    NARROW: "one{#u}two{#u}few{#u}many{#u}other{#u}",
  },
  MINUTE: {
    LONG: "one{# nóiméad}two{# nóiméad}few{# nóiméad}many{# nóiméad}other{# nóiméad}",
    SHORT: "one{# nóim}two{# nóim}few{# nóim}many{# nóim}other{# nóim}",
    NARROW: "one{#n}two{#n}few{#n}many{#n}other{#n}",
  },
  MONTH: {
    LONG: "one{# mhí}two{# mhí}few{# mhí}many{# mí}other{# mí}",
    SHORT: "one{# mhí}two{# mhí}few{# mhí}many{# mí}other{# mí}",
    NARROW: "one{#m}two{#m}few{#m}many{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# soicind}two{# shoicind}few{# shoicind}many{# soicind}other{# soicind}",
    SHORT: "one{# soic}two{# shoic}few{# shoic}many{# soic}other{# soic}",
    NARROW: "one{#s}two{#s}few{#s}many{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# seachtain}two{# sheachtain}few{# seachtaine}many{# seachtaine}other{# seachtain}",
    SHORT: "one{# scht}two{# scht}few{# scht}many{# scht}other{# scht}",
    NARROW: "one{#s}two{#s}few{#s}many{#s}other{#s}",
  },
  YEAR: {
    LONG: "one{# bhliain}two{# bhliain}few{# bliana}many{# mbliana}other{# bliain}",
    SHORT: "one{# bhl}two{# bhl}few{# bl}many{# mbl}other{# bl}",
    NARROW: "one{#b}two{#b}few{#b}many{#b}other{#b}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_gl =  {
  DAY: {
    LONG: "one{# día}other{# días}",
    SHORT: "one{# día}other{# días}",
    NARROW: "one{# d}other{# d}",
  },
  HOUR: {
    LONG: "one{# hora}other{# horas}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# minutos}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{# min}other{# min}",
  },
  MONTH: {
    LONG: "one{# mes}other{# meses}",
    SHORT: "one{# mes}other{# meses}",
    NARROW: "one{# m.}other{# m.}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# segundos}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# semana}other{# semanas}",
    SHORT: "one{# sem.}other{# sem.}",
    NARROW: "one{# sem.}other{# sem.}",
  },
  YEAR: {
    LONG: "one{# ano}other{# anos}",
    SHORT: "one{# ano}other{# anos}",
    NARROW: "one{# a.}other{# a.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_gsw =  {
  DAY: {
    LONG: "one{# Taag}other{# Tääg}",
    SHORT: "other{# d}",
    NARROW: "other{# d}",
  },
  HOUR: {
    LONG: "one{# Schtund}other{# Schtunde}",
    SHORT: "other{# h}",
    NARROW: "other{# h}",
  },
  MINUTE: {
    LONG: "one{# Minuute}other{# Minuute}",
    SHORT: "other{# min}",
    NARROW: "other{# min}",
  },
  MONTH: {
    LONG: "one{# Monet}other{# Mönet}",
    SHORT: "other{# m}",
    NARROW: "other{# m}",
  },
  SECOND: {
    LONG: "one{# Sekunde}other{# Sekunde}",
    SHORT: "other{# s}",
    NARROW: "other{# s}",
  },
  WEEK: {
    LONG: "one{# Wuche}other{# Wuche}",
    SHORT: "other{# w}",
    NARROW: "other{# w}",
  },
  YEAR: {
    LONG: "one{# Jahr}other{# Jahr}",
    SHORT: "other{# y}",
    NARROW: "other{# y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_gu =  {
  DAY: {
    LONG: "one{# દિવસ}other{# દિવસ}",
    SHORT: "one{# દિવસ}other{# દિવસ}",
    NARROW: "one{# દિ}other{# દિ}",
  },
  HOUR: {
    LONG: "one{# કલાક}other{# કલાક}",
    SHORT: "one{# કલાક}other{# કલાક}",
    NARROW: "one{# ક}other{# ક}",
  },
  MINUTE: {
    LONG: "one{# મિનિટ}other{# મિનિટ}",
    SHORT: "one{# મિનિટ}other{# મિનિટ}",
    NARROW: "one{# મિ}other{# મિ}",
  },
  MONTH: {
    LONG: "one{# મહિનો}other{# મહિના}",
    SHORT: "one{# મહિનો}other{# મહિના}",
    NARROW: "one{# મ}other{# મ}",
  },
  SECOND: {
    LONG: "one{# સેકંડ}other{# સેકંડ}",
    SHORT: "one{# સેકંડ}other{# સેકંડ}",
    NARROW: "one{# સે}other{# સે}",
  },
  WEEK: {
    LONG: "one{# અઠવાડિયું}other{# અઠવાડિયા}",
    SHORT: "one{# અઠ.}other{# અઠ.}",
    NARROW: "one{# અઠ.}other{# અઠ.}",
  },
  YEAR: {
    LONG: "one{# વર્ષ}other{# વર્ષ}",
    SHORT: "one{# વર્ષ}other{# વર્ષ}",
    NARROW: "one{# વ}other{# વ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_haw =  {
  DAY: {
    LONG: "one{# lā}other{# lā}",
    SHORT: "other{# d}",
    NARROW: "other{# d}",
  },
  HOUR: {
    LONG: "one{# hola}other{# hola}",
    SHORT: "other{# h}",
    NARROW: "other{# h}",
  },
  MINUTE: {
    LONG: "one{# minuke}other{# minuke}",
    SHORT: "other{# min}",
    NARROW: "other{# min}",
  },
  MONTH: {
    LONG: "one{# mahina}other{# mahina}",
    SHORT: "other{# m}",
    NARROW: "other{# m}",
  },
  SECOND: {
    LONG: "one{# kekona}other{# kekona}",
    SHORT: "other{# s}",
    NARROW: "other{# s}",
  },
  WEEK: {
    LONG: "one{# pule}other{# pule}",
    SHORT: "other{# w}",
    NARROW: "other{# w}",
  },
  YEAR: {
    LONG: "one{# makahiki}other{# makahiki}",
    SHORT: "other{# y}",
    NARROW: "other{# y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_he =  {
  DAY: {
    LONG: "one{יום #}two{יומיים}many{# יום}other{# ימים}",
    SHORT: "one{יום}two{יומיים}many{# ימ׳}other{# ימ׳}",
    NARROW: "one{י׳}two{# י׳}many{# י׳}other{# י׳}",
  },
  HOUR: {
    LONG: "one{שעה}two{שעתיים}many{# שעות}other{# שעות}",
    SHORT: "one{שעה}two{שעתיים}many{# שע׳}other{# שע׳}",
    NARROW: "one{שעה #}two{# שע׳}many{# שע׳}other{# שע׳}",
  },
  MINUTE: {
    LONG: "one{דקה}two{שתי דקות}many{# דקות}other{# דקות}",
    SHORT: "one{דקה}two{שתי דק׳}many{# דק׳}other{# דק׳}",
    NARROW: "one{דקה}two{שתי דק׳}many{# דק׳}other{# דק׳}",
  },
  MONTH: {
    LONG: "one{חודש}two{חודשיים}many{# חודשים}other{# חודשים}",
    SHORT: "one{חודש}two{חודשיים}many{# ח׳}other{# ח׳}",
    NARROW: "one{ח׳ #}two{# ח׳}many{# ח׳}other{# ח׳}",
  },
  SECOND: {
    LONG: "one{שניה}two{שתי שניות}many{‏# שניות}other{# שניות}",
    SHORT: "one{שנ׳}two{שתי שנ׳}many{# שנ׳}other{# שנ׳}",
    NARROW: "one{שניה}two{שתי שנ׳}many{# שנ׳}other{# שנ׳}",
  },
  WEEK: {
    LONG: "one{שבוע}two{שבועיים}many{# שבועות}other{# שבועות}",
    SHORT: "one{שבוע #}two{שבועיים}many{# שבועות}other{# שבועות}",
    NARROW: "one{ש′ #}two{# ש′}many{# ש′}other{# ש′}",
  },
  YEAR: {
    LONG: "one{שנה}two{שנתיים}many{# שנים}other{# שנים}",
    SHORT: "one{שנה #}two{# שנים}many{# שנים}other{# שנים}",
    NARROW: "one{ש′ #}two{# ש′}many{# ש′}other{# ש′}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_hi =  {
  DAY: {
    LONG: "one{# दिन}other{# दिन}",
    SHORT: "one{# दिन}other{# दिन}",
    NARROW: "one{# दि}other{# दि}",
  },
  HOUR: {
    LONG: "one{# घंटा}other{# घंटे}",
    SHORT: "one{# घं॰}other{# घं॰}",
    NARROW: "one{#घं॰}other{# घं}",
  },
  MINUTE: {
    LONG: "one{# मिनट}other{# मिनट}",
    SHORT: "one{# मि॰}other{# मि॰}",
    NARROW: "one{# मि}other{# मि}",
  },
  MONTH: {
    LONG: "one{# महीना}other{# महीने}",
    SHORT: "one{# माह}other{# माह}",
    NARROW: "one{#माह}other{#माह}",
  },
  SECOND: {
    LONG: "one{# सेकंड}other{# सेकंड}",
    SHORT: "one{# से॰}other{# से॰}",
    NARROW: "one{# से}other{# से}",
  },
  WEEK: {
    LONG: "one{# सप्ताह}other{# सप्ताह}",
    SHORT: "one{# सप्ताह}other{# सप्ताह}",
    NARROW: "one{# सप्ताह}other{# सप्ताह}",
  },
  YEAR: {
    LONG: "one{# वर्ष}other{# वर्ष}",
    SHORT: "one{# वर्ष}other{# वर्ष}",
    NARROW: "one{#वर्ष}other{#वर्ष}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_hr =  {
  DAY: {
    LONG: "one{# dan}few{# dana}other{# dana}",
    SHORT: "one{# dan}few{# dana}other{# dana}",
    NARROW: "one{# d.}few{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "one{# sat}few{# sata}other{# sati}",
    SHORT: "one{# h}few{# h}other{# h}",
    NARROW: "one{# h}few{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minuta}few{# minute}other{# minuta}",
    SHORT: "one{# min}few{# min}other{# min}",
    NARROW: "one{# m}few{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# mjesec}few{# mjeseca}other{# mjeseci}",
    SHORT: "one{# mj.}few{# mj.}other{# mj.}",
    NARROW: "one{# mj.}few{# mj.}other{# mj.}",
  },
  SECOND: {
    LONG: "one{# sekunda}few{# sekunde}other{# sekundi}",
    SHORT: "one{# s}few{# s}other{# s}",
    NARROW: "one{# s}few{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# tjedan}few{# tjedna}other{# tjedana}",
    SHORT: "one{# tj.}few{# tj.}other{# tj.}",
    NARROW: "one{# tj.}few{# tj.}other{# tj.}",
  },
  YEAR: {
    LONG: "one{# godina}few{# godine}other{# godina}",
    SHORT: "one{# g.}few{# g.}other{# g.}",
    NARROW: "one{# g.}few{# g.}other{# g.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_hu =  {
  DAY: {
    LONG: "one{# nap}other{# nap}",
    SHORT: "one{# nap}other{# nap}",
    NARROW: "one{# nap}other{# nap}",
  },
  HOUR: {
    LONG: "one{# óra}other{# óra}",
    SHORT: "one{# ó}other{# ó}",
    NARROW: "one{# ó}other{# ó}",
  },
  MINUTE: {
    LONG: "one{# perc}other{# perc}",
    SHORT: "one{# p}other{# p}",
    NARROW: "one{# p}other{# p}",
  },
  MONTH: {
    LONG: "one{# hónap}other{# hónap}",
    SHORT: "one{# hónap}other{# hónap}",
    NARROW: "one{# h.}other{# h.}",
  },
  SECOND: {
    LONG: "one{# másodperc}other{# másodperc}",
    SHORT: "one{# mp}other{# mp}",
    NARROW: "one{# mp}other{# mp}",
  },
  WEEK: {
    LONG: "one{# hét}other{# hét}",
    SHORT: "one{# hét}other{# hét}",
    NARROW: "one{# hét}other{# hét}",
  },
  YEAR: {
    LONG: "one{# év}other{# év}",
    SHORT: "one{# év}other{# év}",
    NARROW: "one{# év}other{# év}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_hy =  {
  DAY: {
    LONG: "one{# օր}other{# օր}",
    SHORT: "one{# օր}other{# օր}",
    NARROW: "one{# օ}other{# օ}",
  },
  HOUR: {
    LONG: "one{# ժամ}other{# ժամ}",
    SHORT: "one{# ժ}other{# ժ}",
    NARROW: "one{# ժ}other{# ժ}",
  },
  MINUTE: {
    LONG: "one{# րոպե}other{# րոպե}",
    SHORT: "one{# ր}other{# ր}",
    NARROW: "one{# ր}other{# ր}",
  },
  MONTH: {
    LONG: "one{# ամիս}other{# ամիս}",
    SHORT: "one{# ամս}other{# ամս}",
    NARROW: "one{# ա}other{# ա}",
  },
  SECOND: {
    LONG: "one{# վայրկյան}other{# վայրկյան}",
    SHORT: "one{# վրկ}other{# վրկ}",
    NARROW: "one{# վ}other{# վ}",
  },
  WEEK: {
    LONG: "one{# շաբաթ}other{# շաբաթ}",
    SHORT: "one{# շաբ}other{# շաբ}",
    NARROW: "one{# շ}other{# շ}",
  },
  YEAR: {
    LONG: "one{# տարի}other{# տարի}",
    SHORT: "one{# տ}other{# տ}",
    NARROW: "one{# տ}other{# տ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_id =  {
  DAY: {
    LONG: "other{# hari}",
    SHORT: "other{# hr}",
    NARROW: "other{# hr}",
  },
  HOUR: {
    LONG: "other{# jam}",
    SHORT: "other{# j}",
    NARROW: "other{# j}",
  },
  MINUTE: {
    LONG: "other{# menit}",
    SHORT: "other{# mnt}",
    NARROW: "other{# mnt}",
  },
  MONTH: {
    LONG: "other{# bulan}",
    SHORT: "other{# bln}",
    NARROW: "other{# bln}",
  },
  SECOND: {
    LONG: "other{# detik}",
    SHORT: "other{# dtk}",
    NARROW: "other{# dtk}",
  },
  WEEK: {
    LONG: "other{# minggu}",
    SHORT: "other{# mgg}",
    NARROW: "other{# mgg}",
  },
  YEAR: {
    LONG: "other{# tahun}",
    SHORT: "other{# thn}",
    NARROW: "other{# thn}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_in =  {
  DAY: {
    LONG: "other{# hari}",
    SHORT: "other{# hr}",
    NARROW: "other{# hr}",
  },
  HOUR: {
    LONG: "other{# jam}",
    SHORT: "other{# j}",
    NARROW: "other{# j}",
  },
  MINUTE: {
    LONG: "other{# menit}",
    SHORT: "other{# mnt}",
    NARROW: "other{# mnt}",
  },
  MONTH: {
    LONG: "other{# bulan}",
    SHORT: "other{# bln}",
    NARROW: "other{# bln}",
  },
  SECOND: {
    LONG: "other{# detik}",
    SHORT: "other{# dtk}",
    NARROW: "other{# dtk}",
  },
  WEEK: {
    LONG: "other{# minggu}",
    SHORT: "other{# mgg}",
    NARROW: "other{# mgg}",
  },
  YEAR: {
    LONG: "other{# tahun}",
    SHORT: "other{# thn}",
    NARROW: "other{# thn}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_is =  {
  DAY: {
    LONG: "one{# dagur}other{# dagar}",
    SHORT: "one{# dagur}other{# dagar}",
    NARROW: "one{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "one{# klukkustund}other{# klukkustundir}",
    SHORT: "one{# klst.}other{# klst.}",
    NARROW: "one{# klst.}other{# klst.}",
  },
  MINUTE: {
    LONG: "one{# mínúta}other{# mínútur}",
    SHORT: "one{# mín.}other{# mín.}",
    NARROW: "one{# mín.}other{# mín.}",
  },
  MONTH: {
    LONG: "one{# mánuður}other{# mánuðir}",
    SHORT: "one{# mán.}other{# mán.}",
    NARROW: "one{# mán.}other{# mán.}",
  },
  SECOND: {
    LONG: "one{# sekúnda}other{# sekúndur}",
    SHORT: "one{# sek.}other{# sek.}",
    NARROW: "one{# sek.}other{# sek.}",
  },
  WEEK: {
    LONG: "one{# vika}other{# vikur}",
    SHORT: "one{# vika}other{# vikur}",
    NARROW: "one{# v.}other{# v.}",
  },
  YEAR: {
    LONG: "one{# ár}other{# ár}",
    SHORT: "one{# ár}other{# ár}",
    NARROW: "one{#á}other{#á}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_it =  {
  DAY: {
    LONG: "one{# giorno}other{# giorni}",
    SHORT: "one{# giorno}other{# giorni}",
    NARROW: "one{#g}other{#gg}",
  },
  HOUR: {
    LONG: "one{# ora}other{# ore}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# minuti}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#min}other{#min}",
  },
  MONTH: {
    LONG: "one{# mese}other{# mesi}",
    SHORT: "one{# mese}other{# mesi}",
    NARROW: "one{# mese}other{# mesi}",
  },
  SECOND: {
    LONG: "one{# secondo}other{# secondi}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# settimana}other{# settimane}",
    SHORT: "one{# sett.}other{# sett.}",
    NARROW: "one{#sett.}other{#sett.}",
  },
  YEAR: {
    LONG: "one{# anno}other{# anni}",
    SHORT: "one{# anno}other{# anni}",
    NARROW: "one{#anno}other{#anni}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_iw =  {
  DAY: {
    LONG: "one{יום #}two{יומיים}many{# יום}other{# ימים}",
    SHORT: "one{יום}two{יומיים}many{# ימ׳}other{# ימ׳}",
    NARROW: "one{י׳}two{# י׳}many{# י׳}other{# י׳}",
  },
  HOUR: {
    LONG: "one{שעה}two{שעתיים}many{# שעות}other{# שעות}",
    SHORT: "one{שעה}two{שעתיים}many{# שע׳}other{# שע׳}",
    NARROW: "one{שעה #}two{# שע׳}many{# שע׳}other{# שע׳}",
  },
  MINUTE: {
    LONG: "one{דקה}two{שתי דקות}many{# דקות}other{# דקות}",
    SHORT: "one{דקה}two{שתי דק׳}many{# דק׳}other{# דק׳}",
    NARROW: "one{דקה}two{שתי דק׳}many{# דק׳}other{# דק׳}",
  },
  MONTH: {
    LONG: "one{חודש}two{חודשיים}many{# חודשים}other{# חודשים}",
    SHORT: "one{חודש}two{חודשיים}many{# ח׳}other{# ח׳}",
    NARROW: "one{ח׳ #}two{# ח׳}many{# ח׳}other{# ח׳}",
  },
  SECOND: {
    LONG: "one{שניה}two{שתי שניות}many{‏# שניות}other{# שניות}",
    SHORT: "one{שנ׳}two{שתי שנ׳}many{# שנ׳}other{# שנ׳}",
    NARROW: "one{שניה}two{שתי שנ׳}many{# שנ׳}other{# שנ׳}",
  },
  WEEK: {
    LONG: "one{שבוע}two{שבועיים}many{# שבועות}other{# שבועות}",
    SHORT: "one{שבוע #}two{שבועיים}many{# שבועות}other{# שבועות}",
    NARROW: "one{ש′ #}two{# ש′}many{# ש′}other{# ש′}",
  },
  YEAR: {
    LONG: "one{שנה}two{שנתיים}many{# שנים}other{# שנים}",
    SHORT: "one{שנה #}two{# שנים}many{# שנים}other{# שנים}",
    NARROW: "one{ש′ #}two{# ש′}many{# ש′}other{# ש′}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ja =  {
  DAY: {
    LONG: "other{# 日}",
    SHORT: "other{# 日}",
    NARROW: "other{#d}",
  },
  HOUR: {
    LONG: "other{# 時間}",
    SHORT: "other{# 時間}",
    NARROW: "other{#h}",
  },
  MINUTE: {
    LONG: "other{# 分}",
    SHORT: "other{# 分}",
    NARROW: "other{#m}",
  },
  MONTH: {
    LONG: "other{# か月}",
    SHORT: "other{# か月}",
    NARROW: "other{#m}",
  },
  SECOND: {
    LONG: "other{# 秒}",
    SHORT: "other{# 秒}",
    NARROW: "other{#s}",
  },
  WEEK: {
    LONG: "other{# 週間}",
    SHORT: "other{# 週間}",
    NARROW: "other{#w}",
  },
  YEAR: {
    LONG: "other{# 年}",
    SHORT: "other{# 年}",
    NARROW: "other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ka =  {
  DAY: {
    LONG: "one{# დღე}other{# დღე}",
    SHORT: "one{# დღე}other{# დღე}",
    NARROW: "one{# დღე}other{# დღე}",
  },
  HOUR: {
    LONG: "one{# საათი}other{# საათი}",
    SHORT: "one{# სთ}other{# სთ}",
    NARROW: "one{#სთ}other{#სთ}",
  },
  MINUTE: {
    LONG: "one{# წუთი}other{# წუთი}",
    SHORT: "one{# წთ}other{# წთ}",
    NARROW: "one{#წთ}other{#წთ}",
  },
  MONTH: {
    LONG: "one{# თვე}other{# თვე}",
    SHORT: "one{# თვე}other{# თვე}",
    NARROW: "one{# თვე}other{# თვე}",
  },
  SECOND: {
    LONG: "one{# წამი}other{# წამი}",
    SHORT: "one{# წმ}other{# წმ}",
    NARROW: "one{#წმ}other{#წმ}",
  },
  WEEK: {
    LONG: "one{# კვირა}other{# კვირა}",
    SHORT: "one{# კვრ}other{# კვრ}",
    NARROW: "one{# კვრ}other{# კვრ}",
  },
  YEAR: {
    LONG: "one{# წელი}other{# წელი}",
    SHORT: "one{# წ}other{# წ}",
    NARROW: "one{# წ}other{# წ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_kk =  {
  DAY: {
    LONG: "one{# күн}other{# күн}",
    SHORT: "one{# күн}other{# күн}",
    NARROW: "one{# к.}other{# к.}",
  },
  HOUR: {
    LONG: "one{# сағат}other{# сағат}",
    SHORT: "one{# сағ}other{# сағ}",
    NARROW: "one{# сағ}other{# сағ}",
  },
  MINUTE: {
    LONG: "one{# минут}other{# минут}",
    SHORT: "one{# мин}other{# мин}",
    NARROW: "one{# мин}other{# мин}",
  },
  MONTH: {
    LONG: "one{# ай}other{# ай}",
    SHORT: "one{# ай}other{# ай}",
    NARROW: "one{# ай}other{# ай}",
  },
  SECOND: {
    LONG: "one{# секунд}other{# секунд}",
    SHORT: "one{# с}other{# с}",
    NARROW: "one{# с}other{# с}",
  },
  WEEK: {
    LONG: "one{# апта}other{# апта}",
    SHORT: "one{# ап.}other{# ап.}",
    NARROW: "one{# ап.}other{# ап.}",
  },
  YEAR: {
    LONG: "one{# жыл}other{# жыл}",
    SHORT: "one{# ж.}other{# ж.}",
    NARROW: "one{# ж.}other{# ж.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_km =  {
  DAY: {
    LONG: "other{# ថ្ងៃ}",
    SHORT: "other{# ថ្ងៃ}",
    NARROW: "other{# ថ្ងៃ}",
  },
  HOUR: {
    LONG: "other{# ម៉ោង}",
    SHORT: "other{# ម៉ោង}",
    NARROW: "other{# ម៉ោង}",
  },
  MINUTE: {
    LONG: "other{# នាទី}",
    SHORT: "other{# នាទី}",
    NARROW: "other{# នាទី}",
  },
  MONTH: {
    LONG: "other{# ខែ}",
    SHORT: "other{# ខែ}",
    NARROW: "other{# ខែ}",
  },
  SECOND: {
    LONG: "other{# វិនាទី}",
    SHORT: "other{# វិនាទី}",
    NARROW: "other{# វិនាទី}",
  },
  WEEK: {
    LONG: "other{# សប្ដាហ៍}",
    SHORT: "other{# សប្ដាហ៍}",
    NARROW: "other{# សប្ដាហ៍}",
  },
  YEAR: {
    LONG: "other{# ឆ្នាំ}",
    SHORT: "other{# ឆ្នាំ}",
    NARROW: "other{# ឆ្នាំ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_kn =  {
  DAY: {
    LONG: "one{# ದಿನವು}other{# ದಿನಗಳು}",
    SHORT: "one{# ದಿನ}other{# ದಿನಗಳು}",
    NARROW: "one{#ದಿ}other{#ದಿ}",
  },
  HOUR: {
    LONG: "one{# ಗಂಟೆಯು}other{# ಗಂಟೆಗಳು}",
    SHORT: "one{# ಗಂ.}other{# ಗಂ.}",
    NARROW: "one{#ಗಂ.}other{#ಗಂ.}",
  },
  MINUTE: {
    LONG: "one{# ನಿಮಿಷವು}other{# ನಿಮಿಷಗಳು}",
    SHORT: "one{# ನಿಮಿ}other{# ನಿಮಿ}",
    NARROW: "one{#ನಿಮಿ}other{#ನಿಮಿ}",
  },
  MONTH: {
    LONG: "one{# ತಿಂಗಳು}other{# ತಿಂಗಳು}",
    SHORT: "one{# ತಿಂ.}other{# ತಿಂ.ಗಳು}",
    NARROW: "one{#ತಿಂ.}other{#ತಿಂ.}",
  },
  SECOND: {
    LONG: "one{# ಸೆಕೆಂಡ್}other{# ಸೆಕೆಂಡುಗಳು}",
    SHORT: "one{# ಸೆಕೆಂ}other{# ಸೆಕೆಂ}",
    NARROW: "one{#ಸೆಕೆಂ}other{# ಸೆಕೆಂ}",
  },
  WEEK: {
    LONG: "one{# ವಾರವು}other{# ವಾರಗಳು}",
    SHORT: "one{# ವಾರ}other{# ವಾರಗಳು}",
    NARROW: "one{#ವಾ}other{#ವಾ}",
  },
  YEAR: {
    LONG: "one{# ವರ್ಷವು}other{# ವರ್ಷಗಳು}",
    SHORT: "one{# ವರ್ಷ}other{# ವರ್ಷಗಳು}",
    NARROW: "one{#ವ}other{#ವ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ko =  {
  DAY: {
    LONG: "other{#일}",
    SHORT: "other{#일}",
    NARROW: "other{#일}",
  },
  HOUR: {
    LONG: "other{#시간}",
    SHORT: "other{#시간}",
    NARROW: "other{#시간}",
  },
  MINUTE: {
    LONG: "other{#분}",
    SHORT: "other{#분}",
    NARROW: "other{#분}",
  },
  MONTH: {
    LONG: "other{#개월}",
    SHORT: "other{#개월}",
    NARROW: "other{#개월}",
  },
  SECOND: {
    LONG: "other{#초}",
    SHORT: "other{#초}",
    NARROW: "other{#초}",
  },
  WEEK: {
    LONG: "other{#주}",
    SHORT: "other{#주}",
    NARROW: "other{#주}",
  },
  YEAR: {
    LONG: "other{#년}",
    SHORT: "other{#년}",
    NARROW: "other{#년}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ky =  {
  DAY: {
    LONG: "one{# күн}other{# күн}",
    SHORT: "one{# күн}other{# күн}",
    NARROW: "one{# кн}other{# кн}",
  },
  HOUR: {
    LONG: "one{# саат}other{# саат}",
    SHORT: "one{# ст}other{# ст}",
    NARROW: "one{# ст}other{# ст}",
  },
  MINUTE: {
    LONG: "one{# мүнөт}other{# мүнөт}",
    SHORT: "one{# мүн}other{# мүн}",
    NARROW: "one{# мүн}other{# мүн}",
  },
  MONTH: {
    LONG: "one{# ай}other{# ай}",
    SHORT: "one{# ай}other{# ай}",
    NARROW: "one{# ай}other{# ай}",
  },
  SECOND: {
    LONG: "one{# секунд}other{# секунд}",
    SHORT: "one{# сек}other{# сек}",
    NARROW: "one{# сек}other{# сек}",
  },
  WEEK: {
    LONG: "one{# апта}other{# апта}",
    SHORT: "one{# апт}other{# апт}",
    NARROW: "one{# ап}other{# ап}",
  },
  YEAR: {
    LONG: "one{# жыл}other{# жыл}",
    SHORT: "one{#-ж.}other{# ж.}",
    NARROW: "one{# ж.}other{# ж.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ln =  {
  DAY: {
    LONG: "other{# d}",
    SHORT: "other{# d}",
    NARROW: "other{# d}",
  },
  HOUR: {
    LONG: "other{# h}",
    SHORT: "other{# h}",
    NARROW: "other{# h}",
  },
  MINUTE: {
    LONG: "other{# min}",
    SHORT: "other{# min}",
    NARROW: "other{# min}",
  },
  MONTH: {
    LONG: "other{# m}",
    SHORT: "other{# m}",
    NARROW: "other{# m}",
  },
  SECOND: {
    LONG: "other{# s}",
    SHORT: "other{# s}",
    NARROW: "other{# s}",
  },
  WEEK: {
    LONG: "other{# w}",
    SHORT: "other{# w}",
    NARROW: "other{# w}",
  },
  YEAR: {
    LONG: "other{# y}",
    SHORT: "other{# y}",
    NARROW: "other{# y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_lo =  {
  DAY: {
    LONG: "other{# ມື້}",
    SHORT: "other{# ມື້}",
    NARROW: "other{# ມ.}",
  },
  HOUR: {
    LONG: "other{# ຊົ່ວໂມງ}",
    SHORT: "other{# ຊມ}",
    NARROW: "other{# ຊມ}",
  },
  MINUTE: {
    LONG: "other{# ນາທີ}",
    SHORT: "other{# ນທ}",
    NARROW: "other{# ນທ}",
  },
  MONTH: {
    LONG: "other{# ເດືອນ}",
    SHORT: "other{# ດ.}",
    NARROW: "other{# ດ.}",
  },
  SECOND: {
    LONG: "other{# ວິນາທີ}",
    SHORT: "other{# ວິ}",
    NARROW: "other{# ວິ}",
  },
  WEEK: {
    LONG: "other{# ອາທິດ}",
    SHORT: "other{# ອທ.}",
    NARROW: "other{# ອທ.}",
  },
  YEAR: {
    LONG: "other{# ປີ}",
    SHORT: "other{# ປີ}",
    NARROW: "other{# ປ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_lt =  {
  DAY: {
    LONG: "one{# diena}few{# dienos}many{# dienos}other{# dienų}",
    SHORT: "one{# d.}few{# d.}many{# d.}other{# d.}",
    NARROW: "one{# d.}few{# d.}many{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "one{# valanda}few{# valandos}many{# valandos}other{# valandų}",
    SHORT: "one{# val.}few{# val.}many{# val.}other{# val.}",
    NARROW: "one{# h}few{# h}many{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minutė}few{# minutės}many{# minutės}other{# minučių}",
    SHORT: "one{# min.}few{# min.}many{# min.}other{# min.}",
    NARROW: "one{# min.}few{# min.}many{# min.}other{# min.}",
  },
  MONTH: {
    LONG: "one{# mėnuo}few{# mėnesiai}many{# mėnesio}other{# mėnesių}",
    SHORT: "one{# mėn.}few{# mėn.}many{# mėn.}other{# mėn.}",
    NARROW: "one{# mėn.}few{# mėn.}many{# mėn.}other{# mėn.}",
  },
  SECOND: {
    LONG: "one{# sekundė}few{# sekundės}many{# sekundės}other{# sekundžių}",
    SHORT: "one{# sek.}few{# sek.}many{# sek.}other{# sek.}",
    NARROW: "one{# s}few{# s}many{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# savaitė}few{# savaitės}many{# savaitės}other{# savaičių}",
    SHORT: "one{# sav.}few{# sav.}many{# sav.}other{# sav.}",
    NARROW: "one{# sav.}few{# sav.}many{# sav.}other{# sav.}",
  },
  YEAR: {
    LONG: "one{# metai}few{# metai}many{# metų}other{# metų}",
    SHORT: "one{# m.}few{# m.}many{# m.}other{# m.}",
    NARROW: "one{# m.}few{# m.}many{# m.}other{# m.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_lv =  {
  DAY: {
    LONG: "zero{# dienu}one{# diena}other{# dienas}",
    SHORT: "zero{# d.}one{# d.}other{# d.}",
    NARROW: "zero{# d.}one{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "zero{# stundu}one{# stunda}other{# stundas}",
    SHORT: "zero{# st.}one{# st.}other{# st.}",
    NARROW: "zero{# h}one{# h}other{# h}",
  },
  MINUTE: {
    LONG: "zero{# minūšu}one{# minūte}other{# minūtes}",
    SHORT: "zero{# min}one{# min}other{# min}",
    NARROW: "zero{# min}one{# min}other{# min}",
  },
  MONTH: {
    LONG: "zero{# mēnešu}one{# mēnesis}other{# mēneši}",
    SHORT: "zero{# mēn.}one{# mēn.}other{# mēn.}",
    NARROW: "zero{# m.}one{# m.}other{# m.}",
  },
  SECOND: {
    LONG: "zero{# sekunžu}one{# sekunde}other{# sekundes}",
    SHORT: "zero{# sek.}one{# sek.}other{# sek.}",
    NARROW: "zero{# s}one{# s}other{# s}",
  },
  WEEK: {
    LONG: "zero{# nedēļu}one{# nedēļa}other{# nedēļas}",
    SHORT: "zero{# ned.}one{# ned.}other{# ned.}",
    NARROW: "zero{# n.}one{# n.}other{# n.}",
  },
  YEAR: {
    LONG: "zero{# gadu}one{# gads}other{# gadi}",
    SHORT: "zero{# g.}one{# g.}other{# g.}",
    NARROW: "zero{# g.}one{# g.}other{# g.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_mk =  {
  DAY: {
    LONG: "one{# ден}other{# дена}",
    SHORT: "one{# ден}other{# дена}",
    NARROW: "one{# д.}other{# д.}",
  },
  HOUR: {
    LONG: "one{# час}other{# часа}",
    SHORT: "one{# ч.}other{# ч.}",
    NARROW: "one{# ч.}other{# ч.}",
  },
  MINUTE: {
    LONG: "one{# минута}other{# минути}",
    SHORT: "one{# мин.}other{# мин.}",
    NARROW: "one{# м.}other{# м.}",
  },
  MONTH: {
    LONG: "one{# месец}other{# месеци}",
    SHORT: "one{# мес.}other{# мес.}",
    NARROW: "one{# м.}other{# м.}",
  },
  SECOND: {
    LONG: "one{# секунда}other{# секунди}",
    SHORT: "one{# сек.}other{# сек.}",
    NARROW: "one{# с.}other{# с.}",
  },
  WEEK: {
    LONG: "one{# седмица}other{# седмици}",
    SHORT: "one{# сед.}other{# сед.}",
    NARROW: "one{# с.}other{# с.}",
  },
  YEAR: {
    LONG: "one{# година}other{# години}",
    SHORT: "one{# год.}other{# год.}",
    NARROW: "one{# г.}other{# г.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ml =  {
  DAY: {
    LONG: "one{# ദിവസം}other{# ദിവസം}",
    SHORT: "one{# ദിവസം‌}other{# ദിവസം‌}",
    NARROW: "one{# ദി}other{# ദി}",
  },
  HOUR: {
    LONG: "one{# മണിക്കൂർ}other{# മണിക്കൂർ}",
    SHORT: "one{# മ}other{# മ}",
    NARROW: "one{# മ}other{# മ}",
  },
  MINUTE: {
    LONG: "one{# മിനിറ്റ്}other{# മിനിറ്റ്}",
    SHORT: "one{# മി.}other{# മി.}",
    NARROW: "one{# മി.}other{# മി.}",
  },
  MONTH: {
    LONG: "one{# മാസം}other{# മാസം}",
    SHORT: "one{# മാസം}other{# മാസം}",
    NARROW: "one{# മാ}other{# മാ}",
  },
  SECOND: {
    LONG: "one{# സെക്കൻഡ്}other{# സെക്കൻഡ്}",
    SHORT: "one{# സെ.}other{# സെ.}",
    NARROW: "one{# സെ.}other{# സെ.}",
  },
  WEEK: {
    LONG: "one{# ആഴ്ച}other{# ആഴ്ച}",
    SHORT: "one{# ആ}other{# ആ}",
    NARROW: "one{# ആ}other{# ആ}",
  },
  YEAR: {
    LONG: "one{# വർഷം}other{# വർഷം}",
    SHORT: "one{# വ}other{# വ}",
    NARROW: "one{# വ}other{# വ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_mn =  {
  DAY: {
    LONG: "one{# хоног}other{# хоног}",
    SHORT: "one{# хоног}other{# хоног}",
    NARROW: "one{# хоног}other{# хоног}",
  },
  HOUR: {
    LONG: "one{# цаг}other{# цаг}",
    SHORT: "one{# цаг}other{# цаг}",
    NARROW: "one{# ц}other{# ц}",
  },
  MINUTE: {
    LONG: "one{# минут}other{# минут}",
    SHORT: "one{# мин}other{# мин}",
    NARROW: "one{# мин}other{# мин}",
  },
  MONTH: {
    LONG: "one{# сар}other{# сар}",
    SHORT: "one{# сар}other{# сар}",
    NARROW: "one{#с}other{#с}",
  },
  SECOND: {
    LONG: "one{# секунд}other{# секунд}",
    SHORT: "one{# сек}other{# сек}",
    NARROW: "one{# сек}other{# сек}",
  },
  WEEK: {
    LONG: "one{# долоо хоног}other{# долоо хоног}",
    SHORT: "one{# д.х}other{# д.х}",
    NARROW: "one{# д.х}other{# д.х}",
  },
  YEAR: {
    LONG: "one{# жил}other{# жил}",
    SHORT: "one{# жил}other{# жил}",
    NARROW: "one{#ж}other{#ж}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_mo =  {
  DAY: {
    LONG: "one{# zi}few{# zile}other{# de zile}",
    SHORT: "one{# zi}few{# zile}other{# zile}",
    NARROW: "one{# z}few{# z}other{# z}",
  },
  HOUR: {
    LONG: "one{# oră}few{# ore}other{# de ore}",
    SHORT: "one{# oră}few{# ore}other{# ore}",
    NARROW: "one{# h}few{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minut}few{# minute}other{# de minute}",
    SHORT: "one{# min.}few{# min.}other{# min.}",
    NARROW: "one{# m}few{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# lună}few{# luni}other{# de luni}",
    SHORT: "one{# lună}few{# luni}other{# luni}",
    NARROW: "one{# l}few{# l}other{# l}",
  },
  SECOND: {
    LONG: "one{# secundă}few{# secunde}other{# de secunde}",
    SHORT: "one{# s}few{# s}other{# s}",
    NARROW: "one{# s}few{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# săptămână}few{# săptămâni}other{# de săptămâni}",
    SHORT: "one{# săpt.}few{# săpt.}other{# săpt.}",
    NARROW: "one{# săpt.}few{# săpt.}other{# săpt.}",
  },
  YEAR: {
    LONG: "one{# an}few{# ani}other{# de ani}",
    SHORT: "one{# an}few{# ani}other{# ani}",
    NARROW: "one{# a}few{# a}other{# a}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_mr =  {
  DAY: {
    LONG: "one{# दिवस}other{# दिवस}",
    SHORT: "one{# दिवस}other{# दिवस}",
    NARROW: "one{#दि}other{#दि}",
  },
  HOUR: {
    LONG: "one{# तास}other{# तास}",
    SHORT: "one{# ता}other{# ता}",
    NARROW: "one{#ता}other{#ता}",
  },
  MINUTE: {
    LONG: "one{# मिनिट}other{# मिनिटे}",
    SHORT: "one{# मिनि}other{# मिनि}",
    NARROW: "one{#मि}other{#मि}",
  },
  MONTH: {
    LONG: "one{# महिना}other{# महिने}",
    SHORT: "one{# महिना}other{# महिने}",
    NARROW: "one{#म}other{#म}",
  },
  SECOND: {
    LONG: "one{# सेकंद}other{# सेकंद}",
    SHORT: "one{# से}other{# से}",
    NARROW: "one{#से}other{#से}",
  },
  WEEK: {
    LONG: "one{# आठवडा}other{# आठवडे}",
    SHORT: "one{# आ}other{# आ}",
    NARROW: "one{#आ}other{#आ}",
  },
  YEAR: {
    LONG: "one{# वर्ष}other{# वर्षे}",
    SHORT: "one{# वर्ष}other{# वर्षे}",
    NARROW: "one{#व}other{#व}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ms =  {
  DAY: {
    LONG: "other{# hari}",
    SHORT: "other{# hari}",
    NARROW: "other{# h}",
  },
  HOUR: {
    LONG: "other{# jam}",
    SHORT: "other{# j}",
    NARROW: "other{# j}",
  },
  MINUTE: {
    LONG: "other{# minit}",
    SHORT: "other{# min}",
    NARROW: "other{# min}",
  },
  MONTH: {
    LONG: "other{# bulan}",
    SHORT: "other{# bln}",
    NARROW: "other{# bln}",
  },
  SECOND: {
    LONG: "other{# saat}",
    SHORT: "other{# saat}",
    NARROW: "other{# s}",
  },
  WEEK: {
    LONG: "other{# minggu}",
    SHORT: "other{# mgu}",
    NARROW: "other{# mgu}",
  },
  YEAR: {
    LONG: "other{# tahun}",
    SHORT: "other{# thn}",
    NARROW: "other{# thn}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_mt =  {
  DAY: {
    LONG: "other{# d}",
    SHORT: "other{# d}",
    NARROW: "other{# d}",
  },
  HOUR: {
    LONG: "other{# h}",
    SHORT: "other{# h}",
    NARROW: "other{# h}",
  },
  MINUTE: {
    LONG: "other{# min}",
    SHORT: "other{# min}",
    NARROW: "other{# min}",
  },
  MONTH: {
    LONG: "other{# m}",
    SHORT: "other{# m}",
    NARROW: "other{# m}",
  },
  SECOND: {
    LONG: "other{# s}",
    SHORT: "other{# s}",
    NARROW: "other{# s}",
  },
  WEEK: {
    LONG: "other{# w}",
    SHORT: "other{# w}",
    NARROW: "other{# w}",
  },
  YEAR: {
    LONG: "other{# y}",
    SHORT: "other{# y}",
    NARROW: "other{# y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_my =  {
  DAY: {
    LONG: "other{# ရက်}",
    SHORT: "other{# ရက်}",
    NARROW: "other{# ရက်}",
  },
  HOUR: {
    LONG: "other{# နာရီ}",
    SHORT: "other{# နာရီ}",
    NARROW: "other{# နာရီ}",
  },
  MINUTE: {
    LONG: "other{# မိနစ်}",
    SHORT: "other{# မိနစ်}",
    NARROW: "other{# မိနစ်}",
  },
  MONTH: {
    LONG: "other{# လ}",
    SHORT: "other{# လ}",
    NARROW: "other{# လ}",
  },
  SECOND: {
    LONG: "other{# စက္ကန့်}",
    SHORT: "other{# sec}",
    NARROW: "other{# s}",
  },
  WEEK: {
    LONG: "other{# ပတ်}",
    SHORT: "other{# ပတ်}",
    NARROW: "other{# ပတ်}",
  },
  YEAR: {
    LONG: "other{# နှစ်}",
    SHORT: "other{# နှစ်}",
    NARROW: "other{# နှစ်}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_nb =  {
  DAY: {
    LONG: "one{# døgn}other{# døgn}",
    SHORT: "one{# d}other{# d}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# time}other{# timer}",
    SHORT: "one{# t}other{# t}",
    NARROW: "one{#t}other{#t}",
  },
  MINUTE: {
    LONG: "one{# minutt}other{# minutter}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# måned}other{# måneder}",
    SHORT: "one{# md.}other{# md.}",
    NARROW: "one{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# sekund}other{# sekunder}",
    SHORT: "one{# sek}other{# sek}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# uke}other{# uker}",
    SHORT: "one{# u}other{# u}",
    NARROW: "one{#u}other{#u}",
  },
  YEAR: {
    LONG: "one{# år}other{# år}",
    SHORT: "one{# år}other{# år}",
    NARROW: "one{#å}other{#å}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ne =  {
  DAY: {
    LONG: "one{# दिन}other{# दिन}",
    SHORT: "one{# दिन}other{# दिन}",
    NARROW: "one{# दिन}other{# दिन}",
  },
  HOUR: {
    LONG: "one{# घण्टा}other{# घण्टा}",
    SHORT: "one{# घण्टा}other{# घण्टा}",
    NARROW: "one{# घण्टा}other{# घण्टा}",
  },
  MINUTE: {
    LONG: "one{# मिनेट}other{# मिनेट}",
    SHORT: "one{# मिनेट}other{# मिनेट}",
    NARROW: "one{# मिनेट}other{# मिनेट}",
  },
  MONTH: {
    LONG: "one{# महिना}other{# महिना}",
    SHORT: "one{# महिना}other{# महिना}",
    NARROW: "one{# महिना}other{# महिना}",
  },
  SECOND: {
    LONG: "one{# सेकेन्ड}other{# सेकेन्ड}",
    SHORT: "one{# सेकेन्ड}other{# सेकेन्ड}",
    NARROW: "one{# सेकेन्ड}other{# सेकेन्ड}",
  },
  WEEK: {
    LONG: "one{# हप्ता}other{# हप्ता}",
    SHORT: "one{# हप्ता}other{# हप्ता}",
    NARROW: "one{# हप्ता}other{# हप्ता}",
  },
  YEAR: {
    LONG: "one{# वर्ष}other{# वर्ष}",
    SHORT: "one{# वर्ष}other{# वर्ष}",
    NARROW: "one{# वर्ष}other{# वर्ष}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_nl =  {
  DAY: {
    LONG: "one{# dag}other{# dagen}",
    SHORT: "one{# dag}other{# dagen}",
    NARROW: "one{# d}other{# d}",
  },
  HOUR: {
    LONG: "one{# uur}other{# uur}",
    SHORT: "one{# uur}other{# uur}",
    NARROW: "one{# u}other{# u}",
  },
  MINUTE: {
    LONG: "one{# minuut}other{# minuten}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# maand}other{# maanden}",
    SHORT: "one{# mnd}other{# mnd}",
    NARROW: "one{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# seconde}other{# seconden}",
    SHORT: "one{# sec}other{# sec}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# week}other{# weken}",
    SHORT: "one{# wk}other{# wkn}",
    NARROW: "one{# w}other{# w}",
  },
  YEAR: {
    LONG: "one{# jaar}other{# jaar}",
    SHORT: "one{# jr}other{# jr}",
    NARROW: "one{# jr}other{# jr}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_no =  {
  DAY: {
    LONG: "one{# døgn}other{# døgn}",
    SHORT: "one{# d}other{# d}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# time}other{# timer}",
    SHORT: "one{# t}other{# t}",
    NARROW: "one{#t}other{#t}",
  },
  MINUTE: {
    LONG: "one{# minutt}other{# minutter}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# måned}other{# måneder}",
    SHORT: "one{# md.}other{# md.}",
    NARROW: "one{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# sekund}other{# sekunder}",
    SHORT: "one{# sek}other{# sek}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# uke}other{# uker}",
    SHORT: "one{# u}other{# u}",
    NARROW: "one{#u}other{#u}",
  },
  YEAR: {
    LONG: "one{# år}other{# år}",
    SHORT: "one{# år}other{# år}",
    NARROW: "one{#å}other{#å}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_no_NO = exports.DurationSymbols_no;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_or =  {
  DAY: {
    LONG: "one{# ଦିନ}other{# ଦିନ}",
    SHORT: "one{# ଦିନ}other{# ଦିନ}",
    NARROW: "one{#ଦିନ}other{#ଦିନ}",
  },
  HOUR: {
    LONG: "one{# ଘଣ୍ଟା}other{# ଘଣ୍ଟା}",
    SHORT: "one{# ଘଣ୍ଟା}other{# ଘଣ୍ଟା}",
    NARROW: "one{#ଘଣ୍ଟା}other{#ଘଣ୍ଟା}",
  },
  MINUTE: {
    LONG: "one{# ମିନିଟ୍‌}other{# ମିନିଟ୍}",
    SHORT: "one{# ମିନିଟ୍‌}other{# ମିନିଟ୍‌}",
    NARROW: "one{#ମିନିଟ୍‌}other{#ମିନିଟ୍‌}",
  },
  MONTH: {
    LONG: "one{# ମାସ}other{# ମାସ}",
    SHORT: "one{# ମାସ}other{# ମାସ}",
    NARROW: "one{#ମାସ}other{#ମାସ}",
  },
  SECOND: {
    LONG: "one{# ସେକେଣ୍ଡ}other{# ସେକେଣ୍ଡ}",
    SHORT: "one{# ସେକେଣ୍ଡ}other{# ସେକେଣ୍ଡ}",
    NARROW: "one{#ସେକ୍}other{#ସେକ୍}",
  },
  WEEK: {
    LONG: "one{# ସପ୍ତାହ}other{# ସପ୍ତାହ}",
    SHORT: "one{# ସପ୍ତାହ}other{# ସପ୍ତାହ}",
    NARROW: "one{#ସପ୍}other{# ସପ୍}",
  },
  YEAR: {
    LONG: "one{# ବର୍ଷ}other{# ବର୍ଷ}",
    SHORT: "one{# ବର୍ଷ}other{# ବର୍ଷ}",
    NARROW: "one{#ବର୍ଷ}other{#ବର୍ଷ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_pa =  {
  DAY: {
    LONG: "one{# ਦਿਨ}other{# ਦਿਨ}",
    SHORT: "one{# ਦਿਨ}other{# ਦਿਨ}",
    NARROW: "one{# ਦਿਨ}other{# ਦਿਨ}",
  },
  HOUR: {
    LONG: "one{# ਘੰਟਾ}other{# ਘੰਟੇ}",
    SHORT: "one{# ਘੰਟਾ}other{# ਘੰਟੇ}",
    NARROW: "one{# ਘੰਟਾ}other{# ਘੰਟੇ}",
  },
  MINUTE: {
    LONG: "one{# ਮਿੰਟ}other{# ਮਿੰਟ}",
    SHORT: "one{# ਮਿੰਟ}other{# ਮਿੰਟ}",
    NARROW: "one{# ਮਿੰਟ}other{# ਮਿੰਟ}",
  },
  MONTH: {
    LONG: "one{# ਮਹੀਨਾ}other{# ਮਹੀਨੇ}",
    SHORT: "one{# ਮਹੀਨਾ}other{# ਮਹੀਨੇ}",
    NARROW: "one{# ਮਹੀਨਾ}other{# ਮਹੀਨੇ}",
  },
  SECOND: {
    LONG: "one{# ਸਕਿੰਟ}other{# ਸਕਿੰਟ}",
    SHORT: "one{# ਸਕਿੰਟ}other{# ਸਕਿੰਟ}",
    NARROW: "one{# ਸਕਿੰਟ}other{# ਸਕਿੰਟ}",
  },
  WEEK: {
    LONG: "one{# ਹਫ਼ਤਾ}other{# ਹਫ਼ਤੇ}",
    SHORT: "one{# ਹਫ਼ਤਾ}other{# ਹਫ਼ਤੇ}",
    NARROW: "one{# ਹਫ਼ਤਾ}other{# ਹਫ਼ਤੇ}",
  },
  YEAR: {
    LONG: "one{# ਸਾਲ}other{# ਸਾਲ}",
    SHORT: "one{# ਸਾਲ}other{# ਸਾਲ}",
    NARROW: "one{# ਸਾਲ}other{# ਸਾਲ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_pl =  {
  DAY: {
    LONG: "one{# doba}few{# doby}many{# dób}other{# doby}",
    SHORT: "one{# doba}few{# doby}many{# dób}other{# doby}",
    NARROW: "one{# d.}few{# d.}many{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "one{# godzina}few{# godziny}many{# godzin}other{# godziny}",
    SHORT: "one{# godz.}few{# godz.}many{# godz.}other{# godz.}",
    NARROW: "one{# h}few{# h}many{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minuta}few{# minuty}many{# minut}other{# minuty}",
    SHORT: "one{# min}few{# min}many{# min}other{# min}",
    NARROW: "one{# min}few{# min}many{# min}other{# min}",
  },
  MONTH: {
    LONG: "one{# miesiąc}few{# miesiące}many{# miesięcy}other{# miesiąca}",
    SHORT: "one{# mies.}few{# mies.}many{# mies.}other{# mies.}",
    NARROW: "one{# m-c}few{# m-ce}many{# m-cy}other{# m-ca}",
  },
  SECOND: {
    LONG: "one{# sekunda}few{# sekundy}many{# sekund}other{# sekundy}",
    SHORT: "one{# sek.}few{# sek.}many{# sek.}other{# sek.}",
    NARROW: "one{# s}few{# s}many{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# tydzień}few{# tygodnie}many{# tygodni}other{# tygodnia}",
    SHORT: "one{# tydz.}few{# tyg.}many{# tyg.}other{# tyg.}",
    NARROW: "one{# tydz.}few{# t.}many{# tyg.}other{# tyg.}",
  },
  YEAR: {
    LONG: "one{# rok}few{# lata}many{# lat}other{# roku}",
    SHORT: "one{# rok}few{# lata}many{# lat}other{# roku}",
    NARROW: "one{# r.}few{# l.}many{# l.}other{# r.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_pt =  {
  DAY: {
    LONG: "one{# dia}other{# dias}",
    SHORT: "one{# dia}other{# dias}",
    NARROW: "one{# dia}other{# dias}",
  },
  HOUR: {
    LONG: "one{# hora}other{# horas}",
    SHORT: "one{# h}other{# h}",
    NARROW: "one{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# minutos}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{# min}other{# min}",
  },
  MONTH: {
    LONG: "one{# mês}other{# meses}",
    SHORT: "one{# mês}other{# meses}",
    NARROW: "one{# mês}other{# meses}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# segundos}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# semana}other{# semanas}",
    SHORT: "one{# sem.}other{# sem.}",
    NARROW: "one{# sem.}other{# sem.}",
  },
  YEAR: {
    LONG: "one{# ano}other{# anos}",
    SHORT: "one{# ano}other{# anos}",
    NARROW: "one{# ano}other{# anos}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_pt_BR = exports.DurationSymbols_pt;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_pt_PT = exports.DurationSymbols_pt;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ro =  {
  DAY: {
    LONG: "one{# zi}few{# zile}other{# de zile}",
    SHORT: "one{# zi}few{# zile}other{# zile}",
    NARROW: "one{# z}few{# z}other{# z}",
  },
  HOUR: {
    LONG: "one{# oră}few{# ore}other{# de ore}",
    SHORT: "one{# oră}few{# ore}other{# ore}",
    NARROW: "one{# h}few{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minut}few{# minute}other{# de minute}",
    SHORT: "one{# min.}few{# min.}other{# min.}",
    NARROW: "one{# m}few{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# lună}few{# luni}other{# de luni}",
    SHORT: "one{# lună}few{# luni}other{# luni}",
    NARROW: "one{# l}few{# l}other{# l}",
  },
  SECOND: {
    LONG: "one{# secundă}few{# secunde}other{# de secunde}",
    SHORT: "one{# s}few{# s}other{# s}",
    NARROW: "one{# s}few{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# săptămână}few{# săptămâni}other{# de săptămâni}",
    SHORT: "one{# săpt.}few{# săpt.}other{# săpt.}",
    NARROW: "one{# săpt.}few{# săpt.}other{# săpt.}",
  },
  YEAR: {
    LONG: "one{# an}few{# ani}other{# de ani}",
    SHORT: "one{# an}few{# ani}other{# ani}",
    NARROW: "one{# a}few{# a}other{# a}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ru =  {
  DAY: {
    LONG: "one{# день}few{# дня}many{# дней}other{# дня}",
    SHORT: "one{# дн.}few{# дн.}many{# дн.}other{# дн.}",
    NARROW: "one{# д.}few{# д.}many{# д.}other{# д.}",
  },
  HOUR: {
    LONG: "one{# час}few{# часа}many{# часов}other{# часа}",
    SHORT: "one{# ч}few{# ч}many{# ч}other{# ч}",
    NARROW: "one{# ч}few{# ч}many{# ч}other{# ч}",
  },
  MINUTE: {
    LONG: "one{# минута}few{# минуты}many{# минут}other{# минуты}",
    SHORT: "one{# мин}few{# мин}many{# мин}other{# мин}",
    NARROW: "one{# мин}few{# мин}many{# мин}other{# мин}",
  },
  MONTH: {
    LONG: "one{# месяц}few{# месяца}many{# месяцев}other{# месяца}",
    SHORT: "one{# мес.}few{# мес.}many{# мес.}other{# мес.}",
    NARROW: "one{# м.}few{# м.}many{# м.}other{# м.}",
  },
  SECOND: {
    LONG: "one{# секунда}few{# секунды}many{# секунд}other{# секунды}",
    SHORT: "one{# с}few{# с}many{# с}other{# с}",
    NARROW: "one{# с}few{# с}many{# с}other{# с}",
  },
  WEEK: {
    LONG: "one{# неделя}few{# недели}many{# недель}other{# недели}",
    SHORT: "one{# нед.}few{# нед.}many{# нед.}other{# нед.}",
    NARROW: "one{# н.}few{# н.}many{# н.}other{# н.}",
  },
  YEAR: {
    LONG: "one{# год}few{# года}many{# лет}other{# года}",
    SHORT: "one{# г.}few{# г.}many{# л.}other{# г.}",
    NARROW: "one{# г.}few{# г.}many{# л.}other{# г.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_sh =  {
  DAY: {
    LONG: "one{# dan}few{# dana}other{# dana}",
    SHORT: "one{# dan}few{# dana}other{# dana}",
    NARROW: "one{# d}few{# d}other{# d}",
  },
  HOUR: {
    LONG: "one{# sat}few{# sata}other{# sati}",
    SHORT: "one{# sat}few{# sata}other{# sati}",
    NARROW: "one{# č}few{# č}other{# č}",
  },
  MINUTE: {
    LONG: "one{# minut}few{# minuta}other{# minuta}",
    SHORT: "one{# min}few{# min}other{# min}",
    NARROW: "one{# m}few{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# mesec}few{# meseca}other{# meseci}",
    SHORT: "one{# mes.}few{# mes.}other{# mes.}",
    NARROW: "one{# m}few{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# sekunda}few{# sekunde}other{# sekundi}",
    SHORT: "one{# sek}few{# sek}other{# sek}",
    NARROW: "one{# s}few{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# nedelja}few{# nedelje}other{# nedelja}",
    SHORT: "one{# ned.}few{# ned.}other{# ned.}",
    NARROW: "one{# n}few{# n}other{# n}",
  },
  YEAR: {
    LONG: "one{# godina}few{# godine}other{# godina}",
    SHORT: "one{# god}few{# god.}other{# god.}",
    NARROW: "one{# g}few{# g}other{# g}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_si =  {
  DAY: {
    LONG: "one{දින #}other{දින #}",
    SHORT: "one{දින #}other{දින #}",
    NARROW: "one{දි #}other{දි #}",
  },
  HOUR: {
    LONG: "one{පැය #}other{පැය #}",
    SHORT: "one{පැය #}other{පැය #}",
    NARROW: "one{පැය #}other{පැය #}",
  },
  MINUTE: {
    LONG: "one{මිනිත්තු #}other{මිනිත්තු #}",
    SHORT: "one{මිනි #}other{මිනි #}",
    NARROW: "one{මි #}other{මි #}",
  },
  MONTH: {
    LONG: "one{මාස #}other{මාස #}",
    SHORT: "one{මාස #}other{මාස #}",
    NARROW: "one{මා #}other{මා #}",
  },
  SECOND: {
    LONG: "one{තත්පර #}other{තත්පර #}",
    SHORT: "one{තත් #}other{තත් #}",
    NARROW: "one{ත #}other{ත #}",
  },
  WEEK: {
    LONG: "one{සති #}other{සති #}",
    SHORT: "one{සති #}other{සති #}",
    NARROW: "one{ස #}other{ස #}",
  },
  YEAR: {
    LONG: "one{වසර #}other{වසර #}",
    SHORT: "one{වසර #}other{වසර #}",
    NARROW: "one{ව #}other{ව #}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_sk =  {
  DAY: {
    LONG: "one{# deň}few{# dni}many{# dňa}other{# dní}",
    SHORT: "one{# deň}few{# dni}many{# dňa}other{# dní}",
    NARROW: "one{# d.}few{# d.}many{# d.}other{# d.}",
  },
  HOUR: {
    LONG: "one{# hodina}few{# hodiny}many{# hodiny}other{# hodín}",
    SHORT: "one{# h}few{# h}many{# h}other{# h}",
    NARROW: "one{# h}few{# h}many{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minúta}few{# minúty}many{# minúty}other{# minút}",
    SHORT: "one{# min}few{# min}many{# min}other{# min}",
    NARROW: "one{# min}few{# min}many{# min}other{# min}",
  },
  MONTH: {
    LONG: "one{# mesiac}few{# mesiace}many{# mesiaca}other{# mesiacov}",
    SHORT: "one{# mes.}few{# mes.}many{# mes.}other{# mes.}",
    NARROW: "one{# m.}few{# m.}many{# m.}other{# m.}",
  },
  SECOND: {
    LONG: "one{# sekunda}few{# sekundy}many{# sekundy}other{# sekúnd}",
    SHORT: "one{# s}few{# s}many{# s}other{# s}",
    NARROW: "one{# s}few{# s}many{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# týždeň}few{# týždne}many{# týždňa}other{# týždňov}",
    SHORT: "one{# týž.}few{# týž.}many{# týž.}other{# týž.}",
    NARROW: "one{# t.}few{# t.}many{# t.}other{# t.}",
  },
  YEAR: {
    LONG: "one{# rok}few{# roky}many{# roka}other{# rokov}",
    SHORT: "one{# r.}few{# r.}many{# r.}other{# r.}",
    NARROW: "one{# r.}few{# r.}many{# r.}other{# r.}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_sl =  {
  DAY: {
    LONG: "one{# dan}two{# dneva}few{# dni}other{# dni}",
    SHORT: "one{# d}two{# d}few{# d}other{# d}",
    NARROW: "one{# d}two{# d}few{# d}other{# d}",
  },
  HOUR: {
    LONG: "one{# ura}two{# uri}few{# ure}other{# ur}",
    SHORT: "one{# h}two{# h}few{# h}other{# h}",
    NARROW: "one{# h}two{# h}few{# h}other{# h}",
  },
  MINUTE: {
    LONG: "one{# minuta}two{# minuti}few{# minute}other{# minut}",
    SHORT: "one{# min}two{# min}few{# min}other{# min}",
    NARROW: "one{# min}two{# min}few{# min}other{# min}",
  },
  MONTH: {
    LONG: "one{# mesec}two{# meseca}few{# meseci}other{# mesecev}",
    SHORT: "one{# m}two{# m}few{# m}other{# m}",
    NARROW: "one{# m}two{# m}few{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# sekunda}two{# sekundi}few{# sekunde}other{# sekund}",
    SHORT: "one{# sek.}two{# sek.}few{# sek.}other{# sek.}",
    NARROW: "one{# s}two{# s}few{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# teden}two{# tedna}few{# tedni}other{# tednov}",
    SHORT: "one{# t}two{# t}few{# t}other{# t}",
    NARROW: "one{# t}two{# t}few{# t}other{# t}",
  },
  YEAR: {
    LONG: "one{# leto}two{# leti}few{# let}other{# let}",
    SHORT: "one{# l}two{# l}few{# l}other{# l}",
    NARROW: "one{# l}two{# l}few{# l}other{# l}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_sq =  {
  DAY: {
    LONG: "one{# ditë}other{# ditë}",
    SHORT: "one{# ditë}other{# ditë}",
    NARROW: "one{# ditë}other{# ditë}",
  },
  HOUR: {
    LONG: "one{# orë}other{# orë}",
    SHORT: "one{# orë}other{# orë}",
    NARROW: "one{# orë}other{# orë}",
  },
  MINUTE: {
    LONG: "one{# minutë}other{# minuta}",
    SHORT: "one{# min.}other{# min.}",
    NARROW: "one{# min.}other{# min.}",
  },
  MONTH: {
    LONG: "one{# muaj}other{# muaj}",
    SHORT: "one{# muaj}other{# muaj}",
    NARROW: "one{# muaj}other{# muaj}",
  },
  SECOND: {
    LONG: "one{# sekondë}other{# sekonda}",
    SHORT: "one{# sek.}other{# sek.}",
    NARROW: "one{# sek.}other{# sek.}",
  },
  WEEK: {
    LONG: "one{# javë}other{# javë}",
    SHORT: "one{# javë}other{# javë}",
    NARROW: "one{# javë}other{# javë}",
  },
  YEAR: {
    LONG: "one{# vit}other{# vjet}",
    SHORT: "one{# vit}other{# vjet}",
    NARROW: "one{# vit}other{# vjet}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_sr =  {
  DAY: {
    LONG: "one{# дан}few{# дана}other{# дана}",
    SHORT: "one{# дан}few{# дана}other{# дана}",
    NARROW: "one{# д}few{# д}other{# д}",
  },
  HOUR: {
    LONG: "one{# сат}few{# сата}other{# сати}",
    SHORT: "one{# сат}few{# сата}other{# сати}",
    NARROW: "one{# ч}few{# ч}other{# ч}",
  },
  MINUTE: {
    LONG: "one{# минут}few{# минута}other{# минута}",
    SHORT: "one{# мин}few{# мин}other{# мин}",
    NARROW: "one{# м}few{# м}other{# м}",
  },
  MONTH: {
    LONG: "one{# месец}few{# месеца}other{# месеци}",
    SHORT: "one{# мес.}few{# мес.}other{# мес.}",
    NARROW: "one{# м}few{# м}other{# м}",
  },
  SECOND: {
    LONG: "one{# секунда}few{# секунде}other{# секунди}",
    SHORT: "one{# сек}few{# сек}other{# сек}",
    NARROW: "one{# с}few{# с}other{# с}",
  },
  WEEK: {
    LONG: "one{# недеља}few{# недеље}other{# недеља}",
    SHORT: "one{# нед.}few{# нед.}other{# нед.}",
    NARROW: "one{# н}few{# н}other{# н}",
  },
  YEAR: {
    LONG: "one{# година}few{# године}other{# година}",
    SHORT: "one{# год}few{# год.}other{# год.}",
    NARROW: "one{# г}few{# г}other{# г}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_sr_Latn =  {
  DAY: {
    LONG: "one{# dan}few{# dana}other{# dana}",
    SHORT: "one{# dan}few{# dana}other{# dana}",
    NARROW: "one{# d}few{# d}other{# d}",
  },
  HOUR: {
    LONG: "one{# sat}few{# sata}other{# sati}",
    SHORT: "one{# sat}few{# sata}other{# sati}",
    NARROW: "one{# č}few{# č}other{# č}",
  },
  MINUTE: {
    LONG: "one{# minut}few{# minuta}other{# minuta}",
    SHORT: "one{# min}few{# min}other{# min}",
    NARROW: "one{# m}few{# m}other{# m}",
  },
  MONTH: {
    LONG: "one{# mesec}few{# meseca}other{# meseci}",
    SHORT: "one{# mes.}few{# mes.}other{# mes.}",
    NARROW: "one{# m}few{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# sekunda}few{# sekunde}other{# sekundi}",
    SHORT: "one{# sek}few{# sek}other{# sek}",
    NARROW: "one{# s}few{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# nedelja}few{# nedelje}other{# nedelja}",
    SHORT: "one{# ned.}few{# ned.}other{# ned.}",
    NARROW: "one{# n}few{# n}other{# n}",
  },
  YEAR: {
    LONG: "one{# godina}few{# godine}other{# godina}",
    SHORT: "one{# god}few{# god.}other{# god.}",
    NARROW: "one{# g}few{# g}other{# g}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_sv =  {
  DAY: {
    LONG: "one{# dygn}other{# dygn}",
    SHORT: "one{# d}other{# d}",
    NARROW: "one{#d}other{#d}",
  },
  HOUR: {
    LONG: "one{# timme}other{# timmar}",
    SHORT: "one{# tim}other{# tim}",
    NARROW: "one{#h}other{#h}",
  },
  MINUTE: {
    LONG: "one{# minut}other{# minuter}",
    SHORT: "one{# min}other{# min}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# månad}other{# månader}",
    SHORT: "one{# mån}other{# mån}",
    NARROW: "one{#m}other{#m}",
  },
  SECOND: {
    LONG: "one{# sekund}other{# sekunder}",
    SHORT: "one{# s}other{# s}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# vecka}other{# veckor}",
    SHORT: "one{# v}other{# v}",
    NARROW: "one{#v}other{#v}",
  },
  YEAR: {
    LONG: "one{# år}other{# år}",
    SHORT: "one{# år}other{# år}",
    NARROW: "one{#å}other{#å}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_sw =  {
  DAY: {
    LONG: "one{siku #}other{siku #}",
    SHORT: "one{siku #}other{siku #}",
    NARROW: "one{siku #}other{siku #}",
  },
  HOUR: {
    LONG: "one{saa #}other{saa #}",
    SHORT: "one{saa #}other{saa #}",
    NARROW: "one{saa #}other{saa #}",
  },
  MINUTE: {
    LONG: "one{dakika #}other{dakika #}",
    SHORT: "one{dakika #}other{dakika #}",
    NARROW: "one{dak #}other{dak #}",
  },
  MONTH: {
    LONG: "one{mwezi #}other{miezi #}",
    SHORT: "one{mwezi #}other{miezi #}",
    NARROW: "one{mwezi #}other{miezi #}",
  },
  SECOND: {
    LONG: "one{sekunde #}other{sekunde #}",
    SHORT: "one{sekunde #}other{sekunde #}",
    NARROW: "one{sek #}other{sek #}",
  },
  WEEK: {
    LONG: "one{wiki #}other{wiki #}",
    SHORT: "one{wiki #}other{wiki #}",
    NARROW: "one{wiki #}other{wiki #}",
  },
  YEAR: {
    LONG: "one{mwaka #}other{miaka #}",
    SHORT: "one{mwaka #}other{miaka #}",
    NARROW: "one{mwaka #}other{miaka #}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ta =  {
  DAY: {
    LONG: "one{# நாள்}other{# நாட்கள்}",
    SHORT: "one{# நாள்}other{# நாட்கள்}",
    NARROW: "one{# நா}other{# நா}",
  },
  HOUR: {
    LONG: "one{# மணிநேரம்}other{# மணிநேரங்கள்}",
    SHORT: "one{# மணிநேரம்}other{# மணிநேரம்}",
    NARROW: "one{# ம.நே.}other{# ம.நே.}",
  },
  MINUTE: {
    LONG: "one{# நிமிடம்}other{# நிமிடங்கள்}",
    SHORT: "one{# நிமிடம்}other{# நிமிட}",
    NARROW: "one{# நிமி.}other{# நிமி.}",
  },
  MONTH: {
    LONG: "one{# மாதம்}other{# மாதங்கள்}",
    SHORT: "one{# மாதம்}other{# மாத.}",
    NARROW: "one{# மா}other{# மா}",
  },
  SECOND: {
    LONG: "one{# விநாடி}other{# விநாடிகள்}",
    SHORT: "one{# விநாடி}other{# விநாடி}",
    NARROW: "one{# வி.}other{# வி.}",
  },
  WEEK: {
    LONG: "one{# வாரம்}other{# வாரங்கள்}",
    SHORT: "one{# வாரம்}other{# வார.}",
    NARROW: "one{# வா}other{# வா}",
  },
  YEAR: {
    LONG: "one{# ஆண்டு}other{# ஆண்டுகள்}",
    SHORT: "one{# ஆண்டு}other{# ஆண்டு.}",
    NARROW: "one{# ஆ}other{# ஆ}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_te =  {
  DAY: {
    LONG: "one{# రోజు}other{# రోజులు}",
    SHORT: "one{# రోజు}other{# రోజులు}",
    NARROW: "one{#రో}other{#రో}",
  },
  HOUR: {
    LONG: "one{# గంట}other{# గంటలు}",
    SHORT: "one{# గం.}other{# గం.}",
    NARROW: "one{#గం}other{#గం}",
  },
  MINUTE: {
    LONG: "one{# నిమిషం}other{# నిమిషాలు}",
    SHORT: "one{# నిమి.}other{# నిమి.}",
    NARROW: "one{#ని}other{#ని}",
  },
  MONTH: {
    LONG: "one{# నెల}other{# నెలలు}",
    SHORT: "one{# నె.}other{# నె.}",
    NARROW: "one{#నె}other{#నె}",
  },
  SECOND: {
    LONG: "one{# సెకను}other{# సెకన్లు}",
    SHORT: "one{# సె.}other{# సెక.}",
    NARROW: "one{#సె}other{#సె}",
  },
  WEEK: {
    LONG: "one{# వారం}other{# వారాలు}",
    SHORT: "one{# వా.}other{# వా.}",
    NARROW: "one{#వా}other{#వా}",
  },
  YEAR: {
    LONG: "one{# సంవత్సరం}other{# సంవత్సరాలు}",
    SHORT: "one{# సం.}other{# సం.}",
    NARROW: "one{#సం}other{#సం}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_th =  {
  DAY: {
    LONG: "other{# วัน}",
    SHORT: "other{# วัน}",
    NARROW: "other{#วัน}",
  },
  HOUR: {
    LONG: "other{# ชั่วโมง}",
    SHORT: "other{# ชม.}",
    NARROW: "other{#ชม.}",
  },
  MINUTE: {
    LONG: "other{# นาที}",
    SHORT: "other{# นาที}",
    NARROW: "other{#นาที}",
  },
  MONTH: {
    LONG: "other{# เดือน}",
    SHORT: "other{# เดือน}",
    NARROW: "other{#เดือน}",
  },
  SECOND: {
    LONG: "other{# วินาที}",
    SHORT: "other{# วิ}",
    NARROW: "other{#วิ}",
  },
  WEEK: {
    LONG: "other{# สัปดาห์}",
    SHORT: "other{# สัปดาห์}",
    NARROW: "other{#สัปดาห์}",
  },
  YEAR: {
    LONG: "other{# ปี}",
    SHORT: "other{# ปี}",
    NARROW: "other{#ปี}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_tl =  {
  DAY: {
    LONG: "one{# araw}other{# na araw}",
    SHORT: "one{# araw}other{# araw}",
    NARROW: "one{# araw}other{# na araw}",
  },
  HOUR: {
    LONG: "one{# oras}other{# na oras}",
    SHORT: "one{# oras}other{# na oras}",
    NARROW: "one{# oras}other{# oras}",
  },
  MINUTE: {
    LONG: "one{# minuto}other{# na minuto}",
    SHORT: "one{# min.}other{# min.}",
    NARROW: "one{#m}other{#m}",
  },
  MONTH: {
    LONG: "one{# buwan}other{# buwan}",
    SHORT: "one{# buwan}other{# buwan}",
    NARROW: "one{#buwan}other{# buwan}",
  },
  SECOND: {
    LONG: "one{# segundo}other{# na segundo}",
    SHORT: "one{# seg.}other{# seg.}",
    NARROW: "one{#s}other{#s}",
  },
  WEEK: {
    LONG: "one{# linggo}other{# na linggo}",
    SHORT: "one{# linggo}other{# na linggo}",
    NARROW: "one{#linggo}other{#linggo}",
  },
  YEAR: {
    LONG: "one{# taon}other{# na taon}",
    SHORT: "one{# taon}other{# taon}",
    NARROW: "one{#taon}other{#taon}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_tr =  {
  DAY: {
    LONG: "one{# gün}other{# gün}",
    SHORT: "one{# gün}other{# gün}",
    NARROW: "one{#g}other{#g}",
  },
  HOUR: {
    LONG: "one{# saat}other{# saat}",
    SHORT: "one{# sa.}other{# sa.}",
    NARROW: "one{# sa}other{#s}",
  },
  MINUTE: {
    LONG: "one{# dakika}other{# dakika}",
    SHORT: "one{# dk.}other{# dk.}",
    NARROW: "one{#d}other{#d}",
  },
  MONTH: {
    LONG: "one{# ay}other{# ay}",
    SHORT: "one{# ay}other{# ay}",
    NARROW: "one{#a}other{#a}",
  },
  SECOND: {
    LONG: "one{# saniye}other{# saniye}",
    SHORT: "one{# sn.}other{# sn.}",
    NARROW: "one{#sn}other{#sn}",
  },
  WEEK: {
    LONG: "one{# hafta}other{# hafta}",
    SHORT: "one{# hf.}other{# hf.}",
    NARROW: "one{#h}other{#h}",
  },
  YEAR: {
    LONG: "one{# yıl}other{# yıl}",
    SHORT: "one{# yıl}other{# yıl}",
    NARROW: "one{#y}other{#y}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_uk =  {
  DAY: {
    LONG: "one{# день}few{# дні}many{# днів}other{# дня}",
    SHORT: "one{# дн.}few{# дн.}many{# дн.}other{# дн.}",
    NARROW: "one{#д}few{#д}many{#д}other{#д}",
  },
  HOUR: {
    LONG: "one{# година}few{# години}many{# годин}other{# години}",
    SHORT: "one{# год}few{# год}many{# год}other{# год}",
    NARROW: "one{# год.}few{# год.}many{# год.}other{# год.}",
  },
  MINUTE: {
    LONG: "one{# хвилина}few{# хвилини}many{# хвилин}other{# хвилини}",
    SHORT: "one{# хв}few{# хв}many{# хв}other{# хв}",
    NARROW: "one{#х}few{#х}many{#х}other{#х}",
  },
  MONTH: {
    LONG: "one{# місяць}few{# місяці}many{# місяців}other{# місяця}",
    SHORT: "one{# міс.}few{# міс.}many{# міс.}other{# міс.}",
    NARROW: "one{#м}few{#м}many{#м}other{#м}",
  },
  SECOND: {
    LONG: "one{# секунда}few{# секунди}many{# секунд}other{# секунди}",
    SHORT: "one{# с}few{# с}many{# с}other{# с}",
    NARROW: "one{#с}few{#с}many{#с}other{#с}",
  },
  WEEK: {
    LONG: "one{# тиждень}few{# тижні}many{# тижнів}other{# тижня}",
    SHORT: "one{# тиж.}few{# тиж.}many{# тиж.}other{# тиж.}",
    NARROW: "one{#т}few{#т}many{#т}other{#т}",
  },
  YEAR: {
    LONG: "one{# рік}few{# роки}many{# років}other{# року}",
    SHORT: "one{# р.}few{# р.}many{# р.}other{# р.}",
    NARROW: "one{#р}few{#р}many{#р}other{#р}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_ur =  {
  DAY: {
    LONG: "one{# دن}other{# دن}",
    SHORT: "one{# دن}other{# دن}",
    NARROW: "one{# دن}other{# دن}",
  },
  HOUR: {
    LONG: "one{# گھنٹہ}other{# گھنٹے}",
    SHORT: "one{# گھنٹہ}other{# گھنٹے}",
    NARROW: "one{# گھنٹہ}other{# گھنٹے}",
  },
  MINUTE: {
    LONG: "one{# منٹ}other{# منٹ}",
    SHORT: "one{# منٹ}other{# منٹ}",
    NARROW: "one{# منٹ}other{# منٹ}",
  },
  MONTH: {
    LONG: "one{# مہینہ}other{# مہینے}",
    SHORT: "one{# مہینہ}other{# مہینے}",
    NARROW: "one{# مہینہ}other{# مہینے}",
  },
  SECOND: {
    LONG: "one{# سیکنڈ}other{# سیکنڈ}",
    SHORT: "one{# سیکنڈ}other{# سیکنڈ}",
    NARROW: "one{# سیکنڈ}other{# سیکنڈ}",
  },
  WEEK: {
    LONG: "one{# ہفتہ}other{# ہفتے}",
    SHORT: "one{# ہفتہ}other{# ہفتے}",
    NARROW: "one{# ہفتہ}other{# ہفتے}",
  },
  YEAR: {
    LONG: "one{# سال}other{# سال}",
    SHORT: "one{# سال}other{# سال}",
    NARROW: "one{# سال}other{# سال}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_uz =  {
  DAY: {
    LONG: "one{# kun}other{# kun}",
    SHORT: "one{# kun}other{# kun}",
    NARROW: "one{# kun}other{# kun}",
  },
  HOUR: {
    LONG: "one{# soat}other{# soat}",
    SHORT: "one{# soat}other{# soat}",
    NARROW: "one{# soat}other{# soat}",
  },
  MINUTE: {
    LONG: "one{# daqiqa}other{# daqiqa}",
    SHORT: "one{# daq.}other{# daq.}",
    NARROW: "one{# daq.}other{# daq.}",
  },
  MONTH: {
    LONG: "one{# oy}other{# oy}",
    SHORT: "one{# oy}other{# oy}",
    NARROW: "one{# oy}other{# oy}",
  },
  SECOND: {
    LONG: "one{# soniya}other{# soniya}",
    SHORT: "one{# son.}other{# son.}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# hafta}other{# hafta}",
    SHORT: "one{# hafta}other{# hafta}",
    NARROW: "one{# hafta}other{# hafta}",
  },
  YEAR: {
    LONG: "one{# yil}other{# yil}",
    SHORT: "one{# yil}other{# yil}",
    NARROW: "one{# yil}other{# yil}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_vi =  {
  DAY: {
    LONG: "other{# ngày}",
    SHORT: "other{# ngày}",
    NARROW: "other{# ngày}",
  },
  HOUR: {
    LONG: "other{# giờ}",
    SHORT: "other{# giờ}",
    NARROW: "other{# giờ}",
  },
  MINUTE: {
    LONG: "other{# phút}",
    SHORT: "other{# phút}",
    NARROW: "other{# phút}",
  },
  MONTH: {
    LONG: "other{# tháng}",
    SHORT: "other{# tháng}",
    NARROW: "other{# tháng}",
  },
  SECOND: {
    LONG: "other{# giây}",
    SHORT: "other{# giây}",
    NARROW: "other{# giây}",
  },
  WEEK: {
    LONG: "other{# tuần}",
    SHORT: "other{# tuần}",
    NARROW: "other{# tuần}",
  },
  YEAR: {
    LONG: "other{# năm}",
    SHORT: "other{# năm}",
    NARROW: "other{# năm}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_zh =  {
  DAY: {
    LONG: "other{#天}",
    SHORT: "other{#天}",
    NARROW: "other{#天}",
  },
  HOUR: {
    LONG: "other{#小时}",
    SHORT: "other{#小时}",
    NARROW: "other{#小时}",
  },
  MINUTE: {
    LONG: "other{#分钟}",
    SHORT: "other{#分钟}",
    NARROW: "other{#分钟}",
  },
  MONTH: {
    LONG: "other{#个月}",
    SHORT: "other{#个月}",
    NARROW: "other{#个月}",
  },
  SECOND: {
    LONG: "other{#秒钟}",
    SHORT: "other{#秒}",
    NARROW: "other{#秒}",
  },
  WEEK: {
    LONG: "other{#周}",
    SHORT: "other{#周}",
    NARROW: "other{#周}",
  },
  YEAR: {
    LONG: "other{#年}",
    SHORT: "other{#年}",
    NARROW: "other{#年}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_zh_CN = exports.DurationSymbols_zh;

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_zh_HK =  {
  DAY: {
    LONG: "other{# 日}",
    SHORT: "other{# 日}",
    NARROW: "other{#日}",
  },
  HOUR: {
    LONG: "other{# 小時}",
    SHORT: "other{# 小時}",
    NARROW: "other{#小時}",
  },
  MINUTE: {
    LONG: "other{# 分鐘}",
    SHORT: "other{# 分鐘}",
    NARROW: "other{#分}",
  },
  MONTH: {
    LONG: "other{# 個月}",
    SHORT: "other{# 個月}",
    NARROW: "other{#個月}",
  },
  SECOND: {
    LONG: "other{# 秒}",
    SHORT: "other{# 秒}",
    NARROW: "other{#秒}",
  },
  WEEK: {
    LONG: "other{# 星期}",
    SHORT: "other{# 星期}",
    NARROW: "other{#週}",
  },
  YEAR: {
    LONG: "other{# 年}",
    SHORT: "other{# 年}",
    NARROW: "other{#年}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_zh_TW =  {
  DAY: {
    LONG: "other{# 天}",
    SHORT: "other{# 天}",
    NARROW: "other{# 天}",
  },
  HOUR: {
    LONG: "other{# 小時}",
    SHORT: "other{# 小時}",
    NARROW: "other{# 小時}",
  },
  MINUTE: {
    LONG: "other{# 分鐘}",
    SHORT: "other{# 分鐘}",
    NARROW: "other{# 分鐘}",
  },
  MONTH: {
    LONG: "other{# 個月}",
    SHORT: "other{# 個月}",
    NARROW: "other{# 個月}",
  },
  SECOND: {
    LONG: "other{# 秒}",
    SHORT: "other{# 秒}",
    NARROW: "other{# 秒}",
  },
  WEEK: {
    LONG: "other{# 週}",
    SHORT: "other{# 週}",
    NARROW: "other{# 週}",
  },
  YEAR: {
    LONG: "other{# 年}",
    SHORT: "other{# 年}",
    NARROW: "other{# 年}",
  },
};

/** @const {!DurationSymbolTypes.DurationSymbols} */
exports.DurationSymbols_zu =  {
  DAY: {
    LONG: "one{# usuku}other{# izinsuku}",
    SHORT: "one{# usuku}other{# izinsuku}",
    NARROW: "one{#}other{# suku}",
  },
  HOUR: {
    LONG: "one{# ihora}other{# amahora}",
    SHORT: "one{# hora}other{# hr}",
    NARROW: "one{# hora}other{# hora}",
  },
  MINUTE: {
    LONG: "one{# iminithi}other{# amaminithi}",
    SHORT: "one{# iminithi}other{# iminithi}",
    NARROW: "one{# umzuzu}other{# umzuzu}",
  },
  MONTH: {
    LONG: "one{# inyanga}other{# izinyanga}",
    SHORT: "one{# nyanga}other{# izinyanga}",
    NARROW: "one{# m}other{# m}",
  },
  SECOND: {
    LONG: "one{# isekhondi}other{# amasekhondi}",
    SHORT: "one{# sekhondi}other{# sec}",
    NARROW: "one{# s}other{# s}",
  },
  WEEK: {
    LONG: "one{# iviki}other{# amaviki}",
    SHORT: "one{# viki}other{# amaviki}",
    NARROW: "one{# w}other{# w}",
  },
  YEAR: {
    LONG: "one{# y}other{# y}",
    SHORT: "one{# y}other{# yrs}",
    NARROW: "one{# y}other{# y}",
  },
};

switch (goog.LOCALE) {
  case 'af':
    defaultSymbols = exports.DurationSymbols_af;
    break;
  case 'am':
    defaultSymbols = exports.DurationSymbols_am;
    break;
  case 'ar':
    defaultSymbols = exports.DurationSymbols_ar;
    break;
  case 'ar_DZ':
  case 'ar-DZ':
    defaultSymbols = exports.DurationSymbols_ar_DZ;
    break;
  case 'ar_EG':
  case 'ar-EG':
    defaultSymbols = exports.DurationSymbols_ar_EG;
    break;
  case 'az':
    defaultSymbols = exports.DurationSymbols_az;
    break;
  case 'be':
    defaultSymbols = exports.DurationSymbols_be;
    break;
  case 'bg':
    defaultSymbols = exports.DurationSymbols_bg;
    break;
  case 'bn':
    defaultSymbols = exports.DurationSymbols_bn;
    break;
  case 'br':
    defaultSymbols = exports.DurationSymbols_br;
    break;
  case 'bs':
    defaultSymbols = exports.DurationSymbols_bs;
    break;
  case 'ca':
    defaultSymbols = exports.DurationSymbols_ca;
    break;
  case 'chr':
    defaultSymbols = exports.DurationSymbols_chr;
    break;
  case 'cs':
    defaultSymbols = exports.DurationSymbols_cs;
    break;
  case 'cy':
    defaultSymbols = exports.DurationSymbols_cy;
    break;
  case 'da':
    defaultSymbols = exports.DurationSymbols_da;
    break;
  case 'de':
    defaultSymbols = exports.DurationSymbols_de;
    break;
  case 'de_AT':
  case 'de-AT':
    defaultSymbols = exports.DurationSymbols_de_AT;
    break;
  case 'de_CH':
  case 'de-CH':
    defaultSymbols = exports.DurationSymbols_de_CH;
    break;
  case 'el':
    defaultSymbols = exports.DurationSymbols_el;
    break;
  case 'en':
    defaultSymbols = exports.DurationSymbols_en;
    break;
  case 'en_AU':
  case 'en-AU':
    defaultSymbols = exports.DurationSymbols_en_AU;
    break;
  case 'en_CA':
  case 'en-CA':
    defaultSymbols = exports.DurationSymbols_en_CA;
    break;
  case 'en_GB':
  case 'en-GB':
    defaultSymbols = exports.DurationSymbols_en_GB;
    break;
  case 'en_IE':
  case 'en-IE':
    defaultSymbols = exports.DurationSymbols_en_IE;
    break;
  case 'en_IN':
  case 'en-IN':
    defaultSymbols = exports.DurationSymbols_en_IN;
    break;
  case 'en_SG':
  case 'en-SG':
    defaultSymbols = exports.DurationSymbols_en_SG;
    break;
  case 'en_US':
  case 'en-US':
    defaultSymbols = exports.DurationSymbols_en_US;
    break;
  case 'en_ZA':
  case 'en-ZA':
    defaultSymbols = exports.DurationSymbols_en_ZA;
    break;
  case 'es':
    defaultSymbols = exports.DurationSymbols_es;
    break;
  case 'es_419':
  case 'es-419':
    defaultSymbols = exports.DurationSymbols_es_419;
    break;
  case 'es_ES':
  case 'es-ES':
    defaultSymbols = exports.DurationSymbols_es_ES;
    break;
  case 'es_MX':
  case 'es-MX':
    defaultSymbols = exports.DurationSymbols_es_MX;
    break;
  case 'es_US':
  case 'es-US':
    defaultSymbols = exports.DurationSymbols_es_US;
    break;
  case 'et':
    defaultSymbols = exports.DurationSymbols_et;
    break;
  case 'eu':
    defaultSymbols = exports.DurationSymbols_eu;
    break;
  case 'fa':
    defaultSymbols = exports.DurationSymbols_fa;
    break;
  case 'fi':
    defaultSymbols = exports.DurationSymbols_fi;
    break;
  case 'fil':
    defaultSymbols = exports.DurationSymbols_fil;
    break;
  case 'fr':
    defaultSymbols = exports.DurationSymbols_fr;
    break;
  case 'fr_CA':
  case 'fr-CA':
    defaultSymbols = exports.DurationSymbols_fr_CA;
    break;
  case 'ga':
    defaultSymbols = exports.DurationSymbols_ga;
    break;
  case 'gl':
    defaultSymbols = exports.DurationSymbols_gl;
    break;
  case 'gsw':
    defaultSymbols = exports.DurationSymbols_gsw;
    break;
  case 'gu':
    defaultSymbols = exports.DurationSymbols_gu;
    break;
  case 'haw':
    defaultSymbols = exports.DurationSymbols_haw;
    break;
  case 'he':
    defaultSymbols = exports.DurationSymbols_he;
    break;
  case 'hi':
    defaultSymbols = exports.DurationSymbols_hi;
    break;
  case 'hr':
    defaultSymbols = exports.DurationSymbols_hr;
    break;
  case 'hu':
    defaultSymbols = exports.DurationSymbols_hu;
    break;
  case 'hy':
    defaultSymbols = exports.DurationSymbols_hy;
    break;
  case 'id':
    defaultSymbols = exports.DurationSymbols_id;
    break;
  case 'in':
    defaultSymbols = exports.DurationSymbols_in;
    break;
  case 'is':
    defaultSymbols = exports.DurationSymbols_is;
    break;
  case 'it':
    defaultSymbols = exports.DurationSymbols_it;
    break;
  case 'iw':
    defaultSymbols = exports.DurationSymbols_iw;
    break;
  case 'ja':
    defaultSymbols = exports.DurationSymbols_ja;
    break;
  case 'ka':
    defaultSymbols = exports.DurationSymbols_ka;
    break;
  case 'kk':
    defaultSymbols = exports.DurationSymbols_kk;
    break;
  case 'km':
    defaultSymbols = exports.DurationSymbols_km;
    break;
  case 'kn':
    defaultSymbols = exports.DurationSymbols_kn;
    break;
  case 'ko':
    defaultSymbols = exports.DurationSymbols_ko;
    break;
  case 'ky':
    defaultSymbols = exports.DurationSymbols_ky;
    break;
  case 'ln':
    defaultSymbols = exports.DurationSymbols_ln;
    break;
  case 'lo':
    defaultSymbols = exports.DurationSymbols_lo;
    break;
  case 'lt':
    defaultSymbols = exports.DurationSymbols_lt;
    break;
  case 'lv':
    defaultSymbols = exports.DurationSymbols_lv;
    break;
  case 'mk':
    defaultSymbols = exports.DurationSymbols_mk;
    break;
  case 'ml':
    defaultSymbols = exports.DurationSymbols_ml;
    break;
  case 'mn':
    defaultSymbols = exports.DurationSymbols_mn;
    break;
  case 'mo':
    defaultSymbols = exports.DurationSymbols_mo;
    break;
  case 'mr':
    defaultSymbols = exports.DurationSymbols_mr;
    break;
  case 'ms':
    defaultSymbols = exports.DurationSymbols_ms;
    break;
  case 'mt':
    defaultSymbols = exports.DurationSymbols_mt;
    break;
  case 'my':
    defaultSymbols = exports.DurationSymbols_my;
    break;
  case 'nb':
    defaultSymbols = exports.DurationSymbols_nb;
    break;
  case 'ne':
    defaultSymbols = exports.DurationSymbols_ne;
    break;
  case 'nl':
    defaultSymbols = exports.DurationSymbols_nl;
    break;
  case 'no':
    defaultSymbols = exports.DurationSymbols_no;
    break;
  case 'no_NO':
  case 'no-NO':
    defaultSymbols = exports.DurationSymbols_no_NO;
    break;
  case 'or':
    defaultSymbols = exports.DurationSymbols_or;
    break;
  case 'pa':
    defaultSymbols = exports.DurationSymbols_pa;
    break;
  case 'pl':
    defaultSymbols = exports.DurationSymbols_pl;
    break;
  case 'pt':
    defaultSymbols = exports.DurationSymbols_pt;
    break;
  case 'pt_BR':
  case 'pt-BR':
    defaultSymbols = exports.DurationSymbols_pt_BR;
    break;
  case 'pt_PT':
  case 'pt-PT':
    defaultSymbols = exports.DurationSymbols_pt_PT;
    break;
  case 'ro':
    defaultSymbols = exports.DurationSymbols_ro;
    break;
  case 'ru':
    defaultSymbols = exports.DurationSymbols_ru;
    break;
  case 'sh':
    defaultSymbols = exports.DurationSymbols_sh;
    break;
  case 'si':
    defaultSymbols = exports.DurationSymbols_si;
    break;
  case 'sk':
    defaultSymbols = exports.DurationSymbols_sk;
    break;
  case 'sl':
    defaultSymbols = exports.DurationSymbols_sl;
    break;
  case 'sq':
    defaultSymbols = exports.DurationSymbols_sq;
    break;
  case 'sr':
    defaultSymbols = exports.DurationSymbols_sr;
    break;
  case 'sr_Latn':
  case 'sr-Latn':
    defaultSymbols = exports.DurationSymbols_sr_Latn;
    break;
  case 'sv':
    defaultSymbols = exports.DurationSymbols_sv;
    break;
  case 'sw':
    defaultSymbols = exports.DurationSymbols_sw;
    break;
  case 'ta':
    defaultSymbols = exports.DurationSymbols_ta;
    break;
  case 'te':
    defaultSymbols = exports.DurationSymbols_te;
    break;
  case 'th':
    defaultSymbols = exports.DurationSymbols_th;
    break;
  case 'tl':
    defaultSymbols = exports.DurationSymbols_tl;
    break;
  case 'tr':
    defaultSymbols = exports.DurationSymbols_tr;
    break;
  case 'uk':
    defaultSymbols = exports.DurationSymbols_uk;
    break;
  case 'ur':
    defaultSymbols = exports.DurationSymbols_ur;
    break;
  case 'uz':
    defaultSymbols = exports.DurationSymbols_uz;
    break;
  case 'vi':
    defaultSymbols = exports.DurationSymbols_vi;
    break;
  case 'zh':
    defaultSymbols = exports.DurationSymbols_zh;
    break;
  case 'zh_CN':
  case 'zh-CN':
    defaultSymbols = exports.DurationSymbols_zh_CN;
    break;
  case 'zh_HK':
  case 'zh-HK':
    defaultSymbols = exports.DurationSymbols_zh_HK;
    break;
  case 'zh_TW':
  case 'zh-TW':
    defaultSymbols = exports.DurationSymbols_zh_TW;
    break;
  case 'zu':
    defaultSymbols = exports.DurationSymbols_zu;
    break;
  default:
    defaultSymbols = exports.DurationSymbols_en;
}
