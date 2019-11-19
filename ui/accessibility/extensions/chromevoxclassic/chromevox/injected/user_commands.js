// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview High level commands that the user can invoke using hotkeys.
 *
 * Usage:
 * If you are here, you probably want to add a new user command. Here are some
 * general steps to get you started.
 * - Go to command_store.js, where all static data about a command lives. Follow
 * the instructions there.
 * - Add the logic of the command to doCommand_ below. Try to reuse or group
 * your command with related commands.
 */


goog.provide('cvox.ChromeVoxUserCommands');

goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.BrailleOverlayWidget');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.CommandStore');
goog.require('cvox.ConsoleTts');
goog.require('cvox.ContextMenuWidget');
goog.require('cvox.DomPredicates');
goog.require('cvox.DomUtil');
goog.require('cvox.FocusUtil');
goog.require('cvox.History');
goog.require('cvox.KeyboardHelpWidget');
goog.require('cvox.NodeSearchWidget');
goog.require('cvox.PlatformUtil');
goog.require('cvox.SearchWidget');
goog.require('cvox.SelectWidget');
goog.require('cvox.TypingEcho');
goog.require('cvox.UserEventDetail');
goog.require('goog.object');


/**
 * Initializes commands map.
 * Initializes global members.
 * @private
 */
cvox.ChromeVoxUserCommands.init_ = function() {
  cvox.ChromeVoxKbHandler.commandHandler = function(commandName) {
    var command = cvox.ChromeVoxUserCommands.commands[commandName];
    if (!command)
      return undefined;
    var history = cvox.History.getInstance();
    history.enterUserCommand(commandName);
    var ret = command();
    history.exitUserCommand(commandName);
    return ret;
  };

  if (cvox.ChromeVoxUserCommands.commands) {
    return;
  } else {
    cvox.ChromeVoxUserCommands.commands = {};
  }
  for (var cmd in cvox.CommandStore.CMD_WHITELIST) {
    cvox.ChromeVoxUserCommands.commands[cmd] =
        cvox.ChromeVoxUserCommands.createCommand_(cmd);
  }
};


/**
 * @type {!Object<function(Object=): boolean>}
 */
cvox.ChromeVoxUserCommands.commands;


/**
 * @type {boolean}
 * TODO (clchen, dmazzoni): Implement syncing on click to avoid needing this.
 */
cvox.ChromeVoxUserCommands.wasMouseClicked = false;


/**
 * @type {boolean} Flag to set whether or not certain user commands will be
 * first dispatched to the underlying web page. Some commands (such as finding
 * the next/prev structural element) may be better implemented by the web app
 * than by ChromeVox.
 *
 * By default, this is enabled; however, for testing, we usually disable this to
 * reduce flakiness caused by event timing issues.
 *
 * TODO (clchen, dtseng): Fix testing framework so that we don't need to turn
 * this feature off at all.
 */
cvox.ChromeVoxUserCommands.enableCommandDispatchingToPage = true;


/**
 * Handles any tab navigation by putting focus at the user's position.
 * This function will create dummy nodes if there is nothing that is focusable
 * at the current position.
 * TODO (adu): This function is too long. We need to break it up into smaller
 * helper functions.
 * @return {boolean} True if default action should be taken.
 * @private
 */
cvox.ChromeVoxUserCommands.handleTabAction_ = function() {
  cvox.ChromeVox.tts.stop();

  // If we are tabbing from an invalid location, prevent the default action.
  // We pass the isFocusable function as a predicate to specify we only want to
  // revert to focusable nodes.
  if (!cvox.ChromeVox.navigationManager.resolve(cvox.DomUtil.isFocusable)) {
    cvox.ChromeVox.navigationManager.setFocus();
    return false;
  }

  // If the user is already focused on anything, nothing more needs to be done.
  if (document.activeElement != document.body) {
    return true;
  }

  // Try to find something reasonable to focus on.
  // Use selection if it exists because it means that the user has probably
  // clicked with their mouse and we should respect their position.
  // If there is no selection, then use the last known position based on
  // NavigationManager's currentNode.
  var anchorNode = null;
  var focusNode = null;
  var sel = window.getSelection();
  if (!cvox.ChromeVoxUserCommands.wasMouseClicked) {
    sel = null;
  } else {
    cvox.ChromeVoxUserCommands.wasMouseClicked = false;
  }
  if (sel == null || sel.anchorNode == null || sel.focusNode == null) {
    anchorNode = cvox.ChromeVox.navigationManager.getCurrentNode();
    focusNode = cvox.ChromeVox.navigationManager.getCurrentNode();
  } else {
    anchorNode = sel.anchorNode;
    focusNode = sel.focusNode;
  }

  // See if we can set focus to either anchorNode or focusNode.
  // If not, try the parents. Otherwise give up and create a dummy span.
  if (anchorNode == null || focusNode == null) {
    return true;
  }
  if (cvox.DomUtil.isFocusable(anchorNode)) {
    anchorNode.focus();
    return true;
  }
  if (cvox.DomUtil.isFocusable(focusNode)) {
    focusNode.focus();
    return true;
  }
  if (cvox.DomUtil.isFocusable(anchorNode.parentNode)) {
    anchorNode.parentNode.focus();
    return true;
  }
  if (cvox.DomUtil.isFocusable(focusNode.parentNode)) {
    focusNode.parentNode.focus();
    return true;
  }

  // Insert and focus a dummy span immediately before the current position
  // so that the default tab action will start off as close to the user's
  // current position as possible.
  var bestGuess = anchorNode;
  var dummySpan = cvox.ChromeVoxUserCommands.createTabDummySpan_();
  bestGuess.parentNode.insertBefore(dummySpan, bestGuess);
  dummySpan.focus();
  return true;
};


/**
 * If a lingering tab dummy span exists, remove it.
 */
cvox.ChromeVoxUserCommands.removeTabDummySpan = function() {
  // Break the following line to get around a Chromium js linter warning.
  // TODO(plundblad): Find a better solution.
  var previousDummySpan = document.
      getElementById('ChromeVoxTabDummySpan');
  if (previousDummySpan && document.activeElement != previousDummySpan) {
    previousDummySpan.parentNode.removeChild(previousDummySpan);
  }
};


/**
 * Create a new tab dummy span.
 * @return {Element} The dummy span element to be inserted.
 * @private
 */
cvox.ChromeVoxUserCommands.createTabDummySpan_ = function() {
  var span = document.createElement('span');
  span.id = 'ChromeVoxTabDummySpan';
  span.tabIndex = -1;
  return span;
};


/**
 * @param {string} cmd The programmatic command name.
 * @return {function(Object=): boolean} The callable command taking an optional
 * args dictionary.
 * @private
 */
cvox.ChromeVoxUserCommands.createCommand_ = function(cmd) {
  return goog.bind(function(opt_kwargs) {
    var cmdStruct = cvox.ChromeVoxUserCommands.lookupCommand_(cmd, opt_kwargs);
    return cvox.ChromeVoxUserCommands.dispatchCommand_(cmdStruct);
  }, cvox.ChromeVoxUserCommands);
};


/**
 * @param {Object} cmdStruct The command to do.
 * @return {boolean} False to prevent the default action. True otherwise.
 * @private
 */
cvox.ChromeVoxUserCommands.dispatchCommand_ = function(cmdStruct) {
  if (cvox.Widget.isActive()) {
    return true;
  }
  if (!cvox.PlatformUtil.matchesPlatform(cmdStruct.platformFilter) ||
      (cmdStruct.skipInput && cvox.FocusUtil.isFocusInTextInputField())) {
    return true;
  }
  // Handle dispatching public command events
  if (cvox.ChromeVoxUserCommands.enableCommandDispatchingToPage &&
      (cvox.UserEventDetail.JUMP_COMMANDS.indexOf(cmdStruct.command) != -1)) {
    var detail = new cvox.UserEventDetail({command: cmdStruct.command});
    var evt = detail.createEventObject();
    var currentNode = cvox.ChromeVox.navigationManager.getCurrentNode();
    if (!currentNode) {
      currentNode = document.body;
    }
    currentNode.dispatchEvent(evt);
    return false;
  }
  // Not a public command; act on this command directly.
  return cvox.ChromeVoxUserCommands.doCommand_(cmdStruct);
};


/**
 * @param {Object} cmdStruct The command to do.
 * @return {boolean} False to prevent the default action. True otherwise.
 * @private
 */
cvox.ChromeVoxUserCommands.doCommand_ = function(cmdStruct) {
  if (cvox.Widget.isActive()) {
    return true;
  }

  if (!cvox.PlatformUtil.matchesPlatform(cmdStruct.platformFilter) ||
      (cmdStruct.skipInput && cvox.FocusUtil.isFocusInTextInputField())) {
    return true;
  }

  if (cmdStruct.disallowOOBE && document.URL.match(/^chrome:\/\/oobe/i)) {
    return true;
  }

  var cmd = cmdStruct.command;

  if (!cmdStruct.allowEvents) {
    cvox.ChromeVoxEventSuspender.enterSuspendEvents();
  }

  if (cmdStruct.disallowContinuation) {
    cvox.ChromeVox.navigationManager.stopReading(true);
  }

  if (cmdStruct.forward) {
    cvox.ChromeVox.navigationManager.setReversed(false);
  } else if (cmdStruct.backward) {
    cvox.ChromeVox.navigationManager.setReversed(true);
  }

  if (cmdStruct.findNext) {
    cmd = 'find';
    cmdStruct.announce = true;
  }

  var errorMsg = '';
  var prefixMsg = '';
  var ret = false;
  switch (cmd) {
    case 'handleTab':
    case 'handleTabPrev':
      ret = cvox.ChromeVoxUserCommands.handleTabAction_();
      break;
    case 'forward':
    case 'backward':
      ret = !cvox.ChromeVox.navigationManager.navigate();
      break;
    case 'right':
    case 'left':
      cvox.ChromeVox.navigationManager.subnavigate();
      break;
    case 'find':
      if (!cmdStruct.findNext) {
        throw 'Invalid find command.';
      }
      var NodeInfoStruct =
          cvox.CommandStore.NODE_INFO_MAP[cmdStruct.findNext];
      var predicateName = NodeInfoStruct.predicate;
      var predicate = cvox.DomPredicates[predicateName];
      var error = '';
      var wrap = '';
      if (cmdStruct.forward) {
        wrap = Msgs.getMsg('wrapped_to_top');
        error = Msgs.getMsg(NodeInfoStruct.forwardError);
      } else if (cmdStruct.backward) {
        wrap = Msgs.getMsg('wrapped_to_bottom');
        error = Msgs.getMsg(NodeInfoStruct.backwardError);
      }
      var found = null;
      var status = cmdStruct.status || cvox.UserEventDetail.Status.PENDING;
      var resultNode = cmdStruct.resultNode || null;
      switch (status) {
        case cvox.UserEventDetail.Status.SUCCESS:
          if (resultNode) {
            cvox.ChromeVox.navigationManager.updateSelToArbitraryNode(
                resultNode, true);
          }
          break;
        case cvox.UserEventDetail.Status.FAILURE:
          prefixMsg = error;
          break;
        default:
          found = cvox.ChromeVox.navigationManager.findNext(
              predicate, predicateName);
          if (!found) {
            cvox.ChromeVox.navigationManager.saveSel();
            prefixMsg = wrap;
            cvox.ChromeVox.navigationManager.syncToBeginning();
            cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.WRAP);
            found = cvox.ChromeVox.navigationManager.findNext(
                predicate, predicateName, true);
            if (!found) {
              prefixMsg = error;
              cvox.ChromeVox.navigationManager.restoreSel();
            }
          }
          break;
      }
      // NavigationManager performs announcement inside of frames when finding.
      if (found && found.start.node.tagName == 'IFRAME') {
        cmdStruct.announce = false;
      }
      break;
    // TODO(stoarca): Bad naming. Should be less instead of previous.
    case 'previousGranularity':
      cvox.ChromeVox.navigationManager.makeLessGranular(true);
      prefixMsg = cvox.ChromeVox.navigationManager.getGranularityMsg();
      break;
    case 'nextGranularity':
      cvox.ChromeVox.navigationManager.makeMoreGranular(true);
      prefixMsg = cvox.ChromeVox.navigationManager.getGranularityMsg();
      break;

    case 'previousCharacter':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.CHARACTER);
      break;
    case 'nextCharacter':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.CHARACTER);
      break;

    case 'previousWord':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.WORD);
      break;
    case 'nextWord':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.WORD);
      break;

    case 'previousSentence':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.SENTENCE);
      break;
    case 'nextSentence':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.SENTENCE);
      break;

    case 'previousLine':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.LINE);
      break;
    case 'nextLine':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.LINE);
      break;

    case 'previousObject':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.OBJECT);
      break;
    case 'nextObject':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.OBJECT);
      break;

    case 'previousGroup':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.GROUP);
      break;
    case 'nextGroup':
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.GROUP);
      break;

    case 'previousRow':
    case 'previousCol':
      // Fold these commands to their "next" equivalents since we already set
      // isReversed above.
      cmd = cmd == 'previousRow' ? 'nextRow' : 'nextCol';
    case 'nextRow':
    case 'nextCol':
      cvox.ChromeVox.navigationManager.performAction('enterShifterSilently');
      cvox.ChromeVox.navigationManager.performAction(cmd);
      break;

    case 'moveToStartOfLine':
    case 'moveToEndOfLine':
      cvox.ChromeVox.navigationManager.setGranularity(
          cvox.NavigationShifter.GRANULARITIES.LINE);
      cvox.ChromeVox.navigationManager.sync();
      cvox.ChromeVox.navigationManager.collapseSelection();
      break;

    case 'readFromHere':
      cvox.ChromeVox.navigationManager.setGranularity(
          cvox.NavigationShifter.GRANULARITIES.OBJECT, true, true);
      cvox.ChromeVox.navigationManager.startReading(
          cvox.QueueMode.FLUSH);
      break;
    case 'cycleTypingEcho':
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'Prefs',
        'action': 'setPref',
        'pref': 'typingEcho',
        'value': cvox.TypingEcho.cycle(cvox.ChromeVox.typingEcho),
        'announce': true
      });
      break;
    case 'jumpToTop':
    case cvox.BrailleKeyCommand.TOP:
      cvox.ChromeVox.navigationManager.syncToBeginning();
      break;
    case 'jumpToBottom':
    case cvox.BrailleKeyCommand.BOTTOM:
      cvox.ChromeVox.navigationManager.syncToBeginning();
      break;
    case 'stopSpeech':
      cvox.ChromeVox.navigationManager.stopReading(true);
      break;
    case 'toggleKeyboardHelp':
      cvox.KeyboardHelpWidget.getInstance().toggle();
      break;
    case 'help':
      cvox.ChromeVox.tts.stop();
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'HelpDocs',
        'action': 'open'
      });
      break;
    case 'contextMenu':
      // Move this logic to a central dispatching class if it grows any bigger.
      var node = cvox.ChromeVox.navigationManager.getCurrentNode();
      if (node.tagName == 'SELECT' && !node.multiple) {
        new cvox.SelectWidget(node).show();
      } else {
        var contextMenuWidget = new cvox.ContextMenuWidget();
        contextMenuWidget.toggle();
      }
      break;
    case 'showBookmarkManager':
      // TODO(stoarca): Should this have tts.stop()??
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'BookmarkManager',
        'action': 'open'
      });
      break;
    case 'showOptionsPage':
      cvox.ChromeVox.tts.stop();
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'Options',
        'action': 'open'
      });
      break;
    case 'showKbExplorerPage':
      cvox.ChromeVox.tts.stop();
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'KbExplorer',
        'action': 'open'
      });
      break;
    case 'readLinkURL':
      var activeElement = document.activeElement;
      var currentSelectionAnchor = window.getSelection().anchorNode;

      var url = '';
      if (activeElement.tagName == 'A') {
        url = cvox.DomUtil.getLinkURL(activeElement);
      } else if (currentSelectionAnchor) {
        url = cvox.DomUtil.getLinkURL(currentSelectionAnchor.parentNode);
      }

      if (url != '') {
        cvox.ChromeVox.tts.speak(url, cvox.QueueMode.QUEUE);
      } else {
        cvox.ChromeVox.tts.speak(Msgs.getMsg('no_url_found'),
                                 cvox.QueueMode.QUEUE);
      }
      break;
    case 'readCurrentTitle':
      cvox.ChromeVox.tts.speak(document.title, cvox.QueueMode.QUEUE);
      break;
    case 'readCurrentURL':
      cvox.ChromeVox.tts.speak(document.URL, cvox.QueueMode.QUEUE);
      break;
    case 'performDefaultAction':
      if (cvox.DomPredicates.linkPredicate([document.activeElement])) {
        if (cvox.DomUtil.isInternalLink(document.activeElement)) {
          // First, sync our selection to the destination of the internal link.
          cvox.DomUtil.syncInternalLink(document.activeElement);
          // Now, sync our selection based on the current granularity.
          cvox.ChromeVox.navigationManager.sync();
          // Announce this new selection.
          cmdStruct.announce = true;
        }
      }
      break;
    case 'forceClickOnCurrentItem':
      prefixMsg = Msgs.getMsg('element_clicked');
      var targetNode = cvox.ChromeVox.navigationManager.getCurrentNode();
      cvox.DomUtil.clickElem(targetNode, false, false);
      break;
    case 'forceDoubleClickOnCurrentItem':
      prefixMsg = Msgs.getMsg('element_double_clicked');
      var targetNode = cvox.ChromeVox.navigationManager.getCurrentNode();
      cvox.DomUtil.clickElem(targetNode, false, false, true);
      break;
    case 'toggleChromeVox':
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'Prefs',
        'action': 'setPref',
        'pref': 'active',
        'value': !cvox.ChromeVox.isActive
      });
      break;
    case 'toggleChromeVoxVersion':
    case 'showNextUpdatePage':
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'next',
        'action': 'onCommand',
        'command': cmd
      });
      break;
    case 'fullyDescribe':
      var descs = cvox.ChromeVox.navigationManager.getFullDescription();
      cvox.ChromeVox.navigationManager.speakDescriptionArray(
          descs,
          cvox.QueueMode.FLUSH,
          null);
      break;
    case 'speakTimeAndDate':
      var dateTime = new Date();
      cvox.ChromeVox.tts.speak(
          dateTime.toLocaleTimeString() + ', ' + dateTime.toLocaleDateString(),
          cvox.QueueMode.QUEUE);
      break;
    case 'toggleSelection':
      var selState = cvox.ChromeVox.navigationManager.togglePageSel();
      prefixMsg = Msgs.getMsg(
          selState ? 'begin_selection' : 'end_selection');
    break;
    case 'startHistoryRecording':
      cvox.History.getInstance().startRecording();
      break;
    case 'stopHistoryRecording':
      cvox.History.getInstance().stopRecording();
      break;
    case 'enableConsoleTts':
      cvox.ConsoleTts.getInstance().setEnabled(true);
      break;
    case 'toggleBrailleCaptions':
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'Prefs',
        'action': 'setPref',
        'pref': 'brailleCaptions',
        'value': !cvox.BrailleOverlayWidget.getInstance().isActive()
      });
      break;

    // Table actions.
    case 'goToFirstCell':
    case 'goToLastCell':
    case 'goToRowFirstCell':
    case 'goToRowLastCell':
    case 'goToColFirstCell':
    case 'goToColLastCell':
    case 'announceHeaders':
    case 'speakTableLocation':
    case 'exitShifterContent':
      if (!cvox.DomPredicates.tablePredicate(cvox.DomUtil.getAncestors(
          cvox.ChromeVox.navigationManager.getCurrentNode()))) {
        errorMsg = 'not_inside_table';
      } else if (!cvox.ChromeVox.navigationManager.performAction(cmd)) {
        errorMsg = 'not_in_table_mode';
      }
      break;

    // Generic actions.
    case 'enterShifter':
    case 'exitShifter':
      cvox.ChromeVox.navigationManager.performAction(cmd);
      break;
    // TODO(stoarca): Code repetition.
    case 'decreaseTtsRate':
      // TODO(stoarca): This function name is way too long.
      cvox.ChromeVox.tts.increaseOrDecreaseProperty(
          cvox.AbstractTts.RATE, false);
      break;
    case 'increaseTtsRate':
      cvox.ChromeVox.tts.increaseOrDecreaseProperty(
          cvox.AbstractTts.RATE, true);
      break;
    case 'decreaseTtsPitch':
      cvox.ChromeVox.tts.increaseOrDecreaseProperty(
          cvox.AbstractTts.PITCH, false);
      break;
    case 'increaseTtsPitch':
      cvox.ChromeVox.tts.increaseOrDecreaseProperty(
          cvox.AbstractTts.PITCH, true);
      break;
    case 'decreaseTtsVolume':
      cvox.ChromeVox.tts.increaseOrDecreaseProperty(
          cvox.AbstractTts.VOLUME, false);
      break;
    case 'increaseTtsVolume':
      cvox.ChromeVox.tts.increaseOrDecreaseProperty(
          cvox.AbstractTts.VOLUME, true);
      break;
      case 'cyclePunctuationEcho':
        cvox.ChromeVox.host.sendToBackgroundPage({
            'target': 'TTS',
            'action': 'cyclePunctuationEcho'
          });
        break;

    case 'toggleStickyMode':
      cvox.ChromeVox.host.sendToBackgroundPage({
        'target': 'Prefs',
        'action': 'setPref',
        'pref': 'sticky',
        'value': !cvox.ChromeVox.isStickyPrefOn,
        'announce': true
      });
      break;
    case 'toggleKeyPrefix':
      cvox.ChromeVox.keyPrefixOn = !cvox.ChromeVox.keyPrefixOn;
      break;
    case 'passThroughMode':
      cvox.ChromeVox.passThroughMode = true;
      cvox.ChromeVox.tts.speak(
          Msgs.getMsg('pass_through_key'), cvox.QueueMode.QUEUE);
      break;
    case 'toggleSearchWidget':
      cvox.SearchWidget.getInstance().toggle();
      break;

    case 'toggleEarcons':
      prefixMsg = cvox.ChromeVox.earcons.toggle() ?
          Msgs.getMsg('earcons_on') :
              Msgs.getMsg('earcons_off');
      break;

    case 'showHeadingsList':
    case 'showLinksList':
    case 'showFormsList':
    case 'showTablesList':
    case 'showLandmarksList':
      if (!cmdStruct.nodeList) {
        break;
      }
      var nodeListStruct =
          cvox.CommandStore.NODE_INFO_MAP[cmdStruct.nodeList];

      cvox.NodeSearchWidget.create(nodeListStruct.typeMsg,
                  cvox.DomPredicates[nodeListStruct.predicate]).show();
      break;

    case 'openLongDesc':
      var currentNode = cvox.ChromeVox.navigationManager.getCurrentNode();
      if (cvox.DomUtil.hasLongDesc(currentNode)) {
        cvox.ChromeVox.host.sendToBackgroundPage({
          'target': 'OpenTab',
          'url': currentNode.longDesc // Use .longDesc instead of getAttribute
                                      // since we want Chrome to convert the
                                      // longDesc to an absolute URL.
        });
      } else {
        cvox.ChromeVox.tts.speak(
          Msgs.getMsg('no_long_desc'),
          cvox.QueueMode.FLUSH,
          cvox.AbstractTts.PERSONALITY_ANNOTATION);
      }
      break;

    case 'pauseAllMedia':
      var videos = document.getElementsByTagName('VIDEO');
      for (var i = 0, mediaElem; mediaElem = videos[i]; i++) {
        mediaElem.pause();
      }
      var audios = document.getElementsByTagName('AUDIO');
      for (var i = 0, mediaElem; mediaElem = audios[i]; i++) {
        mediaElem.pause();
      }
      break;

    // Math specific commands.
    case 'toggleSemantics':
      if (cvox.TraverseMath.toggleSemantic()) {
        cvox.ChromeVox.tts.speak(Msgs.getMsg('semantics_on'),
                                 cvox.QueueMode.QUEUE);
      } else {
        cvox.ChromeVox.tts.speak(Msgs.getMsg('semantics_off'),
                                 cvox.QueueMode.QUEUE);
      }
      break;

    // Braille specific commands.
    case cvox.BrailleKeyCommand.ROUTING:
      var braille = cmdStruct.content;
      if (braille) {
        cvox.BrailleUtil.click(braille, cmdStruct.event.displayPosition);
      }
      break;
    case cvox.BrailleKeyCommand.PAN_LEFT:
    case cvox.BrailleKeyCommand.LINE_UP:
    case cvox.BrailleKeyCommand.PAN_RIGHT:
    case cvox.BrailleKeyCommand.LINE_DOWN:
      // TODO(dtseng, plundblad): This needs to sync to the last pan position
      // after line up/pan left and move the display to the far right on the
      // line in case the synced to node is longer than one display line.
      // Should also work with all widgets.
      cvox.ChromeVox.navigationManager.navigate(false,
          cvox.NavigationShifter.GRANULARITIES.LINE);
      break;

    case 'debug':
      // TODO(stoarca): This doesn't belong here.
      break;

    case 'nop':
      break;
    default:
      throw 'Command behavior not defined: ' + cmd;
  }

  if (errorMsg != '') {
    cvox.ChromeVox.tts.speak(
        Msgs.getMsg(errorMsg),
        cvox.QueueMode.FLUSH,
        cvox.AbstractTts.PERSONALITY_ANNOTATION);
  } else if (cvox.ChromeVox.navigationManager.isReading()) {
    if (cmdStruct.disallowContinuation) {
      cvox.ChromeVox.navigationManager.stopReading(true);
    } else if (cmd != 'readFromHere') {
      cvox.ChromeVox.navigationManager.skip();
    }
  } else {
    if (cmdStruct.announce) {
      cvox.ChromeVox.navigationManager.finishNavCommand(prefixMsg);
    }
  }
  if (!cmdStruct.allowEvents) {
    cvox.ChromeVoxEventSuspender.exitSuspendEvents();
  }
  return !!cmdStruct.doDefault || ret;
};


/**
 * Default handler for public user commands that are dispatched to the web app
 * first so that the web developer can handle these commands instead of
 * ChromeVox if they decide they can do a better job than the default algorithm.
 *
 * @param {Object} cvoxUserEvent The cvoxUserEvent to handle.
 */
cvox.ChromeVoxUserCommands.handleChromeVoxUserEvent = function(cvoxUserEvent) {
  var detail = new cvox.UserEventDetail(cvoxUserEvent.detail);
  if (detail.command) {
    cvox.ChromeVoxUserCommands.doCommand_(
        cvox.ChromeVoxUserCommands.lookupCommand_(detail.command, detail));
  }
};


/**
 * Returns an object containing information about the given command.
 * @param {string} cmd The name of the command.
 * @param {Object=} opt_kwargs Optional key values to add to the command
 * structure.
 * @return {Object} A key value mapping.
 * @private
 */
cvox.ChromeVoxUserCommands.lookupCommand_ = function(cmd, opt_kwargs) {
  var cmdStruct = cvox.CommandStore.CMD_WHITELIST[cmd];
  if (!cmdStruct) {
    throw 'Invalid command: ' + cmd;
  }
  cmdStruct = goog.object.clone(cmdStruct);
  cmdStruct.command = cmd;
  if (opt_kwargs) {
    goog.object.extend(cmdStruct, opt_kwargs);
  }
  return cmdStruct;
};


cvox.ChromeVoxUserCommands.init_();
