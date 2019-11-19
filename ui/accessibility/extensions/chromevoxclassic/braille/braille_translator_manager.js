// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of the current braille translators.
 */

goog.provide('cvox.BrailleTranslatorManager');

goog.require('cvox.BrailleTable');
goog.require('cvox.ExpandingBrailleTranslator');
goog.require('cvox.LibLouis');

/**
 * @param {cvox.LibLouis=} opt_liblouisForTest Liblouis instance to use
 *     for testing.
 * @constructor
 */
cvox.BrailleTranslatorManager = function(opt_liblouisForTest) {
  /**
   * @type {!cvox.LibLouis}
   * @private
   */
  this.liblouis_ = opt_liblouisForTest || new cvox.LibLouis(
      chrome.extension.getURL('braille/liblouis_nacl.nmf'),
      chrome.extension.getURL('braille/tables'));
  /**
   * @type {!Array<function()>}
   * @private
   */
  this.changeListeners_ = [];
  /**
   * @type {!Array<cvox.BrailleTable.Table>}
   * @private
   */
  this.tables_ = [];
  /**
   * @type {cvox.ExpandingBrailleTranslator}
   * @private
   */
  this.expandingTranslator_ = null;
  /**
   * @type {cvox.LibLouis.Translator}
   * @private
   */
  this.defaultTranslator_ = null;
  /**
   * @type {string?}
   * @private
   */
  this.defaultTableId_ = null;
  /**
   * @type {cvox.LibLouis.Translator}
   * @private
   */
  this.uncontractedTranslator_ = null;
  /**
   * @type {string?}
   * @private
   */
  this.uncontractedTableId_ = null;

  if (!opt_liblouisForTest) {
    document.addEventListener('DOMContentLoaded',
                              this.loadLiblouis_.bind(this),
                              false);
  }
};

cvox.BrailleTranslatorManager.prototype = {
  /**
   * Adds a listener to be called whenever there is a change in the
   * translator(s) returned by other methods of this instance.
   * @param {function()} listener The listener.
   */
  addChangeListener: function(listener) {
    this.changeListeners_.push(listener);
  },

  /**
   * Refreshes the braille translator(s) used for input and output.  This
   * should be called when something has changed (such as a preference) to
   * make sure that the correct translator is used.
   */
  refresh: function() {
    var tables = this.tables_;
    if (tables.length == 0)
      return;

    // First, see if we have a braille table set previously.
    var table = cvox.BrailleTable.forId(tables, localStorage['brailleTable']);
    if (!table) {
      // Match table against current locale.
      var currentLocale = chrome.i18n.getMessage('@@ui_locale').split(/[_-]/);
      var major = currentLocale[0];
      var minor = currentLocale[1];
      var firstPass = tables.filter(function(table) {
        return table.locale.split(/[_-]/)[0] == major;
      });
      if (firstPass.length > 0) {
        table = firstPass[0];
        if (minor) {
          var secondPass = firstPass.filter(function(table) {
            return table.locale.split(/[_-]/)[1] == minor;
          });
          if (secondPass.length > 0)
            table = secondPass[0];
        }
      }
    }
    if (!table)
      table = cvox.BrailleTable.forId(tables, 'en-US-comp8');

    // TODO(plundblad): Only update when user explicitly selects a table
    // so that switching locales changes table by default.  crbug.com/441206.
    localStorage['brailleTable'] = table.id;
    if (!localStorage['brailleTable6'])
      localStorage['brailleTable6'] = 'en-US-g1';
    if (!localStorage['brailleTable8'])
      localStorage['brailleTable8'] = 'en-US-comp8';

    if (table.dots == '6') {
      localStorage['brailleTableType'] = 'brailleTable6';
      localStorage['brailleTable6'] = table.id;
    } else {
      localStorage['brailleTableType'] = 'brailleTable8';
      localStorage['brailleTable8'] = table.id;
    }

    // If the user explicitly set an 8 dot table, use that when looking
    // for an uncontracted table.  Otherwise, use the current table and let
    // getUncontracted find an appropriate corresponding table.
    var table8Dot = cvox.BrailleTable.forId(tables,
                                            localStorage['brailleTable8']);
    var uncontractedTable = cvox.BrailleTable.getUncontracted(
        tables, table8Dot || table);

    var newDefaultTableId = table.id;
    var newUncontractedTableId = table.id === uncontractedTable.id ?
        null : uncontractedTable.id;
    if (newDefaultTableId === this.defaultTableId_ &&
        newUncontractedTableId === this.uncontractedTableId_) {
      return;
    }

    var finishRefresh = function(defaultTranslator, uncontractedTranslator) {
      this.defaultTableId_ = newDefaultTableId;
      this.uncontractedTableId_ = newUncontractedTableId;
      this.expandingTranslator_ = new cvox.ExpandingBrailleTranslator(
          defaultTranslator, uncontractedTranslator);
      this.defaultTranslator_ = defaultTranslator;
      this.uncontractedTranslator_ = uncontractedTranslator;
      this.changeListeners_.forEach(function(listener) { listener(); });
    }.bind(this);

    this.liblouis_.getTranslator(table.fileNames, function(translator) {
      if (!newUncontractedTableId) {
        finishRefresh(translator, null);
      } else {
        this.liblouis_.getTranslator(
            uncontractedTable.fileNames,
            function(uncontractedTranslator) {
              finishRefresh(translator, uncontractedTranslator);
            });
          }
    }.bind(this));
  },

  /**
   * @return {cvox.ExpandingBrailleTranslator} The current expanding braille
   *     translator, or {@code null} if none is available.
   */
  getExpandingTranslator: function() {
    return this.expandingTranslator_;
  },

  /**
   * @return {cvox.LibLouis.Translator} The current braille translator to use
   *     by default, or {@code null} if none is available.
   */
  getDefaultTranslator: function() {
    return this.defaultTranslator_;
  },

  /**
   * @return {cvox.LibLouis.Translator} The current uncontracted braille
   *     translator, or {@code null} if it is the same as the default
   *     translator.
   */
  getUncontractedTranslator: function() {
    return this.uncontractedTranslator_;
  },

  /**
   * Asynchronously fetches the list of braille tables and refreshes the
   * translators when done.
   * @private
   */
  fetchTables_: function() {
    cvox.BrailleTable.getAll(function(tables) {
      this.tables_ = tables;
      this.refresh();
    }.bind(this));
  },

  /**
   * Loads the liblouis instance by attaching it to the document.
   * @private
   */
  loadLiblouis_: function() {
    // Cast away nullability.  When the document is loaded, it will always
    // have a body.
    this.liblouis_.attachToElement(
        /** @type {!HTMLBodyElement} */ (document.body));
    this.fetchTables_();
  },

  /**
   * @return {!cvox.LibLouis} The liblouis instance used by this object.
   */
  getLibLouisForTest: function() {
    return this.liblouis_;
  },

  /**
   * @return {!Array<cvox.BrailleTable.Table>} The currently loaded braille
   *     tables, or an empty array if they are not yet loaded.
   */
  getTablesForTest: function() {
    return this.tables_;
  }
};
