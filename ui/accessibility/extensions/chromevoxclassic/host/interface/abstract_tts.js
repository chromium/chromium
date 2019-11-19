// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for Text-to-Speech engines that actually transform
 * text to speech.
 *
 */

goog.provide('cvox.AbstractTts');

goog.require('Msgs');
goog.require('cvox.TtsInterface');
goog.require('goog.i18n.MessageFormat');

/**
 * Creates a new instance.
 * @constructor
 * @implements {cvox.TtsInterface}
 */
cvox.AbstractTts = function() {
  this.ttsProperties = new Object();

  /**
   * Default value for TTS properties.
   * Note that these as well as the subsequent properties might be different
   * on different host platforms (like Chrome, Android, etc.).
   * @type {{pitch : number,
   *         rate: number,
   *         volume: number}}
   * @protected
   */
  this.propertyDefault = {
    'rate': 0.5,
    'pitch': 0.5,
    'volume': 0.5
  };

  /**
   * Min value for TTS properties.
   * @type {{pitch : number,
   *         rate: number,
   *         volume: number}}
   * @protected
   */
  this.propertyMin = {
    'rate': 0.0,
    'pitch': 0.0,
    'volume': 0.0
  };

  /**
   * Max value for TTS properties.
   * @type {{pitch : number,
   *         rate: number,
   *         volume: number}}
   * @protected
   */
  this.propertyMax = {
    'rate': 1.0,
    'pitch': 1.0,
    'volume': 1.0
  };

  /**
   * Step value for TTS properties.
   * @type {{pitch : number,
   *         rate: number,
   *         volume: number}}
   * @protected
   */
  this.propertyStep = {
    'rate': 0.1,
    'pitch': 0.1,
    'volume': 0.1
  };


  /** @private */

  if (cvox.AbstractTts.pronunciationDictionaryRegexp_ == undefined) {
    // Create an expression that matches all words in the pronunciation
    // dictionary on word boundaries, ignoring case.
    var words = [];
    for (var word in cvox.AbstractTts.PRONUNCIATION_DICTIONARY) {
      words.push(word);
    }
    var expr = '\\b(' + words.join('|') + ')\\b';
    cvox.AbstractTts.pronunciationDictionaryRegexp_ = new RegExp(expr, 'ig');
  }

  if (cvox.AbstractTts.substitutionDictionaryRegexp_ == undefined) {
    // Create an expression that matches all words in the substitution
    // dictionary.
    var symbols = [];
    for (var symbol in cvox.AbstractTts.SUBSTITUTION_DICTIONARY) {
      symbols.push(symbol);
    }
    var expr = '(' + symbols.join('|') + ')';
    cvox.AbstractTts.substitutionDictionaryRegexp_ = new RegExp(expr, 'ig');
  }
};


/**
 * Default TTS properties for this TTS engine.
 * @type {Object}
 * @protected
 */
cvox.AbstractTts.prototype.ttsProperties;


/** @override */
cvox.AbstractTts.prototype.speak = function(textString, queueMode, properties) {
  return this;
};


/** @override */
cvox.AbstractTts.prototype.isSpeaking = function() {
  return false;
};


/** @override */
cvox.AbstractTts.prototype.stop = function() {
};


/** @override */
cvox.AbstractTts.prototype.addCapturingEventListener = function(listener) { };


/** @override */
cvox.AbstractTts.prototype.increaseOrDecreaseProperty =
    function(propertyName, increase) {
      var min = this.propertyMin[propertyName];
      var max = this.propertyMax[propertyName];
      var step = this.propertyStep[propertyName];
      var current = this.ttsProperties[propertyName];
      current = increase ? current + step : current - step;
      this.ttsProperties[propertyName] = Math.max(Math.min(current, max), min);
    };

/**
 * Converts an engine property value to a percentage from 0.00 to 1.00.
 * @param {string} property The property to convert.
 * @return {?number} The percentage of the property.
 */
cvox.AbstractTts.prototype.propertyToPercentage = function(property) {
  return (this.ttsProperties[property] - this.propertyMin[property]) /
         Math.abs(this.propertyMax[property] - this.propertyMin[property]);
};

/**
 * Merges the given properties with the default ones. Always returns a
 * new object, so that you can safely modify the result of mergeProperties
 * without worrying that you're modifying an object used elsewhere.
 * @param {Object=} properties The properties to merge with the current ones.
 * @return {Object} The merged properties.
 * @protected
 */
cvox.AbstractTts.prototype.mergeProperties = function(properties) {
  var mergedProperties = new Object();
  var p;
  if (this.ttsProperties) {
    for (p in this.ttsProperties) {
      mergedProperties[p] = this.ttsProperties[p];
    }
  }
  if (properties) {
    var tts = cvox.AbstractTts;
    if (typeof(properties[tts.VOLUME]) == 'number') {
      mergedProperties[tts.VOLUME] = properties[tts.VOLUME];
    }
    if (typeof(properties[tts.PITCH]) == 'number') {
      mergedProperties[tts.PITCH] = properties[tts.PITCH];
    }
    if (typeof(properties[tts.RATE]) == 'number') {
      mergedProperties[tts.RATE] = properties[tts.RATE];
    }
    if (typeof(properties[tts.LANG]) == 'string') {
      mergedProperties[tts.LANG] = properties[tts.LANG];
    }

    var context = this;
    var mergeRelativeProperty = function(abs, rel) {
      if (typeof(properties[rel]) == 'number' &&
          typeof(mergedProperties[abs]) == 'number') {
        mergedProperties[abs] += properties[rel];
        var min = context.propertyMin[abs];
        var max = context.propertyMax[abs];
        if (mergedProperties[abs] > max) {
          mergedProperties[abs] = max;
        } else if (mergedProperties[abs] < min) {
          mergedProperties[abs] = min;
        }
      }
    };

    mergeRelativeProperty(tts.VOLUME, tts.RELATIVE_VOLUME);
    mergeRelativeProperty(tts.PITCH, tts.RELATIVE_PITCH);
    mergeRelativeProperty(tts.RATE, tts.RELATIVE_RATE);
  }

  for (p in properties) {
    if (!mergedProperties.hasOwnProperty(p)) {
      mergedProperties[p] = properties[p];
    }
  }

  return mergedProperties;
};


/**
 * Method to preprocess text to be spoken properly by a speech
 * engine.
 *
 * 1. Replace any single character with a description of that character.
 *
 * 2. Convert all-caps words to lowercase if they don't look like an
 *    acronym / abbreviation.
 *
 * @param {string} text A text string to be spoken.
 * @param {Object= } properties Out parameter populated with how to speak the
 *     string.
 * @return {string} The text formatted in a way that will sound better by
 *     most speech engines.
 * @protected
 */
cvox.AbstractTts.prototype.preprocess = function(text, properties) {
  if (text.length == 1 && text >= 'A' && text <= 'Z') {
    for (var prop in cvox.AbstractTts.PERSONALITY_CAPITAL)
    properties[prop] = cvox.AbstractTts.PERSONALITY_CAPITAL[prop];
  }

  // Substitute all symbols in the substitution dictionary. This is pretty
  // efficient because we use a single regexp that matches all symbols
  // simultaneously.
  text = text.replace(
      cvox.AbstractTts.substitutionDictionaryRegexp_,
      function(symbol) {
        return ' ' + cvox.AbstractTts.SUBSTITUTION_DICTIONARY[symbol] + ' ';
      });

  // Handle single characters that we want to make sure we pronounce.
  if (text.length == 1) {
    return cvox.AbstractTts.CHARACTER_DICTIONARY[text] ?
        (new goog.i18n.MessageFormat(Msgs.getMsg(
                cvox.AbstractTts.CHARACTER_DICTIONARY[text])))
            .format({'COUNT': 1}) :
        text.toUpperCase();
  }

  // Substitute all words in the pronunciation dictionary. This is pretty
  // efficient because we use a single regexp that matches all words
  // simultaneously, and it calls a function with each match, which we can
  // use to look up the replacement in our dictionary.
  text = text.replace(
      cvox.AbstractTts.pronunciationDictionaryRegexp_,
      function(word) {
        return cvox.AbstractTts.PRONUNCIATION_DICTIONARY[word.toLowerCase()];
      });

  // Expand all repeated characters.
  text = text.replace(
      cvox.AbstractTts.repetitionRegexp_, cvox.AbstractTts.repetitionReplace_);

  return text;
};


/** TTS rate property. @type {string} */
cvox.AbstractTts.RATE = 'rate';
/** TTS pitch property. @type {string} */
cvox.AbstractTts.PITCH = 'pitch';
/** TTS volume property. @type {string} */
cvox.AbstractTts.VOLUME = 'volume';
/** TTS language property. @type {string} */
cvox.AbstractTts.LANG = 'lang';

/** TTS relative rate property. @type {string} */
cvox.AbstractTts.RELATIVE_RATE = 'relativeRate';
/** TTS relative pitch property. @type {string} */
cvox.AbstractTts.RELATIVE_PITCH = 'relativePitch';
/** TTS relative volume property. @type {string} */
cvox.AbstractTts.RELATIVE_VOLUME = 'relativeVolume';

/** TTS color property (for the lens display). @type {string} */
cvox.AbstractTts.COLOR = 'color';
/** TTS CSS font-weight property (for the lens display). @type {string} */
cvox.AbstractTts.FONT_WEIGHT = 'fontWeight';

/** TTS punctuation-echo property. @type {string} */
cvox.AbstractTts.PUNCTUATION_ECHO = 'punctuationEcho';

/** TTS pause property. @type {string} */
cvox.AbstractTts.PAUSE = 'pause';

/**
 * TTS personality for annotations - text spoken by ChromeVox that
 * elaborates on a user interface element but isn't displayed on-screen.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_ANNOTATION = {
  'relativePitch': -0.25,
  // TODO:(rshearer) Added this color change for I/O presentation.
  'color': 'yellow',
  'punctuationEcho': 'none'
};


/**
 * TTS personality for announcements - text spoken by ChromeVox that
 * isn't tied to any user interface elements.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT = {
  'punctuationEcho': 'none'
};

/**
 * TTS personality for alerts from the system, such as battery level
 * warnings.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_SYSTEM_ALERT = {
  'punctuationEcho': 'none',
  'doNotInterrupt': true
};

/**
 * TTS personality for an aside - text in parentheses.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_ASIDE = {
  'relativePitch': -0.1,
  'color': '#669'
};


/**
 * TTS personality for capital letters.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_CAPITAL = {
  'relativePitch': 0.6
};


/**
 * TTS personality for deleted text.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_DELETED = {
  'punctuationEcho': 'none',
  'relativePitch': -0.6
};


/**
 * TTS personality for quoted text.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_QUOTE = {
  'relativePitch': 0.1,
  'color': '#b6b',
  'fontWeight': 'bold'
};


/**
 * TTS personality for strong or bold text.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_STRONG = {
  'relativePitch': 0.1,
  'color': '#b66',
  'fontWeight': 'bold'
};


/**
 * TTS personality for emphasis or italicized text.
 * @type {Object}
 */
cvox.AbstractTts.PERSONALITY_EMPHASIS = {
  'relativeVolume': 0.1,
  'relativeRate': -0.1,
  'color': '#6bb',
  'fontWeight': 'bold'
};


/**
 * Flag indicating if the TTS is being debugged.
 * @type {boolean}
 */
cvox.AbstractTts.DEBUG = true;


/**
 * Character dictionary. These symbols are replaced with their human readable
 * equivalents. This replacement only occurs for single character utterances.
 * @type {Object<string>}
 */
cvox.AbstractTts.CHARACTER_DICTIONARY = {
  ' ': 'space',
  '`': 'backtick',
  '~': 'tilde',
  '!': 'exclamation',
  '@': 'at',
  '#': 'pound',
  '$': 'dollar',
  '%': 'percent',
  '^': 'caret',
  '&': 'ampersand',
  '*': 'asterisk',
  '(': 'open_paren',
  ')': 'close_paren',
  '-': 'dash',
  '_': 'underscore',
  '=': 'equals',
  '+': 'plus',
  '[': 'left_bracket',
  ']': 'right_bracket',
  '{': 'left_brace',
  '}': 'right_brace',
  '|': 'pipe',
  ';': 'semicolon',
  ':': 'colon',
  ',': 'comma',
  '.': 'dot',
  '<': 'less_than',
  '>': 'greater_than',
  '/': 'slash',
  '?': 'question_mark',
  '"': 'quote',
  '\'': 'apostrophe',
  '\t': 'tab',
  '\r': 'return',
  '\n': 'new_line',
  '\\': 'backslash'
};


/**
 * Pronunciation dictionary. Each key must be lowercase, its replacement
 * should be spelled out the way most TTS engines will pronounce it
 * correctly. This particular dictionary only handles letters and numbers,
 * no symbols.
 * @type {Object<string>}
 */
cvox.AbstractTts.PRONUNCIATION_DICTIONARY = {
  'admob': 'ad-mob',
  'adsense': 'ad-sense',
  'adwords': 'ad-words',
  'angularjs': 'angular j s',
  'bcc': 'B C C',
  'cc': 'C C',
  'chromevox': 'chrome vox',
  'cr48': 'C R 48',
  'ctrl': 'control',
  'doubleclick': 'double-click',
  'gmail': 'gee mail',
  'gtalk': 'gee talk',
  'http': 'H T T P',
  'https' : 'H T T P S',
  'igoogle': 'eye google',
  'pagerank': 'page-rank',
  'username': 'user-name',
  'www': 'W W W',
  'youtube': 'you tube'
};


/**
 * Pronunciation dictionary regexp.
 * @type {RegExp};
 * @private
 */
cvox.AbstractTts.pronunciationDictionaryRegexp_;


/**
 * Substitution dictionary. These symbols or patterns are ALWAYS substituted
 * whenever they occur, so this should be reserved only for unicode characters
 * and characters that never have any different meaning in context.
 *
 * For example, do not include '$' here because $2 should be read as
 * "two dollars".
 * @type {Object<string>}
 */
cvox.AbstractTts.SUBSTITUTION_DICTIONARY = {
  '://': 'colon slash slash',
  '\u00bc': 'one fourth',
  '\u00bd': 'one half',
  '\u2190': 'left arrow',
  '\u2191': 'up arrow',
  '\u2192': 'right arrow',
  '\u2193': 'down arrow',
  '\u21d0': 'left double arrow',
  '\u21d1': 'up double arrow',
  '\u21d2': 'right double  arrow',
  '\u21d3': 'down double arrow',
  '\u21e6': 'left arrow',
  '\u21e7': 'up arrow',
  '\u21e8': 'right arrow',
  '\u21e9': 'down arrow',
  '\u2303': 'control',
  '\u2318': 'command',
  '\u2325': 'option',
  '\u25b2': 'up triangle',
  '\u25b3': 'up triangle',
  '\u25b4': 'up triangle',
  '\u25b5': 'up triangle',
  '\u25b6': 'right triangle',
  '\u25b7': 'right triangle',
  '\u25b8': 'right triangle',
  '\u25b9': 'right triangle',
  '\u25ba': 'right pointer',
  '\u25bb': 'right pointer',
  '\u25bc': 'down triangle',
  '\u25bd': 'down triangle',
  '\u25be': 'down triangle',
  '\u25bf': 'down triangle',
  '\u25c0': 'left triangle',
  '\u25c1': 'left triangle',
  '\u25c2': 'left triangle',
  '\u25c3': 'left triangle',
  '\u25c4': 'left pointer',
  '\u25c5': 'left pointer',
  '\uf8ff': 'apple'
};


/**
 * Substitution dictionary regexp.
 * @type {RegExp};
 * @private
 */
cvox.AbstractTts.substitutionDictionaryRegexp_;


/**
 * repetition filter regexp.
 * @type {RegExp}
 * @private
 */
cvox.AbstractTts.repetitionRegexp_ =
    /([-\/\\|!@#$%^&*\(\)=_+\[\]\{\}.?;'":<>])\1{2,}/g;


/**
 * Constructs a description of a repeated character. Use as a param to
 * string.replace.
 * @param {string} match The matching string.
 * @return {string} The description.
 * @private
 */
cvox.AbstractTts.repetitionReplace_ = function(match) {
  var count = match.length;
  return ' ' + (new goog.i18n.MessageFormat(Msgs.getMsg(
      cvox.AbstractTts.CHARACTER_DICTIONARY[match[0]])))
          .format({'COUNT': count}) + ' ';
};


/**
 * @override
 */
cvox.AbstractTts.prototype.getDefaultProperty = function(property) {
  return this.propertyDefault[property];
};
