// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.Statistics');

goog.require('i18n.input.chrome.TriggerType');

goog.scope(function() {
var TriggerType = i18n.input.chrome.TriggerType;



/**
 * The statistics util class for IME of ChromeOS.
 *
 * @constructor
 */
i18n.input.chrome.Statistics = function() {
};
goog.addSingletonGetter(i18n.input.chrome.Statistics);
var Statistics = i18n.input.chrome.Statistics;


/**
 * The layout types for stats.
 *
 * @enum {number}
 */
Statistics.LayoutTypes = {
  COMPACT: 0,
  COMPACT_SYMBOL: 1,
  COMPACT_MORE: 2,
  FULL: 3,
  A11Y: 4,
  HANDWRITING: 5,
  EMOJI: 6,
  MAX: 7
};


/**
 * The commit type for stats.
 * Keep this in sync with the enum IMECommitType2 in histograms.xml file in
 * chromium.
 * Please append new items at the end.
 *
 * @enum {number}
 */
Statistics.CommitTypes = {
  X_X0: 0, // User types X, and chooses X as top suggestion.
  X_Y0: 1, // User types X, and chooses Y as top suggestion.
  X_X1: 2, // User types X, and chooses X as 2nd suggestion.
  X_Y1: 3, // User types X, and chooses Y as 2nd suggestion.
  X_X2: 4, // User types X, and chooses X as 3rd/other suggestion.
  X_Y2: 5, // User types X, and chooses Y as 3rd/other suggestion.
  PREDICTION: 6,
  REVERT: 7,
  VOICE: 8,
  MAX: 9
};


/**
 * The current input method id.
 *
 * @private {string}
 */
Statistics.prototype.inputMethodId_ = '';


/**
 * The current auto correct level.
 *
 * @private {number}
 */
Statistics.prototype.autoCorrectLevel_ = 0;


/**
 * Maximum pause duration in milliseconds.
 *
 * @private {number}
 * @const
 */
Statistics.prototype.MAX_PAUSE_DURATION_ = 3000;


/**
 * Minimum words typed before committing the WPM statistic.
 *
 * @private {number}
 * @const
 */
Statistics.prototype.MIN_WORDS_FOR_WPM_ = 10;


/**
 * Timestamp of last activity.
 *
 * @private {number}
 */
Statistics.prototype.lastActivityTimeStamp_ = 0;


/**
 * Whether recording for physical keyboard specially.
 *
 * @private {boolean}
 */
Statistics.prototype.isPhysicalKeyboard_ = false;


/**
 * The length of the last text commit.
 *
 * @private {number}
 */
Statistics.prototype.lastCommitLength_ = 0;


/**
 * The number of characters typed in this session.
 *
 * @private {number}
 */
Statistics.prototype.charactersCommitted_ = 0;


/**
 * The number of characters to ignore when calculating WPM.
 *
 * @private {number}
 */
Statistics.prototype.droppedKeys_ = 0;


/**
 * Sets whether recording for physical keyboard.
 *
 * @param {boolean} isPhysicalKeyboard .
 */
Statistics.prototype.setPhysicalKeyboard = function(isPhysicalKeyboard) {
  this.isPhysicalKeyboard_ = isPhysicalKeyboard;
};


/**
 * Sets the current input method id.
 *
 * @param {string} inputMethodId .
 */
Statistics.prototype.setInputMethodId = function(
    inputMethodId) {
  this.inputMethodId_ = inputMethodId;
};


/**
 * Sets the current auto-correct level.
 *
 * @param {number} level .
 */
Statistics.prototype.setAutoCorrectLevel = function(
    level) {
  this.autoCorrectLevel_ = level;
  this.recordEnum('InputMethod.AutoCorrectLevel', level, 3);
};


/**
 * Records that the controller session ended.
 */
Statistics.prototype.recordSessionEnd = function() {
  // Do not record cases where we gain and immediately lose focus. This also
  // excudes the focus loss-gain on the new tab page from being counted.
  if (this.charactersCommitted_ > 0) {
    this.recordValue('InputMethod.VirtualKeyboard.CharactersCommitted',
        this.charactersCommitted_, 16384, 50);
  }
  this.droppedKeys_ = 0;
  this.charactersCommitted_ = 0;
  this.lastCommitLength_ = 0;
  this.lastActivityTimeStamp_ = 0;
};


/**
 * Records the metrics for each commit.
 *
 * @param {string} source .
 * @param {string} target .
 * @param {number} targetIndex The target index.
 * @param {!TriggerType} triggerType The trigger type.
 */
Statistics.prototype.recordCommit = function(
    source, target, targetIndex, triggerType) {
  if (!this.inputMethodId_) {
    return;
  }
  var CommitTypes = Statistics.CommitTypes;
  var commitType = -1;
  var length = target.length;

  if (triggerType == TriggerType.REVERT) {
    length -= this.lastCommitLength_;
    commitType = CommitTypes.REVERT;
  } else if (triggerType == TriggerType.VOICE) {
    commitType = CommitTypes.VOICE;
  } else if (triggerType == TriggerType.RESET) {
    // Increment to include space.
    length++;
  } else if (triggerType == TriggerType.CANDIDATE ||
      triggerType == TriggerType.SPACE) {
    if (!source && target) {
      commitType = CommitTypes.PREDICTION;
    } else if (targetIndex == 0 && source == target) {
      commitType = CommitTypes.X_X0;
    } else if (targetIndex == 0 && source != target) {
      commitType = CommitTypes.X_Y0;
    } else if (targetIndex == 1 && source == target) {
      commitType = CommitTypes.X_X1;
    } else if (targetIndex == 1 && source != target) {
      commitType = CommitTypes.X_Y1;
    } else if (targetIndex > 1 && source == target) {
      commitType = CommitTypes.X_X2;
    } else if (targetIndex > 1 && source != target) {
      commitType = CommitTypes.X_Y2;
    }
  }
  this.lastCommitLength_ = length;
  this.charactersCommitted_ += length;

  if (commitType < 0) {
    return;
  }

  // For latin transliteration, record the logs under the name with 'Pk' which
  // means Physical Keyboard.
  var name = this.isPhysicalKeyboard_ ?
      'InputMethod.PkCommit.' : 'InputMethod.Commit.';
  var type = this.isPhysicalKeyboard_ ? 'Type' : 'Type2';

  var self = this;
  var record = function(suffix) {
    self.recordEnum(name + 'Index' + suffix, targetIndex + 1, 20);
    self.recordEnum(name + type + suffix, commitType, CommitTypes.MAX);
  };

  record('');

  if (/^pinyin/.test(this.inputMethodId_)) {
    record('.Pinyin');
  } else if (/^xkb:us/.test(this.inputMethodId_)) {
    record('.US');
    record('.US.AC' + this.autoCorrectLevel_);
  } else if (/^xkb:fr/.test(this.inputMethodId_)) {
    record('.FR');
    record('.FR.AC' + this.autoCorrectLevel_);
  }
};


/**
 * Records the latency value for stats.
 *
 * @param {string} name .
 * @param {number} timeInMs .
 */
Statistics.prototype.recordLatency = function(
    name, timeInMs) {
  this.recordValue(name, timeInMs, 1000, 50);
};


/**
 * Gets the layout type for stats.
 *
 * @param {string} layoutCode .
 * @param {boolean} isA11yMode .
 * @return {Statistics.LayoutTypes}
 */
Statistics.prototype.getLayoutType = function(layoutCode, isA11yMode) {
  var LayoutTypes = Statistics.LayoutTypes;
  var layoutType = LayoutTypes.MAX;
  if (isA11yMode) {
    layoutType = LayoutTypes.A11Y;
  } else if (/compact/.test(layoutCode)) {
    if (/symbol/.test(layoutCode)) {
      layoutType = LayoutTypes.COMPACT_SYMBOL;
    } else if (/more/.test(layoutCode)) {
      layoutType = LayoutTypes.COMPACT_MORE;
    } else {
      layoutType = LayoutTypes.COMPACT;
    }
  } else if (/^hwt/.test(layoutCode)) {
    layoutType = LayoutTypes.HANDWRITING;
  } else if (/^emoji/.test(layoutCode)) {
    layoutType = LayoutTypes.EMOJI;
  }
  return layoutType;
};


/**
 * Records the layout usage.
 *
 * @param {string} layoutCode The layout code to be switched to.
 * @param {boolean} isA11yMode .
 */
Statistics.prototype.recordLayout = function(
    layoutCode, isA11yMode) {
  this.recordEnum('InputMethod.VirtualKeyboard.Layout',
      this.getLayoutType(layoutCode, isA11yMode), Statistics.LayoutTypes.MAX);
};

/**
 * Records enum value.
 *
 * @param {string} name .
 * @param {number} enumVal .
 * @param {number} enumCount .
 */
Statistics.prototype.recordEnum = function(
    name, enumVal, enumCount) {
  if (chrome.metricsPrivate && chrome.metricsPrivate.recordValue) {
    chrome.metricsPrivate.recordValue({
      'metricName': name,
      'type': 'histogram-linear',
      'min': 0,
      'max': enumCount - 1,
      'buckets': enumCount
    }, enumVal);
  }
};


/**
 * Records count value.
 *
 * @param {string} name .
 * @param {number} count .
 * @param {number} max .
 * @param {number} bucketCount .
 */
Statistics.prototype.recordValue = function(
    name, count, max, bucketCount) {
  if (chrome.metricsPrivate && chrome.metricsPrivate.recordValue) {
    chrome.metricsPrivate.recordValue({
      'metricName': name,
      'type': 'histogram-log',
      'min': 0,
      'max': max,
      'buckets': bucketCount
    }, count);
  }
};


/**
 * Records a key down.
 */
Statistics.prototype.recordCharacterKey = function() {
  var now = Date.now();
  if (this.lastActivityTimeStamp_) {
    if (now >= (this.lastActivityTimeStamp_ + this.MAX_PAUSE_DURATION_)) {
      // Exceeded pause duration. Ignore this character.
      this.droppedKeys_++;
    }
  } else {
    // Ignore the first character in the new session.
    this.droppedKeys_++;
  }
  this.lastActivityTimeStamp_ = now;
};
});  // goog.scope
