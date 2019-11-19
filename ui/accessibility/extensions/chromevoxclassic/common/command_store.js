// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview This class acts as the persistent store for all static data
 * about commands.
 *
 * This store can safely be used within either a content or background script
 * context.
 *
 * If you are looking to add a user command, follow the below steps for best
 * integration with existing components:
 * 1. Add a command below in cvox.CommandStore.CMD_WHITELIST. Pick a
 * programmatic name and fill in each of the relevant JSON keys.
 * Be sure to add a msg id and define it in chromevox/messages/messages.js which
 * describes the command. Please also add a category msg id so that the command
 * will show up in the options page.
 * 2. Add the command's logic to cvox.UserCommands inside of our switch-based
 * dispatch method (doCommand_).
 * 3. Add a key binding in chromevox/background/keymaps/classic_keymap.json and
 * chromevox/background/keymaps/flat_keymap.json.
 *
 * Class description:
 * This class is entirely static and holds a JSON structure that stores
 * commands and their associated metadata.
 *
 * From this metadata, we compute relevant subsets of data such as all present
 * categories.
 */


goog.provide('cvox.CommandStore');

goog.require('cvox.PlatformFilter');


/**
 * Returns all of the categories in the store as an array.
 * @return {Array<string>} The collection of categories.
 */
cvox.CommandStore.categories = function() {
  var categorySet = {};
  for (var cmd in cvox.CommandStore.CMD_WHITELIST) {
    var struct = cvox.CommandStore.CMD_WHITELIST[cmd];
    if (struct.category) {
      categorySet[struct.category] = true;
    }
  }
  var ret = [];
  for (var category in categorySet) {
    ret.push(category);
  }
  return ret;
};


/**
 * Gets a message given a command.
 * @param {string} command The command to query.
 * @return {string|undefined} The message id, if any.
 */
cvox.CommandStore.messageForCommand = function(command) {
  return (cvox.CommandStore.CMD_WHITELIST[command] || {}).msgId;
};


/**
 * Gets a category given a command.
 * @param {string} command The command to query.
 * @return {string|undefined} The command, if any.
 */
cvox.CommandStore.categoryForCommand = function(command) {
  return (cvox.CommandStore.CMD_WHITELIST[command] || {}).category;
};


/**
 * Gets all commands for a category.
 * @param {string} category The category to query.
 * @return {Array<string>} The commands, if any.
 */
cvox.CommandStore.commandsForCategory = function(category) {
  var ret = [];
  for (var cmd in cvox.CommandStore.CMD_WHITELIST) {
    var struct = cvox.CommandStore.CMD_WHITELIST[cmd];
    if (category == struct.category) {
      ret.push(cmd);
    }
  }
  return ret;
};


/**
 * List of commands and their properties
 * @type {Object<{forward: (undefined|boolean),
 *                backward: (undefined|boolean),
 *                announce: boolean,
 *                category: (undefined|string),
 *                findNext: (undefined|string),
 *                doDefault: (undefined|boolean),
 *                msgId: (undefined|string),
 *                nodeList: (undefined|string),
 *                platformFilter: (undefined|cvox.PlatformFilter),
 *                skipInput: (undefined|boolean),
 *                allowEvents: (undefined|boolean),
 *                disallowContinuation: (undefined|boolean)}>}
 *  forward: Whether this command points forward.
 *  backward: Whether this command points backward. If neither forward or
 *            backward are specified, it stays facing in the current direction.
 *  announce: Whether to call finishNavCommand and announce the current
 *            position after the command is done.
 *  findNext: The id from the map above if this command is used for
 *            finding next/previous of something.
 *  category: The message resource describing the command's category.
 *  doDefault: Whether to do the default action. This means that keys will be
 *             passed through to the usual DOM capture/bubble phases.
 *  msgId: The message resource describing the command.
 *  nodeList: The id from the map above if this command is used for
 *            showing a list of nodes.
 *  platformFilter: Specifies to which platforms this command applies. If left
 *                  undefined, the command applies to all platforms.
 *  skipInput: Explicitly skips this command when text input has focus.
 *             Defaults to false.
 *  disallowOOBE: Explicitly disallows this command when on chrome://oobe/*.
 *             Defaults to false.
 *  allowEvents: Allows EventWatcher to continue processing events which can
 * trump TTS.
 *  disallowContinuation: Disallows continuous read to proceed. Defaults to
 * false.
 */
cvox.CommandStore.CMD_WHITELIST = {
  'toggleStickyMode': {announce: false,
                       msgId: 'toggle_sticky_mode',
                       'disallowOOBE': true,
                       category: 'modifier_keys'},
  'toggleKeyPrefix': {announce: false,
                      skipInput: true,
                      msgId: 'prefix_key',
                         'disallowOOBE': true,
                      category: 'modifier_keys'},
  'passThroughMode': {announce: false,
                      msgId: 'pass_through_key_description',
                      category: 'modifier_keys'},

  'stopSpeech': {announce: false,
                 disallowContinuation: true,
                 doDefault: true,
                 msgId: 'stop_speech_key',
                 category: 'controlling_speech'},
  'toggleChromeVox': {announce: false,
                      platformFilter: cvox.PlatformFilter.WML,
                      msgId: 'toggle_chromevox_active',
                      category: 'modifier_keys'},
  'toggleChromeVoxVersion': {announce: false},
  'showNextUpdatePage': {
    msgId: 'show_next_update_description',
    announce: false, 'category': 'help_commands'},
  'openChromeVoxMenus': {announce: false,
                         msgId: 'menus_title'},
  'decreaseTtsRate': {announce: false,
                      msgId: 'decrease_tts_rate',
                      category: 'controlling_speech'},
  'increaseTtsRate': {announce: false,
                      msgId: 'increase_tts_rate',
                      category: 'controlling_speech'},
  'decreaseTtsPitch': {announce: false,
                      msgId: 'decrease_tts_pitch',
                      category: 'controlling_speech'},
  'increaseTtsPitch': {announce: false,
                      msgId: 'increase_tts_pitch',
                      category: 'controlling_speech'},
  'decreaseTtsVolume': {announce: false,
                      msgId: 'decrease_tts_volume',
                      category: 'controlling_speech'},
  'increaseTtsVolume': {announce: false,
                      msgId: 'increase_tts_volume',
                      category: 'controlling_speech'},
  'cyclePunctuationEcho': {announce: false,
                           msgId: 'cycle_punctuation_echo',
                           category: 'controlling_speech'},
  'cycleTypingEcho': {announce: false,
                      msgId: 'cycle_typing_echo',
                      category: 'controlling_speech'},


  'toggleEarcons': {announce: true,
                    msgId: 'toggle_earcons',
                    category: 'controlling_speech'},

  'handleTab': {
    allowEvents: true,
    msgId: 'handle_tab_next',
    disallowContinuation: true,
    category: 'navigation'},
  'handleTabPrev': {
    allowEvents: true,
    msgId: 'handle_tab_prev',
    disallowContinuation: true,
    category: 'navigation'},
  'forward': {forward: true,
              announce: true,
              msgId: 'forward',
              category: 'navigation'},
  'backward': {backward: true,
               announce: true,
               msgId: 'backward',
               category: 'navigation'},
  'right': {forward: true,
            announce: true,
            msgId: 'right',
            category: 'navigation'},
  'left': {backward: true,
           announce: true,
           msgId: 'left',
           category: 'navigation'},
  'previousGranularity': {announce: true,
                          msgId: 'previous_granularity',
                          category: 'navigation'},
  'nextGranularity': {announce: true,
                          msgId: 'next_granularity',
                          category: 'navigation'},

  'previousCharacter': {backward: true,
                        announce: true,
                        msgId: 'previous_character',
                        skipInput: true,
                        category: 'navigation'},
  'nextCharacter': {forward: true,
                    announce: true,
                    msgId: 'next_character',
                    skipInput: true,
                    category: 'navigation'},
  'previousWord': {backward: true,
                        announce: true,
                        msgId: 'previous_word',
                        skipInput: true,
                        category: 'navigation'},
  'nextWord': {forward: true,
                    announce: true,
                    msgId: 'next_word',
                    skipInput: true,
                    category: 'navigation'},
  'previousLine': {backward: true,
                        announce: true,
                        msgId: 'previous_line',
                        category: 'navigation'},
  'nextLine': {forward: true,
                    announce: true,
                    msgId: 'next_line',
                    category: 'navigation'},
  'previousSentence': {backward: true,
                        announce: true,
                        msgId: 'previous_sentence',
                        skipInput: true,
                        category: 'navigation'},
  'nextSentence': {forward: true,
                    announce: true,
                    msgId: 'next_sentence',
                    skipInput: true,
                    category: 'navigation'},
  'previousObject': {backward: true,
                        announce: true,
                        msgId: 'previous_object',
                        skipInput: true,
                        category: 'navigation'},
  'nextObject': {forward: true,
                    announce: true,
                    msgId: 'next_object',
                    skipInput: true,
                    category: 'navigation'},
  'previousGroup': {backward: true,
                        announce: true,
                        msgId: 'previous_group',
                        skipInput: true,
                        category: 'navigation'},
  'nextGroup': {forward: true,
                    announce: true,
                    msgId: 'next_group',
                    skipInput: true,
                    category: 'navigation'},

  'jumpToTop': {forward: true,
                announce: true,
                msgId: 'jump_to_top',
                category: 'navigation'
},
  'jumpToBottom': {backward: true,
                   announce: true,
                   msgId: 'jump_to_bottom',
                   category: 'navigation'},
  // Intentionally uncategorized.
  'moveToStartOfLine': {forward: true, announce: true},
  'moveToEndOfLine': {backward: true, announce: true},

  'readFromHere': {forward: true,
                   announce: false,
                   msgId: 'read_from_here',
                   category: 'navigation'},

  'performDefaultAction': {disallowContinuation: true,
                           msgId: 'perform_default_action',
                           doDefault: true,
                           skipInput: true,
                           category: 'navigation'},
  'forceClickOnCurrentItem': {announce: true,
                              disallowContinuation: true,
                              allowEvents: true,
                              msgId: 'force_click_on_current_item',
                              category: 'navigation'},
  'forceDoubleClickOnCurrentItem': {announce: true,
                                    allowEvents: true,
                                    disallowContinuation: true},

  'readLinkURL': {announce: false,
                  msgId: 'read_link_url',
                  category: 'information'},
  'readCurrentTitle': {announce: false,
                       msgId: 'read_current_title',
                       category: 'information'},
  'readCurrentURL': {announce: false,
                     msgId: 'read_current_url',
                     category: 'information'},

  'fullyDescribe': {announce: false,
                    msgId: 'fully_describe',
                    category: 'information'},
  'speakTimeAndDate': {announce: false,
                       msgId: 'speak_time_and_date',
                       category: 'information'},
  'toggleSelection': {announce: true,
                      msgId: 'toggle_selection',
                      category: 'information'},

  'toggleSearchWidget': {announce: false,
                         disallowContinuation: true,
                         msgId: 'toggle_search_widget',
                         category: 'information'},

  'toggleKeyboardHelp': {announce: false,
                         disallowContinuation: true,
                         msgId: 'show_power_key',
                         category: 'help_commands'},
  'help': {announce: false,
           msgId: 'help',
           'disallowOOBE': true,
           disallowContinuation: true,
           category: 'help_commands'},
  'contextMenu': {announce: false,
                  msgId: 'show_context_menu',
                  disallowContinuation: true},

  'showOptionsPage': {announce: false,
                      disallowContinuation: true,
                      msgId: 'show_options_page',
                      'disallowOOBE': true,
                      category: 'help_commands'},
  'showKbExplorerPage': {announce: false,
                         disallowContinuation: true,
                         msgId: 'show_kb_explorer_page',
                         'disallowOOBE': true,
                         category: 'help_commands'},


  'showFormsList': {announce: false,
                    disallowContinuation: true,
                    nodeList: 'formField',
                    msgId: 'show_forms_list',
                    category: 'overview'},
  'showHeadingsList': {announce: false, nodeList: 'heading',
                       disallowContinuation: true,
                       msgId: 'show_headings_list',
                       category: 'overview'},
  'showLandmarksList': {announce: false, nodeList: 'landmark',
                        disallowContinuation: true,
                        msgId: 'show_landmarks_list',
                        category: 'overview'},
  'showLinksList': {announce: false, nodeList: 'link',
                    disallowContinuation: true,
                    msgId: 'show_links_list',
                    category: 'overview'},
  'showTablesList': {announce: false, nodeList: 'table',
                     disallowContinuation: true,
                     msgId: 'show_tables_list',
                     category: 'overview'},

  'nextArticle': {forward: true,
                  findNext: 'article'},

  'nextButton': {forward: true,
                 findNext: 'button',
                 msgId: 'next_button',
                 category: 'jump_commands'},
  'nextCheckbox': {forward: true,
                   findNext: 'checkbox',
                   msgId: 'next_checkbox',
                   category: 'jump_commands'},
  'nextComboBox': {forward: true,
                   findNext: 'combobox',
                   msgId: 'next_combo_box',
                   category: 'jump_commands'},
  'nextControl': {forward: true, findNext: 'control'},
  'nextEditText': {forward: true,
                   findNext: 'editText',
                   msgId: 'next_edit_text',
                   category: 'jump_commands'},
  'nextFormField': {forward: true,
                    findNext: 'formField',
                    msgId: 'next_form_field',
                    category: 'jump_commands'},
  'nextGraphic': {forward: true,
                  findNext: 'graphic',
                  msgId: 'next_graphic',
                  category: 'jump_commands'},
  'nextHeading': {forward: true,
                  findNext: 'heading',
                  msgId: 'next_heading',
                  category: 'jump_commands'},
  'nextHeading1': {forward: true,
                   findNext: 'heading1',
                   msgId: 'next_heading1',
                   category: 'jump_commands'},
  'nextHeading2': {forward: true,
                   findNext: 'heading2',
                   msgId: 'next_heading2',
                   category: 'jump_commands'},
  'nextHeading3': {forward: true,
                   findNext: 'heading3',
                   msgId: 'next_heading3',
                   category: 'jump_commands'},
  'nextHeading4': {forward: true,
                   findNext: 'heading4',
                   msgId: 'next_heading4',
                   category: 'jump_commands'},
  'nextHeading5': {forward: true,
                   findNext: 'heading5',
                   msgId: 'next_heading5',
                   category: 'jump_commands'},
  'nextHeading6': {forward: true,
                   findNext: 'heading6',
                   msgId: 'next_heading6',
                   category: 'jump_commands'},

  'nextLandmark': {forward: true,
                   findNext: 'landmark',
                   msgId: 'next_landmark',
                   category: 'jump_commands'},
  'nextLink': {forward: true,
               findNext: 'link',
               msgId: 'next_link',
               category: 'jump_commands'},
  'nextList': {forward: true,
               findNext: 'list',
               msgId: 'next_list',
               category: 'jump_commands'},
  'nextListItem': {forward: true,
                   findNext: 'listItem',
                   msgId: 'next_list_item',
                   category: 'jump_commands'},
  'nextMath': {forward: true,
               findNext: 'math',
               msgId: 'next_math',
               category: 'jump_commands'},
  'nextMedia': {forward: true,
                findNext: 'media',
                msgId: 'next_media',
                category: 'jump_commands'},
  'nextRadio': {forward: true,
                findNext: 'radio',
                msgId: 'next_radio',
                category: 'jump_commands'},
  'nextSection': {forward: true, findNext: 'section'},
  'nextSlider': {forward: true, findNext: 'slider'},
  'nextTable': {forward: true,
                findNext: 'table',
                msgId: 'next_table',
                category: 'jump_commands'},
  'nextVisitedLink': {forward: true,
                findNext: 'visitedLink',
                msgId: 'next_visited_link',
                category: 'jump_commands'},


  'previousArticle': {backward: true,
                  findNext: 'article'},

  'previousButton': {backward: true,
                 findNext: 'button',
                 msgId: 'previous_button',
                 category: 'jump_commands'},
  'previousCheckbox': {backward: true,
                   findNext: 'checkbox',
                   msgId: 'previous_checkbox',
                   category: 'jump_commands'},
  'previousComboBox': {backward: true,
                   findNext: 'combobox',
                   msgId: 'previous_combo_box',
                   category: 'jump_commands'},
  'previousControl': {backward: true, findNext: 'control'},
  'previousEditText': {backward: true,
                   findNext: 'editText',
                   msgId: 'previous_edit_text',
                   category: 'jump_commands'},
  'previousFormField': {backward: true,
                    findNext: 'formField',
                    msgId: 'previous_form_field',
                    category: 'jump_commands'},
  'previousGraphic': {backward: true,
                  findNext: 'graphic',
                  msgId: 'previous_graphic',
                  category: 'jump_commands'},
  'previousHeading': {backward: true,
                  findNext: 'heading',
                  msgId: 'previous_heading',
                  category: 'jump_commands'},
  'previousHeading1': {backward: true,
                   findNext: 'heading1',
                   msgId: 'previous_heading1',
                   category: 'jump_commands'},
  'previousHeading2': {backward: true,
                   findNext: 'heading2',
                   msgId: 'previous_heading2',
                   category: 'jump_commands'},
  'previousHeading3': {backward: true,
                   findNext: 'heading3',
                   msgId: 'previous_heading3',
                   category: 'jump_commands'},
  'previousHeading4': {backward: true,
                   findNext: 'heading4',
                   msgId: 'previous_heading4',
                   category: 'jump_commands'},
  'previousHeading5': {backward: true,
                   findNext: 'heading5',
                   msgId: 'previous_heading5',
                   category: 'jump_commands'},
  'previousHeading6': {backward: true,
                   findNext: 'heading6',
                   msgId: 'previous_heading6',
                   category: 'jump_commands'},

  'previousLandmark': {backward: true,
                   findNext: 'landmark',
                   msgId: 'previous_landmark',
                   category: 'jump_commands'},
  'previousLink': {backward: true,
                   findNext: 'link',
                   msgId: 'previous_link',
                   category: 'jump_commands'},
  'previousList': {backward: true,
               findNext: 'list',
               msgId: 'previous_list',
               category: 'jump_commands'},
  'previousListItem': {backward: true,
                   findNext: 'listItem',
                   msgId: 'previous_list_item',
                   category: 'jump_commands'},
  'previousMath': {backward: true,
                   findNext: 'math',
                   msgId: 'previous_math',
                   category: 'jump_commands'},
  'previousMedia': {backward: true,
                    findNext: 'media',
                    msgId: 'previous_media',
                    category: 'jump_commands'},
  'previousRadio': {backward: true,
                findNext: 'radio',
                msgId: 'previous_radio',
                category: 'jump_commands'},
  'previousSection': {backward: true, findNext: 'section'},
  'previousSlider': {backward: true, findNext: 'slider'},
  'previousTable': {backward: true,
                findNext: 'table',
                msgId: 'previous_table',
                category: 'jump_commands'},
  'previousVisitedLink': {backward: true,
                          findNext: 'visitedLink',
                          msgId: 'previous_visited_link',
                          category: 'jump_commands'},


  // Table Actions.
  'announceHeaders': {announce: false,
                      msgId: 'announce_headers',
                      category: 'tables'},
  'speakTableLocation': {announce: false,
                         msgId: 'speak_table_location',
                         category: 'tables'},
  'goToFirstCell': {announce: true,
                    msgId: 'skip_to_beginning',
                    category: 'tables'},
  'goToLastCell': {announce: true,
                   msgId: 'skip_to_end',
                   category: 'tables'},
  'goToRowFirstCell': {announce: true,
                       msgId: 'skip_to_row_beginning',
                       category: 'tables'},
  'goToRowLastCell': {announce: true,
                      msgId: 'skip_to_row_end',
                      category: 'tables'},
  'goToColFirstCell': {announce: true,
                       msgId: 'skip_to_col_beginning',
                       category: 'tables'},
  'goToColLastCell': {announce: true,
                      msgId: 'skip_to_col_end',
                      category: 'tables'},
  // These commands are left out of the options page because they involve
  // multiple, non-user configurable modifiers.
  'previousRow': {backward: true, announce: true, skipInput: true},
  'previousCol': {backward: true, announce: true, skipInput: true},
  'nextRow': {forward: true, announce: true, skipInput: true},
  'nextCol': {forward: true, announce: true, skipInput: true},

  // Generic Actions.
  'enterShifter': {announce: true,
                   msgId: 'enter_content',
                   category: 'navigation'},
  'exitShifter': {announce: true,
                  msgId: 'exit_content',
                  category: 'navigation'},
  'exitShifterContent': {announce: true},

  'openLongDesc': {announce: false,
                   msgId: 'open_long_desc',
                   category: 'information'},

  'pauseAllMedia': {announce: false,
                    msgId: 'pause_all_media',
                    category: 'information'},

  // Math specific commands.
  'toggleSemantics': {announce: false,
                      msgId: 'toggle_semantics',
                      category: 'information'},

  // Braille specific commands.
  'routing': {announce: false,
              allowEvents: true,
              msgId: 'braille_routing',
              category: 'braille'},
  'pan_left': {backward: true,
               announce: true,
               msgId: 'braille_pan_left',
               category: 'braille'},
  'pan_right': {forward: true,
                announce: true,
                msgId: 'braille_pan_right',
                category: 'braille'},
  'line_up': {backward: true,
              announce: true,
              msgId: 'braille_line_up',
              category: 'braille'},
  'line_down': {forward: true,
                announce: true,
                msgId: 'braille_line_down',
                category: 'braille'},
  'top': {forward: true,
                announce: true,
                msgId: 'braille_top',
                category: 'braille'},
  'bottom': {backward: true,
                announce: true,
                msgId: 'braille_bottom',
                category: 'braille'},

  // Developer commands.
  'enableConsoleTts': {announce: false,
                      msgId: 'enable_tts_log',
                      category: 'developer'},
  'toggleBrailleCaptions': {announce: false,
                            msgId: 'braille_captions',
                            category: 'developer'},

  'startHistoryRecording': {announce: false},
  'stopHistoryRecording': {announce: false},
  'autorunner': {announce: false},

  'debug': {announce: false},

  'nop': {announce: false}
};


/**
 * List of find next commands and their associated data.
 * @type {Object<{predicate: string,
 *                forwardError: string,
 *                backwardError: string}>}
 *  predicate: The name of the predicate. This must be defined in DomPredicates.
 *  forwardError: The message id of the error string when moving forward.
 *  backwardError: The message id of the error string when moving backward.
 */
cvox.CommandStore.NODE_INFO_MAP = {
  'checkbox': {predicate: 'checkboxPredicate',
               forwardError: 'no_next_checkbox',
               backwardError: 'no_previous_checkbox',
               typeMsg: 'role_checkbox'},
  'radio': {predicate: 'radioPredicate',
            forwardError: 'no_next_radio_button',
            backwardError: 'no_previous_radio_button',
            typeMsg: 'role_radio'},
  'slider': {predicate: 'sliderPredicate',
             forwardError: 'no_next_slider',
             backwardError: 'no_previous_slider',
             typeMsg: 'role_slider'},
  'graphic': {predicate: 'graphicPredicate',
              forwardError: 'no_next_graphic',
              backwardError: 'no_previous_graphic',
              typeMsg: 'UNUSED'},
  'article': {predicate: 'articlePredicate',
             forwardError: 'no_next_ARTICLE',
             backwardError: 'no_previous_ARTICLE',
             typeMsg: 'TAG_ARTICLE'},
  'button': {predicate: 'buttonPredicate',
             forwardError: 'no_next_button',
             backwardError: 'no_previous_button',
             typeMsg: 'role_button'},
  'combobox': {predicate: 'comboBoxPredicate',
               forwardError: 'no_next_combo_box',
               backwardError: 'no_previous_combo_box',
               typeMsg: 'role_combobox'},
  'editText': {predicate: 'editTextPredicate',
               forwardError: 'no_next_edit_text',
               backwardError: 'no_previous_edit_text',
               typeMsg: 'input_type_text'},
  'heading': {predicate: 'headingPredicate',
              forwardError: 'no_next_heading',
              backwardError: 'no_previous_heading',
              typeMsg: 'role_heading'},
  'heading1': {predicate: 'heading1Predicate',
               forwardError: 'no_next_heading_1',
               backwardError: 'no_previous_heading_1'},
  'heading2': {predicate: 'heading2Predicate',
               forwardError: 'no_next_heading_2',
               backwardError: 'no_previous_heading_2'},
  'heading3': {predicate: 'heading3Predicate',
               forwardError: 'no_next_heading_3',
               backwardError: 'no_previous_heading_3'},
  'heading4': {predicate: 'heading4Predicate',
               forwardError: 'no_next_heading_4',
               backwardError: 'no_previous_heading_4'},
  'heading5': {predicate: 'heading5Predicate',
               forwardError: 'no_next_heading_5',
               backwardError: 'no_previous_heading_5'},
  'heading6': {predicate: 'heading6Predicate',
               forwardError: 'no_next_heading_6',
               backwardError: 'no_previous_heading_6'},

  'link': {predicate: 'linkPredicate',
           forwardError: 'no_next_link',
           backwardError: 'no_previous_link',
           typeMsg: 'role_link'},
  'table': {predicate: 'tablePredicate',
            forwardError: 'no_next_table',
            backwardError: 'no_previous_table',
            typeMsg: 'table_strategy'},
  'visitedLink': {predicate: 'visitedLinkPredicate',
            forwardError: 'no_next_visited_link',
            backwardError: 'no_previous_visited_link',
            typeMsg: 'role_link'},
  'list': {predicate: 'listPredicate',
           forwardError: 'no_next_list',
           backwardError: 'no_previous_list',
           typeMsg: 'role_list'},
  'listItem': {predicate: 'listItemPredicate',
               forwardError: 'no_next_list_item',
               backwardError: 'no_previous_list_item',
               typeMsg: 'role_listitem'},
  'formField': {predicate: 'formFieldPredicate',
                forwardError: 'no_next_form_field',
                backwardError: 'no_previous_form_field',
                typeMsg: 'role_form'},
  'landmark': {predicate: 'landmarkPredicate',
               forwardError: 'no_next_landmark',
               backwardError: 'no_previous_landmark',
               typeMsg: 'role_landmark'},
  'math': {predicate: 'mathPredicate',
           forwardError: 'no_next_math',
           backwardError: 'no_previous_math',
           typeMsg: 'math_expr'},
  'media': {predicate: 'mediaPredicate',
            forwardError: 'no_next_media_widget',
            backwardError: 'no_previous_media_widget'},
  'section': {predicate: 'sectionPredicate',
           forwardError: 'no_next_section',
           backwardError: 'no_previous_section'},
  'control': {predicate: 'controlPredicate',
           forwardError: 'no_next_control',
           backwardError: 'no_previous_control'}
};
