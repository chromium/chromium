// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox options page.
 *
 */

goog.provide('cvox.OptionsPage');

goog.require('Msgs');
goog.require('cvox.BrailleTable');
goog.require('cvox.BrailleTranslatorManager');
goog.require('cvox.ChromeEarcons');
goog.require('cvox.ChromeTts');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxPrefs');
goog.require('cvox.CommandStore');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.KeyMap');
goog.require('cvox.KeySequence');
goog.require('cvox.PlatformFilter');
goog.require('cvox.PlatformUtil');

/**
 * Class to manage the options page.
 * @constructor
 */
cvox.OptionsPage = function() {
};

/**
 * The ChromeVoxPrefs object.
 * @type {cvox.ChromeVoxPrefs}
 */
cvox.OptionsPage.prefs;


/**
 * A mapping from keycodes to their human readable text equivalents.
 * This is initialized in cvox.OptionsPage.init for internationalization.
 * @type {Object<string>}
 */
cvox.OptionsPage.KEYCODE_TO_TEXT = {
};

/**
 * A mapping from human readable text to keycode values.
 * This is initialized in cvox.OptionsPage.init for internationalization.
 * @type {Object<string>}
 */
cvox.OptionsPage.TEXT_TO_KEYCODE = {
};

/**
 * Initialize the options page by setting the current value of all prefs,
 * building the key bindings table, and adding event listeners.
 * @suppress {missingProperties} Property prefs never defined on Window
 */
cvox.OptionsPage.init = function() {
  cvox.OptionsPage.prefs = chrome.extension.getBackgroundPage().prefs;
  cvox.OptionsPage.populateKeyMapSelect();
  cvox.OptionsPage.addKeys();
  cvox.OptionsPage.populateVoicesSelect();
  cvox.BrailleTable.getAll(function(tables) {
    /** @type {!Array<cvox.BrailleTable.Table>} */
    cvox.OptionsPage.brailleTables = tables;
    cvox.OptionsPage.populateBrailleTablesSelect();
  });
  chrome.storage.local.get({'brailleWordWrap': true}, function(items) {
    $('brailleWordWrap').checked = items.brailleWordWrap;
  });

  Msgs.addTranslatedMessagesToDom(document);
  cvox.OptionsPage.hidePlatformSpecifics();

  cvox.OptionsPage.update();

  document.addEventListener('change', cvox.OptionsPage.eventListener, false);
  document.addEventListener('click', cvox.OptionsPage.eventListener, false);
  document.addEventListener('keydown', cvox.OptionsPage.eventListener, false);

  cvox.ExtensionBridge.addMessageListener(function(message) {
    if (message['keyBindings'] || message['prefs']) {
      cvox.OptionsPage.update();
    }
  });

  $('selectKeys').addEventListener(
      'click', cvox.OptionsPage.reset, false);

  if (cvox.PlatformUtil.matchesPlatform(cvox.PlatformFilter.WML)) {
    $('version').textContent =
        chrome.app.getDetails().version;
  }
};

/**
 * Update the value of controls to match the current preferences.
 * This happens if the user presses a key in a tab that changes a
 * pref.
 */
cvox.OptionsPage.update = function() {
  var prefs = cvox.OptionsPage.prefs.getPrefs();
  for (var key in prefs) {
    // TODO(rshearer): 'active' is a pref, but there's no place in the
    // options page to specify whether you want ChromeVox active.
    var elements = document.querySelectorAll('*[name="' + key + '"]');
    for (var i = 0; i < elements.length; i++) {
      cvox.OptionsPage.setValue(elements[i], prefs[key]);
    }
  }
};

/**
 * Populate the keymap select element with stored keymaps
 */
cvox.OptionsPage.populateKeyMapSelect = function() {
  var select = $('cvox_keymaps');
  for (var id in cvox.KeyMap.AVAILABLE_MAP_INFO) {
    var info = cvox.KeyMap.AVAILABLE_MAP_INFO[id];
    var option = document.createElement('option');
    option.id = id;
    option.className = 'i18n';
    option.setAttribute('msgid', id);
    if (localStorage['currentKeyMap'] == id) {
      option.setAttribute('selected', '');
    }
    select.appendChild(option);
  }

  select.addEventListener('change', cvox.OptionsPage.reset, true);
};

/**
 * Add the input elements for the key bindings to the container element
 * in the page. They're sorted in order of description.
 */
cvox.OptionsPage.addKeys = function() {
  var container = $('keysContainer');
  var keyMap = cvox.OptionsPage.prefs.getKeyMap();

  cvox.OptionsPage.prevTime = new Date().getTime();
  cvox.OptionsPage.keyCount = 0;
  container.addEventListener('keypress', goog.bind(function(evt) {
    if (evt.target.id == 'cvoxKey') {
      return;
    }
    this.keyCount++;
    var currentTime = new Date().getTime();
    if (currentTime - this.prevTime > 1000 || this.keyCount > 2) {
      if (document.activeElement.id == 'toggleKeyPrefix') {
        this.keySequence = new cvox.KeySequence(evt, false);
        this.keySequence.keys['ctrlKey'][0] = true;
      } else {
        this.keySequence = new cvox.KeySequence(evt, true);
      }

      this.keyCount = 1;
    } else {
      this.keySequence.addKeyEvent(evt);
    }

    var keySeqStr = cvox.KeyUtil.keySequenceToString(this.keySequence, true);
    var announce = keySeqStr.replace(/\+/g,
        ' ' + Msgs.getMsg('then') + ' ');
    announce = announce.replace(/>/g,
        ' ' + Msgs.getMsg('followed_by') + ' ');
    announce = announce.replace('Cvox',
        ' ' + Msgs.getMsg('modifier_key') + ' ');

    // TODO(dtseng): Only basic conflict detection; it does not speak the
    // conflicting command. Nor does it detect prefix conflicts like Cvox+L vs
    // Cvox+L>L.
    if (cvox.OptionsPage.prefs.setKey(document.activeElement.id,
        this.keySequence)) {
      document.activeElement.value = keySeqStr;
    } else {
      announce = Msgs.getMsg('key_conflict', [announce]);
    }
    cvox.OptionsPage.speak(announce, cvox.QueueMode.QUEUE);
    this.prevTime = currentTime;

    evt.preventDefault();
    evt.stopPropagation();
  }, cvox.OptionsPage), true);

  var categories = cvox.CommandStore.categories();
  for (var i = 0; i < categories.length; i++) {
    // Braille bindings can't be customized, so don't include them.
    if (categories[i] == 'braille') {
      continue;
    }
    var headerElement = document.createElement('h3');
    headerElement.className = 'i18n';
    headerElement.setAttribute('msgid', categories[i]);
    headerElement.id = categories[i];
    container.appendChild(headerElement);
    var commands = cvox.CommandStore.commandsForCategory(categories[i]);
    for (var j = 0; j < commands.length; j++) {
      var command = commands[j];
      // TODO: Someday we may want to have more than one key
      // mapped to a command, so we'll need to figure out how to display
      // that. For now, just take the first key.
      var keySeqObj = keyMap.keyForCommand(command)[0];

      // Explicitly skip toggleChromeVox in ChromeOS.
      if (command == 'toggleChromeVox' &&
          cvox.PlatformUtil.matchesPlatform(cvox.PlatformFilter.CHROMEOS)) {
        continue;
      }

      var inputElement = document.createElement('input');
      inputElement.type = 'text';
      inputElement.className = 'key active-key';
      inputElement.id = command;

      var displayedCombo;
      if (keySeqObj != null) {
        displayedCombo = cvox.KeyUtil.keySequenceToString(keySeqObj, true);
      } else {
        displayedCombo = '';
      }
      inputElement.value = displayedCombo;

      // Don't allow the user to change the sticky mode or stop speaking key.
      if (command == 'toggleStickyMode' || command == 'stopSpeech') {
        inputElement.disabled = true;
      }
      var message = cvox.CommandStore.messageForCommand(command);
      if (!message) {
        // TODO(dtseng): missing message id's.
        message = command;
      }

      var labelElement = document.createElement('label');
      labelElement.className = 'i18n';
      labelElement.setAttribute('msgid', message);
      labelElement.setAttribute('for', inputElement.id);

      var divElement = document.createElement('div');
      divElement.className = 'key-container';
      container.appendChild(divElement);
      divElement.appendChild(inputElement);
      divElement.appendChild(labelElement);
    }
      var brElement = document.createElement('br');
      container.appendChild(brElement);
  }

  if ($('cvoxKey') == null) {
    // Add the cvox key field
    var inputElement = document.createElement('input');
    inputElement.type = 'text';
    inputElement.className = 'key';
    inputElement.id = 'cvoxKey';

    var labelElement = document.createElement('label');
    labelElement.className = 'i18n';
    labelElement.setAttribute('msgid', 'options_cvox_modifier_key');
    labelElement.setAttribute('for', 'cvoxKey');

    var modifierSectionSibling =
        $('modifier_keys').nextSibling;
    var modifierSectionParent = modifierSectionSibling.parentNode;
    modifierSectionParent.insertBefore(labelElement, modifierSectionSibling);
    modifierSectionParent.insertBefore(inputElement, labelElement);
    var cvoxKey = $('cvoxKey');
    cvoxKey.value = localStorage['cvoxKey'];

    cvoxKey.addEventListener('keydown', function(evt) {
      if (!this.modifierSeq_) {
        this.modifierCount_ = 0;
        this.modifierSeq_ = new cvox.KeySequence(evt, false);
      } else {
        this.modifierSeq_.addKeyEvent(evt);
      }

      //  Never allow non-modified keys.
      if (!this.modifierSeq_.isAnyModifierActive()) {
        // Indicate error and instructions excluding tab.
        if (evt.keyCode != 9) {
          cvox.OptionsPage.speak(
              Msgs.getMsg('modifier_entry_error'),
              cvox.QueueMode.FLUSH, {});
        }
        this.modifierSeq_ = null;
      } else {
        this.modifierCount_++;
      }

      // Don't trap tab or shift.
      if (!evt.shiftKey && evt.keyCode != 9) {
        evt.preventDefault();
        evt.stopPropagation();
      }
    }, true);

    cvoxKey.addEventListener('keyup', function(evt) {
      if (this.modifierSeq_) {
        this.modifierCount_--;

        if (this.modifierCount_ == 0) {
          var modifierStr =
              cvox.KeyUtil.keySequenceToString(this.modifierSeq_, true, true);
          evt.target.value = modifierStr;
          cvox.OptionsPage.speak(
              Msgs.getMsg('modifier_entry_set', [modifierStr]),
              cvox.QueueMode.QUEUE);
          localStorage['cvoxKey'] = modifierStr;
          this.modifierSeq_ = null;
        }
        evt.preventDefault();
        evt.stopPropagation();
      }
    }, true);
  }
};

/**
 * Populates the voices select with options.
 */
cvox.OptionsPage.populateVoicesSelect = function() {
  var select = $('voices');

  function setVoiceList() {
    var selectedVoiceName =
        chrome.extension.getBackgroundPage()['getCurrentVoice']();
    chrome.tts.getVoices(function(voices) {
      select.innerHTML = '';
      // TODO(plundblad): voiceName can actually be omitted in the TTS engine.
      // We should generate a name in that case.
      voices.forEach(function(voice) {
        voice.voiceName = voice.voiceName || '';
      });
      voices.sort(function(a, b) {
        return a.voiceName.localeCompare(b.voiceName);
      });
      voices.forEach(function(voice) {
        var option = document.createElement('option');
        option.voiceName = voice.voiceName;
        option.innerText = option.voiceName;
        if (selectedVoiceName === voice.voiceName) {
          option.setAttribute('selected', '');
        }
        select.add(option);
      });
    });
  }

  window.speechSynthesis.onvoiceschanged = setVoiceList;
  setVoiceList();

  select.addEventListener('change', function(evt) {
    var selIndex = select.selectedIndex;
    var sel = select.options[selIndex];
    chrome.storage.local.set({voiceName: sel.voiceName});
  }, true);
};

/**
 * Populates the braille select control.
 */
cvox.OptionsPage.populateBrailleTablesSelect = function() {
  if (!cvox.ChromeVox.isChromeOS) {
    return;
  }
  var tables = cvox.OptionsPage.brailleTables;
  var populateSelect = function(node, dots) {
    var activeTable = localStorage[node.id] || localStorage['brailleTable'];
    // Gather the display names and sort them according to locale.
    var items = [];
    for (var i = 0, table; table = tables[i]; i++) {
      if (table.dots !== dots) {
        continue;
      }
      items.push({id: table.id,
                  name: cvox.BrailleTable.getDisplayName(table)});
    }
    items.sort(function(a, b) { return a.name.localeCompare(b.name);});
    for (var i = 0, item; item = items[i]; ++i) {
      var elem = document.createElement('option');
      elem.id = item.id;
      elem.textContent = item.name;
      if (item.id == activeTable) {
        elem.setAttribute('selected', '');
      }
      node.appendChild(elem);
    }
  };
  var select6 = $('brailleTable6');
  var select8 = $('brailleTable8');
  populateSelect(select6, '6');
  populateSelect(select8, '8');

  var handleBrailleSelect = function(node) {
    return function(evt) {
      var selIndex = node.selectedIndex;
      var sel = node.options[selIndex];
      localStorage['brailleTable'] = sel.id;
      localStorage[node.id] = sel.id;
      cvox.OptionsPage.getBrailleTranslatorManager().refresh();
    };
  };

  select6.addEventListener('change', handleBrailleSelect(select6), true);
  select8.addEventListener('change', handleBrailleSelect(select8), true);

  var tableTypeButton = $('brailleTableType');
  var updateTableType = function(setFocus) {
    var currentTableType = localStorage['brailleTableType'] || 'brailleTable6';
    if (currentTableType == 'brailleTable6') {
      select6.removeAttribute('aria-hidden');
      select6.setAttribute('tabIndex', 0);
      select6.style.display = 'block';
      if (setFocus) {
        select6.focus();
      }
      select8.setAttribute('aria-hidden', 'true');
      select8.setAttribute('tabIndex', -1);
      select8.style.display = 'none';
      localStorage['brailleTable'] = localStorage['brailleTable6'];
      localStorage['brailleTableType'] = 'brailleTable6';
      tableTypeButton.textContent =
          Msgs.getMsg('options_braille_table_type_6');
    } else {
      select6.setAttribute('aria-hidden', 'true');
      select6.setAttribute('tabIndex', -1);
      select6.style.display = 'none';
      select8.removeAttribute('aria-hidden');
      select8.setAttribute('tabIndex', 0);
      select8.style.display = 'block';
      if (setFocus) {
        select8.focus();
      }
      localStorage['brailleTable'] = localStorage['brailleTable8'];
      localStorage['brailleTableType'] = 'brailleTable8';
      tableTypeButton.textContent =
          Msgs.getMsg('options_braille_table_type_8');
    }
    cvox.OptionsPage.getBrailleTranslatorManager().refresh();
  };
  updateTableType(false);

  tableTypeButton.addEventListener('click', function(evt) {
    var oldTableType = localStorage['brailleTableType'];
    localStorage['brailleTableType'] =
        oldTableType == 'brailleTable6' ? 'brailleTable8' : 'brailleTable6';
    updateTableType(true);
  }, true);
};

/**
 * Set the html element for a preference to match the given value.
 * @param {Element} element The HTML control.
 * @param {string} value The new value.
 */
cvox.OptionsPage.setValue = function(element, value) {
  if (element.tagName == 'INPUT' && element.type == 'checkbox') {
    element.checked = (value == 'true');
  } else if (element.tagName == 'INPUT' && element.type == 'radio') {
    element.checked = (String(element.value) == value);
  } else {
    element.value = value;
  }
};

/**
 * Event listener, called when an event occurs in the page that might
 * affect one of the preference controls.
 * @param {Event} event The event.
 * @return {boolean} True if the default action should occur.
 */
cvox.OptionsPage.eventListener = function(event) {
  window.setTimeout(function() {
    var target = event.target;
    if (target.id == 'brailleWordWrap') {
      chrome.storage.local.set({brailleWordWrap: target.checked});
    } else if (target.classList.contains('pref')) {
      if (target.tagName == 'INPUT' && target.type == 'checkbox') {
        cvox.OptionsPage.prefs.setPref(target.name, target.checked);
      } else if (target.tagName == 'INPUT' && target.type == 'radio') {
        var key = target.name;
        var elements = document.querySelectorAll('*[name="' + key + '"]');
        for (var i = 0; i < elements.length; i++) {
          if (elements[i].checked) {
            cvox.OptionsPage.prefs.setPref(target.name, elements[i].value);
          }
        }
      }
    } else if (target.classList.contains('key')) {
      var keySeq = cvox.KeySequence.fromStr(target.value);
      var success = false;
      if (target.id == 'cvoxKey') {
        cvox.OptionsPage.prefs.setPref(target.id, target.value);
        cvox.OptionsPage.prefs.sendPrefsToAllTabs(true, true);
        success = true;
      } else {
        success =
            cvox.OptionsPage.prefs.setKey(target.id, keySeq);

        // TODO(dtseng): Don't surface conflicts until we have a better
        // workflow.
      }
    }
  }, 0);
  return true;
};

/**
 * Refreshes all dynamic content on the page.
 * This includes all key related information.
 */
cvox.OptionsPage.reset = function() {
  var selectKeyMap = $('cvox_keymaps');
  var id = selectKeyMap.options[selectKeyMap.selectedIndex].id;

  var msgs = Msgs;
  var announce = cvox.OptionsPage.prefs.getPrefs()['currentKeyMap'] == id ?
      msgs.getMsg('keymap_reset', [msgs.getMsg(id)]) :
      msgs.getMsg('keymap_switch', [msgs.getMsg(id)]);
  cvox.OptionsPage.updateStatus_(announce);

  $('keysContainer').innerHTML = '';
  cvox.OptionsPage.prefs.switchToKeyMap(id);
  cvox.OptionsPage.addKeys();
  Msgs.addTranslatedMessagesToDom(document);
};

/**
 * Updates the status live region.
 * @param {string} status The new status.
 * @private
 */
cvox.OptionsPage.updateStatus_ = function(status) {
  $('status').innerText = status;
};


/**
 * Hides all elements not matching the current platform.
 */
cvox.OptionsPage.hidePlatformSpecifics = function() {
  if (!cvox.ChromeVox.isChromeOS) {
    var elements = document.body.querySelectorAll('.chromeos');
    for (var i = 0, el; el = elements[i]; i++) {
      el.setAttribute('aria-hidden', 'true');
      el.style.display = 'none';
    }
  }
};


/**
 * Calls a {@code cvox.TtsInterface.speak} method in the background page to
 * speak an utterance.  See that method for further details.
 * @param {string} textString The string of text to be spoken.
 * @param {cvox.QueueMode} queueMode The queue mode to use.
 * @param {Object=} properties Speech properties to use for this utterance.
 */
cvox.OptionsPage.speak = function(textString, queueMode, properties) {
  var speak =
      /** @type Function} */ (chrome.extension.getBackgroundPage()['speak']);
  speak.apply(null, arguments);
};

/**
 * @return {cvox.BrailleTranslatorManager}
 */
cvox.OptionsPage.getBrailleTranslatorManager = function() {
  return chrome.extension.getBackgroundPage()['braille_translator_manager'];
};

document.addEventListener('DOMContentLoaded', function() {
  cvox.OptionsPage.init();
}, false);
