// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A utility class for general braille functionality.
 */


goog.provide('cvox.BrailleUtil');

goog.require('Spannable');
goog.require('cvox.ChromeVox');
goog.require('cvox.DomUtil');
goog.require('cvox.EditableTextAreaShadow');
goog.require('cvox.Focuser');
goog.require('cvox.NavBraille');
goog.require('cvox.NodeStateUtil');
goog.require('cvox.ValueSelectionSpan');
goog.require('cvox.ValueSpan');


/**
 * Trimmable whitespace character that appears between consecutive items in
 * braille.
 * @const {string}
 */
cvox.BrailleUtil.ITEM_SEPARATOR = ' ';


/**
 * Messages considered as containers in braille.
 * Containers are distinguished from roles by their appearance higher up in the
 * DOM tree of a selected node.
 * This list should be very short.
 * @type {!Array<string>}
 */
cvox.BrailleUtil.CONTAINER = [
  'tag_h1_brl',
  'tag_h2_brl',
  'tag_h3_brl',
  'tag_h4_brl',
  'tag_h5_brl',
  'tag_h6_brl'
];


/**
 * Maps a ChromeVox message id to a braille template.
 * The template takes one-character specifiers:
 * n: replaced with braille name.
 * r: replaced with braille role.
 * s: replaced with braille state.
 * c: replaced with braille container role; this potentially returns whitespace,
 * so place at the beginning or end of templates for trimming.
 * v: replaced with braille value.
 * @type {Object<string>}
 */
cvox.BrailleUtil.TEMPLATE = {
  'base': 'c n v r s',
  'role_alert': 'r: n',
  'role_button': 'n r s',
  'role_checkbox': 'n r s',
  'role_menuitemcheckbox': 'n r s',
  'role_menuitemradio': 'n r s',
  'role_radio': 'n r s',
  'role_textbox': 'n: v r s',
  'input_type_email': 'n: v r s',
  'input_type_number': 'n: v r s',
  'input_type_password': 'n: v r s',
  'input_type_search': 'n: v r s',
  'input_type_text': 'n: v r s',
  'input_type_url': 'n: v r s',
  'tag_textarea': 'n: v r s'
};


/**
 * Gets the braille name for a node.
 * See DomUtil for a more precise definition of 'name'.
 * Additionally, whitespace is trimmed.
 * @param {Node} node The node.
 * @return {string} The string representation.
 */
cvox.BrailleUtil.getName = function(node) {
  if (!node) {
    return '';
  }
  return cvox.DomUtil.getName(node).trim();
};


/**
 * Gets the braille role message id for a node.
 * See DomUtil for a more precise definition of 'role'.
 * @param {Node} node The node.
 * @return {string} The string representation.
 */
cvox.BrailleUtil.getRoleMsg = function(node) {
  if (!node) {
    return '';
  }
  var roleMsg = cvox.DomUtil.getRoleMsg(node, cvox.VERBOSITY_VERBOSE);
  if (roleMsg) {
    roleMsg = cvox.DomUtil.collapseWhitespace(roleMsg);
  }
  if (roleMsg && (roleMsg.length > 0)) {
    if (Msgs.getMsg(roleMsg + '_brl')) {
      roleMsg += '_brl';
    }
  }
  return roleMsg;
};


/**
 * Transforms a {@code cvox.NodeState} list of state messages to the
 * corresponding messages for braille and expands them into a localized
 * string suitable for output on a braille display.
 * @param {cvox.NodeState} stateMsgs The states to expand.  The content of this
 *     array is modified.
 * @return {string} The string representation.
 * @private
 */
cvox.BrailleUtil.expandStateMsgs_ = function(stateMsgs) {
  stateMsgs.forEach(function(state) {
    // Check to see if a variant of the message with '_brl' exists,
    // and use it if so.
    //
    // Note: many messages are templatized, and if we don't pass any
    // argument to substitute, getMsg might throw an error if the
    // resulting string is empty. To avoid this, we pass a dummy
    // substitution string array here.
    var dummySubs = ['dummy', 'dummy', 'dummy'];
    if (Msgs.getMsg(state[0] + '_brl', dummySubs)) {
      state[0] += '_brl';
    }
  });
  return cvox.NodeStateUtil.expand(stateMsgs);
};


/**
 * Gets the braille container role of a node.
 * @param {Node} prev The previous node in navigation.
 * @param {Node} node The node.
 * @return {string} The string representation.
 */
cvox.BrailleUtil.getContainer = function(prev, node) {
  if (!prev || !node) {
    return '';
  }
  var ancestors = cvox.DomUtil.getUniqueAncestors(prev, node);
  for (var i = 0, container; container = ancestors[i]; i++) {
    var msg = cvox.BrailleUtil.getRoleMsg(container);
    if (msg && cvox.BrailleUtil.CONTAINER.indexOf(msg) != -1) {
      return Msgs.getMsg(msg);
    }
  }
  return '';
};


/**
 * Gets the braille value of a node. A {@code cvox.ValueSpan} will be
 * attached, along with (possibly) a {@code cvox.ValueSelectionSpan}.
 * @param {Node} node The node.
 * @return {!Spannable} The value spannable.
 */
cvox.BrailleUtil.getValue = function(node) {
  if (!node) {
    return new Spannable();
  }
  var valueSpan = new cvox.ValueSpan(0 /* offset */);
  if (cvox.DomUtil.isInputTypeText(node)) {
    var value = node.value;
    if (node.type === 'password') {
      value = value.replace(/./g, '*');
    }
    var spannable = new Spannable(value, valueSpan);
    if (node === document.activeElement &&
        cvox.DomUtil.doesInputSupportSelection(node)) {
      var selectionStart = cvox.BrailleUtil.clamp_(
          node.selectionStart, 0, spannable.length);
      var selectionEnd = cvox.BrailleUtil.clamp_(
          node.selectionEnd, 0, spannable.length);
      spannable.setSpan(new cvox.ValueSelectionSpan(),
                        Math.min(selectionStart, selectionEnd),
                        Math.max(selectionStart, selectionEnd));
    }
    return spannable;
  } else if (node instanceof HTMLTextAreaElement) {
    var shadow = new cvox.EditableTextAreaShadow();
    shadow.update(node);
    var lineIndex = shadow.getLineIndex(node.selectionEnd);
    var lineStart = shadow.getLineStart(lineIndex);
    var lineEnd = shadow.getLineEnd(lineIndex);
    var lineText = node.value.substring(lineStart, lineEnd);
    valueSpan.offset = lineStart;
    var spannable = new Spannable(lineText, valueSpan);
    if (node === document.activeElement) {
      var selectionStart = cvox.BrailleUtil.clamp_(
          node.selectionStart - lineStart, 0, spannable.length);
      var selectionEnd = cvox.BrailleUtil.clamp_(
          node.selectionEnd - lineStart, 0, spannable.length);
      spannable.setSpan(new cvox.ValueSelectionSpan(),
                        Math.min(selectionStart, selectionEnd),
                        Math.max(selectionStart, selectionEnd));
    }
    return spannable;
  } else {
    return new Spannable(cvox.DomUtil.getValue(node), valueSpan);
  }
};


/**
 * Gets the templated representation of braille.
 * @param {Node} prev The previous node (during navigation).
 * @param {Node} node The node.
 * @param {{name:(undefined|string),
 *     role:(undefined|string),
 *     roleMsg:(undefined|string),
 *     state:(undefined|string),
 *     container:(undefined|string),
 *     value:(undefined|Spannable)}|Object=} opt_override Override a
 *     specific property for the given node.
 * @return {!Spannable} The string representation.
 */
cvox.BrailleUtil.getTemplated = function(prev, node, opt_override) {
  opt_override = opt_override ? opt_override : {};
  var roleMsg = opt_override.roleMsg ||
      (node ? cvox.DomUtil.getRoleMsg(node, cvox.VERBOSITY_VERBOSE) : '');
  var template = cvox.BrailleUtil.TEMPLATE[roleMsg] ||
      cvox.BrailleUtil.TEMPLATE['base'];
  var state = opt_override.state;
  if (!state) {
    if (node) {
      state = cvox.BrailleUtil.expandStateMsgs_(
          cvox.DomUtil.getStateMsgs(node, true));
    } else {
      state = '';
    }
  }
  var role = opt_override.role || '';
  if (!role && roleMsg) {
    role = Msgs.getMsg(roleMsg + '_brl') ||
        Msgs.getMsg(roleMsg);
  }

  var templated = new Spannable();
  var mapChar = function(c) {
    switch (c) {
      case 'n':
        return opt_override.name || cvox.BrailleUtil.getName(node);
      case 'r':
        return role;
      case 's':
        return state;
      case 'c':
        return opt_override.container ||
            cvox.BrailleUtil.getContainer(prev, node);
      case 'v':
        return opt_override.value || cvox.BrailleUtil.getValue(node);
      default:
        return c;
    }
  };
  for (var i = 0; i < template.length; i++) {
    var component = mapChar(template[i]);
    templated.append(component);
    // Ignore the next whitespace separator if the current component is empty,
    // unless the empty value has a selection, in which case the cursor
    // should be placed on the empty space after the empty value.
    if (!component.toString() && template[i + 1] == ' ' &&
        (!(component instanceof Spannable) ||
        !/**@type {Spannable}*/(component).getSpanInstanceOf(
            cvox.ValueSelectionSpan))) {
      i++;
    }
  }
  return templated.trimRight();
};


/**
 * Creates a braille value from a string and, optionally, a selection range.
 * A {@code cvox.ValueSpan} will be attached, along with a
 * {@code cvox.ValueSelectionSpan} if applicable.
 * @param {string} text The text to display as the value.
 * @param {number=} opt_selStart Selection start.
 * @param {number=} opt_selEnd Selection end if different from selection start.
 * @param {number=} opt_textOffset Start offset of text.
 * @return {!Spannable} The value spannable.
 */
cvox.BrailleUtil.createValue = function(text, opt_selStart, opt_selEnd,
                                        opt_textOffset) {
  var spannable = new Spannable(text, new cvox.ValueSpan(opt_textOffset || 0));
  if (goog.isDef(opt_selStart)) {
    opt_selEnd = goog.isDef(opt_selEnd) ? opt_selEnd : opt_selStart;
    // TODO(plundblad): This looses the distinction between the selection
    // anchor (start) and focus (end).  We should use that information to
    // decide where to pan the braille display.
    if (opt_selStart > opt_selEnd) {
      var temp = opt_selStart;
      opt_selStart = opt_selEnd;
      opt_selEnd = temp;
    }

    spannable.setSpan(new cvox.ValueSelectionSpan(), opt_selStart, opt_selEnd);
  }
  return spannable;
};


/**
 * Activates a position in a nav braille.  Moves the caret in text fields
 * and simulates a mouse click on the node at the position.
 *
 * @param {!cvox.NavBraille} braille the nav braille representing the display
 *        content that was active when the user issued the key command.
 *        The annotations in the spannable are used to decide what
 *        node to activate and what part of the node value (if any) to
 *        move the caret to.
 * @param {number=} opt_displayPosition position of the display that the user
 *                  activated, relative to the start of braille.
 */
cvox.BrailleUtil.click = function(braille, opt_displayPosition) {
  var handled = false;
  var spans = braille.text.getSpans(opt_displayPosition || 0);
  var node = spans.filter(function(n) { return n instanceof Node; })[0];
  if (node) {
    if (goog.isDef(opt_displayPosition) &&
        (cvox.DomUtil.isInputTypeText(node) ||
            node instanceof HTMLTextAreaElement)) {
      var valueSpan = spans.filter(
          function(s) {
            return s instanceof cvox.ValueSpan;
          })[0];
      if (valueSpan) {
        if (document.activeElement !== node) {
          cvox.Focuser.setFocus(node);
        }
        var cursorPosition = opt_displayPosition -
            braille.text.getSpanStart(valueSpan) +
            valueSpan.offset;
        cvox.ChromeVoxEventWatcher.setUpTextHandler();
        node.selectionStart = node.selectionEnd = cursorPosition;
        cvox.ChromeVoxEventWatcher.handleTextChanged(true);
        handled = true;
      }
    }
  }
  if (!handled) {
    cvox.DomUtil.clickElem(
        node || cvox.ChromeVox.navigationManager.getCurrentNode(),
        false, false, false, true);
  }
};


/**
 * Clamps a number so it is within the given boundaries.
 * @param {number} number The number to clamp.
 * @param {number} min The minimum value to return.
 * @param {number} max The maximum value to return.
 * @return {number} {@code number} if it is within the bounds, or the nearest
 *     number within the bounds otherwise.
 * @private
 */
cvox.BrailleUtil.clamp_ = function(number, min, max) {
  return Math.min(Math.max(number, min), max);
};
