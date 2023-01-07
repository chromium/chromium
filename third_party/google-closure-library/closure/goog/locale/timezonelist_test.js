/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.locale.TimeZoneListTest');
goog.setTestOnly();

const locale = goog.require('goog.locale');
const testSuite = goog.require('goog.testing.testSuite');
const timezonelist = goog.require('goog.locale.timezonelist');

/* Uncomment to display complete listing in the unit tested invocations.

document.write('Shortnames in German for France:<br>');

var idlist = goog.locale.timezonelist.getTimeZoneSelectedShortNames('FR');

for (var i = 0; i < idlist.length; i++) {
  document.write(i + ') ' + idlist[i].id + ' = ' + idlist[i].name + '<br>');
}

document.write('<hr>');

document.write('long names in German for all en speakers:<br>');
var idlist = goog.locale.timezonelist.getTimeZoneSelectedLongNames('en');

for (var i = 0; i < idlist.length; i++) {
  document.write(i + ') ' + idlist[i].id + ' = ' + idlist[i].name + '<br>');
}

document.write('<hr>');

document.write('Longnames in German for germans:<br>');
var idlist = goog.locale.timezonelist.getTimeZoneSelectedLongNames();

for (var i = 0; i < idlist.length; i++) {
  document.write(i + ') ' + idlist[i].id + ' = ' + idlist[i].name + '<br>');
}

document.write('<hr>');

document.write('All longnames in German:<br>');
var idlist = goog.locale.timezonelist.getTimeZoneAllLongNames();

for (var i = 0; i < idlist.length; i++) {
  var pair = idlist[i];
  document.write(i + ') ' + pair.id + ' = ' + pair.name + '<br>');
}

document.write('<hr>');
*/

// Test cases.

testSuite({
  setUpPage() {
    // Test data files are in in http://go/js_locale_data

    // Test data from TimeZoneSelectedIds__FR.js
    const TimeZoneSelectedIds__FR =
        ['Etc/GMT+12', 'Pacific/Midway', 'America/Adak', 'Pacific/Honolulu'];
    locale.registerTimeZoneSelectedIds(TimeZoneSelectedIds__FR, 'FR');

    // Test data from TimeZoneSelectedShortNames__de_DE.js
    const TimeZoneSelectedShortNames__de_DE = {
      'Etc/GMT+12': 'GMT-12:00',
      'Etc/GMT+11': 'GMT-11:00',
      'Pacific/Pago_Pago': 'Amerikanisch-Samoa',
      'Pacific/Midway': 'Midway (Amerikanisch-Ozeanien)',
      'Pacific/Honolulu': 'Honolulu (Vereinigte Staaten)',
      'Etc/GMT+10': 'GMT-10:00',
      'America/Adak': 'Adak (Vereinigte Staaten)',
    };
    locale.registerTimeZoneSelectedShortNames(
        TimeZoneSelectedShortNames__de_DE, 'de_DE');

    // Test data from TimeZoneSelectedLongNames__de_DE.js
    const TimeZoneSelectedLongNames__de_DE = {
      'Etc/GMT+12': 'GMT-12:00',
      'Etc/GMT+11': 'GMT-11:00',
      'Pacific/Pago_Pago': 'GMT-11:00 Amerikanisch-Samoa',
      'Pacific/Midway': 'GMT-11:00 Midway (Amerikanisch-Ozeanien)',
      'Pacific/Honolulu': 'GMT-10:00 Honolulu (Vereinigte Staaten)',
      'Etc/GMT+10': 'GMT-10:00',
      'America/Adak': 'GMT-10:00 Adak (Vereinigte Staaten)',
    };
    locale.registerTimeZoneSelectedLongNames(
        TimeZoneSelectedLongNames__de_DE, 'de_DE');

    // Test data from TimeZoneSelectedIds__en.js
    const TimeZoneSelectedIds__en =
        ['Etc/GMT+12', 'Pacific/Midway', 'America/Adak', 'Pacific/Honolulu'];
    locale.registerTimeZoneSelectedIds(TimeZoneSelectedIds__en, 'en');

    // Test data from TimeZoneSelectedIds__DE.js
    const TimeZoneSelectedIds__DE =
        ['Etc/GMT+12', 'Pacific/Midway', 'America/Adak', 'Pacific/Honolulu'];
    locale.registerTimeZoneSelectedIds(TimeZoneSelectedIds__DE, 'DE');

    // Test data from TimeZoneAllLongNames__de_DE.js
    const TimeZoneAllLongNames__de_DE = [
      {id: 'Etc/GMT+12', name: 'GMT-12:00'},
      {id: 'Pacific/Apia', name: 'GMT-11:00 Samoa'},
      {id: 'Pacific/Midway', name: 'GMT-11:00 Midway (Amerikanisch-Ozeanien)'},
      {id: 'Pacific/Niue', name: 'GMT-11:00 Niue'},
      {id: 'Pacific/Pago_Pago', name: 'GMT-11:00 Amerikanisch-Samoa'},
      {id: 'Etc/GMT+11', name: 'GMT-11:00'},
      {id: 'America/Adak', name: 'GMT-10:00 Adak (Vereinigte Staaten)'},
      {id: 'Pacific/Fakaofo', name: 'GMT-10:00 Tokelau'},
    ];
    locale.registerTimeZoneAllLongNames(TimeZoneAllLongNames__de_DE, 'de_DE');

    locale.setLocale('de_DE');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTimeZoneSelectedShortNames() {
    // Shortnames in German for France.
    const result = timezonelist.getTimeZoneSelectedShortNames('FR');
    assertEquals('Honolulu (Vereinigte Staaten)', result[3].name);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTimeZoneSelectedLongNames() {
    // Long names in German for all English speaking regions.
    let result = timezonelist.getTimeZoneSelectedLongNames('en');
    assertEquals('GMT-11:00 Midway (Amerikanisch-Ozeanien)', result[1].name);

    // Long names in German for germans.
    result = timezonelist.getTimeZoneSelectedLongNames();
    assertEquals('GMT-10:00 Adak (Vereinigte Staaten)', result[2].name);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTimeZoneAllLongNames() {
    // All longnames in German
    const result = timezonelist.getTimeZoneAllLongNames();
    assertEquals('GMT-10:00 Tokelau', result[7].name);
  },
});
