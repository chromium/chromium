// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for walking one character at a time.
 */


goog.provide('cvox.CharacterWalker');

goog.require('cvox.AbstractSelectionWalker');
goog.require('cvox.TraverseContent');

/**
 * @constructor
 * @extends {cvox.AbstractSelectionWalker}
 */
cvox.CharacterWalker = function() {
  cvox.AbstractSelectionWalker.call(this);
  this.grain = cvox.TraverseContent.kCharacter;
};
goog.inherits(cvox.CharacterWalker, cvox.AbstractSelectionWalker);

/**
 * @override
 */
cvox.CharacterWalker.prototype.getDescription = function(prevSel, sel) {
  var desc = goog.base(this, 'getDescription', prevSel, sel);
  desc.forEach(function(item) {
    if (!item.personality) {
      item.personality = {};
    }
    item.personality['phoneticCharacters'] = true;
  });
  return desc;
};

/**
 * @override
 */
cvox.CharacterWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('character_granularity');
};
