// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Defines methods related to retrieving translated messages.
 */

goog.provide('Msgs');

/**
 * @constructor
 */
Msgs = function() {};

/**
 * The namespace for all Chromevox messages.
 * @type {string}
 * @const
 * @private
 */
Msgs.NAMESPACE_ = 'chromevox_';

/**
 * Return the current locale.
 * @return {string} The locale.
 */
Msgs.getLocale = function() {
  return chrome.i18n.getMessage('locale');
};

/**
 * Returns the message with the given message id from the ChromeVox namespace.
 *
 * If we can't find a message, throw an exception.  This allows us to catch
 * typos early.
 *
 * @param {string} messageId The id.
 * @param {Array<string>=} opt_subs Substitution strings.
 * @return {string} The localized message.
 */
Msgs.getMsg = function(messageId, opt_subs) {
  var message = Msgs.Untranslated[messageId.toUpperCase()];
  if (message !== undefined)
    return Msgs.applySubstitutions_(message, opt_subs);
  message = chrome.i18n.getMessage(
      Msgs.NAMESPACE_ + messageId, opt_subs);
  if (message == undefined || message == '') {
    throw new Error('Invalid ChromeVox message id: ' + messageId);
  }
  return message;
};

/**
 * Processes an HTML DOM, replacing text content with translated text messages
 * on elements marked up for translation.  Elements whose class attributes
 * contain the 'i18n' class name are expected to also have an msgid
 * attribute. The value of the msgid attributes are looked up as message
 * IDs and the resulting text is used as the text content of the elements.
 *
 * @param {Node} root The root node where the translation should be performed.
 */
Msgs.addTranslatedMessagesToDom = function(root) {
  var elts = root.querySelectorAll('.i18n');
  for (var i = 0; i < elts.length; i++) {
    var msgid = elts[i].getAttribute('msgid');
    if (!msgid) {
      throw new Error('Element has no msgid attribute: ' + elts[i]);
    }
    var val = this.getMsg(msgid);
    if (elts[i].tagName == 'INPUT') {
      elts[i].setAttribute('placeholder', val);
    } else {
      elts[i].textContent = val;
    }
    elts[i].classList.add('i18n-processed');
  }
};

/**
 * Retuns a number formatted correctly.
 *
 * @param {number} num The number.
 * @return {string} The number in the correct locale.
 */
Msgs.getNumber = function(num) {
  return '' + num;
};

/**
 * Applies substitions of the form $N, where N is a number from 1 to 9, to a
 * string. The numbers are one-based indices into |opt_subs|.
 * @param {string} message
 * @param {Array<string>=} opt_subs
 * @return {string}
 * @private
 */
Msgs.applySubstitutions_ = function(message, opt_subs) {
  if (opt_subs) {
    for (var i = 0; i < opt_subs.length; i++) {
      message = message.replace('$' + (i + 1), opt_subs[i]);
    }
  }
  return message;
};

/**
 * Strings that are displayed in the user interface but don't need
 * be translated.
 * @type {Object<string>}
 */
Msgs.Untranslated = {
  /** The unchecked state for a checkbox in braille. */
  CHECKBOX_UNCHECKED_STATE_BRL: '( )',
  /** The checked state for a checkbox in braille. */
  CHECKBOX_CHECKED_STATE_BRL: '(x)',
  /** The unselected state for a radio button in braille. */
  RADIO_UNSELECTED_STATE_BRL: '( )',
  /** The selected state for a radio button in braille. */
  RADIO_SELECTED_STATE_BRL: '(x)',
  /** Brailled after a menu if the menu has a submenu. */
  ARIA_HAS_SUBMENU_BRL: '->',
  /** Describes an element with the ARIA role option. */
  ROLE_OPTION: ' ',
  /** Braille of element with the ARIA role option. */
  ROLE_OPTION_BRL: ' ',
  /** Braille of element with the ARIA attribute aria-checked=true. */
  ARIA_CHECKED_TRUE_BRL: '(x)',
  /** Braille of element with the ARIA attribute aria-checked=false. */
  ARIA_CHECKED_FALSE_BRL: '( )',
  /** Braille of element with the ARIA attribute aria-checked=mixed. */
  ARIA_CHECKED_MIXED_BRL: '(-)',
  /** Braille of element with the ARIA attribute aria-disabled=true. */
  ARIA_DISABLED_TRUE_BRL: 'xx',
  /** Braille of element with the ARIA attribute aria-expanded=true. */
  ARIA_EXPANDED_TRUE_BRL: '-',
  /** Braille of element with the ARIA attribute aria-expanded=false. */
  ARIA_EXPANDED_FALSE_BRL: '+',
  /** Braille of element with the ARIA attribute aria-invalid=true. */
  ARIA_INVALID_TRUE_BRL: '!',
  /** Braille of element with the ARIA attribute aria-pressed=true. */
  ARIA_PRESSED_TRUE_BRL: '=',
  /** Braille of element with the ARIA attribute aria-pressed=false. */
  ARIA_PRESSED_FALSE_BRL: ' ',
  /** Braille of element with the ARIA attribute aria-pressed=mixed. */
  ARIA_PRESSED_MIXED_BRL: '-',
  /** Braille of element with the ARIA attribute aria-selected=true. */
  ARIA_SELECTED_TRUE_BRL: '(x)',
  /** Braille of element with the ARIA attribute aria-selected=false. */
  ARIA_SELECTED_FALSE_BRL: '( )',
  /** Brailled after a menu if it has a submenu. */
  HAS_SUBMENU_BRL: '->',
  /** Brailled to describe a <time> tag. */
  TAG_TIME_BRL: ' ',
  /** Spoken when describing an ARIA value. */
  ARIA_VALUE_NOW: '$1',
  /** Brailled when describing an ARIA value. */
  ARIA_VALUE_NOW_BRL: '$1',
  /** Spoken when describing an ARIA value text. */
  ARIA_VALUE_TEXT: '$1',
  /** Brailled when describing an ARIA value text. */
  ARIA_VALUE_TEXT_BRL: '$1',
};
