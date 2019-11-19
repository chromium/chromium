// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A set of classes to support aural CSS.
 */


goog.provide('cvox.AuralProperty');
goog.provide('cvox.AuralStyleConverter');
goog.provide('cvox.AuralStyleUtil');

// This seems the only way to lay out an enum and use it as a key.
/**
 * @enum {string}
 */
cvox.AuralProperty = {
  VOLUME: 'VOLUME',
  SPEAK: 'SPEAK',
  PAUSE_BEFORE: 'PAUSE_BEFORE',
  PAUSE_AFTER: 'PAUSE_AFTER',
  PAUSE: 'PAUSE',
  CUE_BEFORE: 'CUE_BEFORE',
  CUE_AFTER: 'CUE_AFTER',
  CUE: 'CUE',
  PLAY_DURING: 'PLAY_DURING',
  AZIMUTH: 'AZIMUTH',
  ELEVATION: 'ELEVATION',
  SPEECH_RATE: 'SPEECH_RATE',
  VOICE_FAMILY: 'VOICE_FAMILY',
  PITCH: 'PITCH',
  PITCH_RANGE: 'PITCH_RANGE',
  STRESS: 'STRESS',
  RICHNESS: 'RICHNESS',
  SPEAK_PUNCTUATION: 'SPEAK_PUNCTUATION',
  SPEAK_NUMERIAL: 'SPEAK_NUMERIAL',
  SPEAK_HEADER: 'SPEAK_HEADER',
  NONE: 'NONE'
};


/* A series of conversion functions. */
/**
 * An identity conversion.
 * @param {number} value The aural CSS value to convert.
 * @return {number} The resulting tts property value.
 */
cvox.AuralStyleConverter.identity = function(value) {
  return value;
};


/**
 * Conversion from an aural style property to Chrome TTS property.
 * TODO(dtseng): no-op's below need to be supported by the extension API itself
 * or by ChromeVox.
 * @type {Object<cvox.AuralProperty, string>}
 */
cvox.AuralStyleConverter.propertyTable = {
  VOLUME: 'volume',
  SPEAK: 'no-op',
  PAUSE_BEFORE: 'no-op',
  PAUSE_AFTER: 'no-op',
  PAUSE: 'no-op',
  CUE_BEFORE: 'no-op',
  CUE_AFTER: 'no-op',
  CUE: 'no-op',
  PLAY_DURING: 'no-op',
  AZIMUTH: 'no-op',
  ELEVATION: 'no-op',
  SPEECH_RATE: 'relativeRate',
  VOICE_FAMILY: 'no-op',
  PITCH: 'relativePitch',
  PITCH_RANGE: 'no-op',
  STRESS: 'no-op',
  RICHNESS: 'no-op',
  SPEAK_PUNCTUATION: 'no-op',
  SPEAK_NUMERIAL: 'no-op',
  SPEAK_HEADER: 'no-op',
  NONE: 'no-op'
};


/**
 * Conversion from an aural style value to Chrome TTS value.
 * TODO(dtseng): Conversion of aural CSS values is incomplete; everything is an
 * identity conversion at the moment.
 * @type {Object<cvox.AuralProperty, function(*)>}
 */
cvox.AuralStyleConverter.valueTable = {
  VOLUME: cvox.AuralStyleConverter.identity,
  SPEAK: cvox.AuralStyleConverter.identity,
  PAUSE_BEFORE: cvox.AuralStyleConverter.identity,
  PAUSE_AFTER: cvox.AuralStyleConverter.identity,
  PAUSE: cvox.AuralStyleConverter.identity,
  CUE_BEFORE: cvox.AuralStyleConverter.identity,
  CUE_AFTER: cvox.AuralStyleConverter.identity,
  CUE: cvox.AuralStyleConverter.identity,
  PLAY_DURING: cvox.AuralStyleConverter.identity,
  AZIMUTH: cvox.AuralStyleConverter.identity,
  ELEVATION: cvox.AuralStyleConverter.identity,
  SPEECH_RATE: cvox.AuralStyleConverter.identity,
  VOICE_FAMILY: cvox.AuralStyleConverter.identity,
  PITCH: cvox.AuralStyleConverter.identity,
  PITCH_RANGE: cvox.AuralStyleConverter.identity,
  STRESS: cvox.AuralStyleConverter.identity,
  RICHNESS: cvox.AuralStyleConverter.identity,
  SPEAK_PUNCTUATION: cvox.AuralStyleConverter.identity,
  SPEAK_NUMERIAL: cvox.AuralStyleConverter.identity,
  SPEAK_HEADER: cvox.AuralStyleConverter.identity,
  NONE: cvox.AuralStyleConverter.identity
};


/**
 * Converts a given aural property/value rule to a tts property/value.
 * @param {cvox.AuralProperty} property The property.
 * @param {*} value The CSS-based value.
 * @return {Object} An object holding tts property and value.
 */
cvox.AuralStyleConverter.convertRule = function(property, value) {
  return {
    property: cvox.AuralStyleConverter.propertyTable[property],
    value: cvox.AuralStyleConverter.valueTable[property](value)
  };
};


/**
 * Converts an aural CSS style block to a TTS property object.
 * @param {Object<cvox.AuralProperty, *>} style The style.
 * @return {Object} The tts property object.
 */
cvox.AuralStyleConverter.convertStyle = function(style) {
  var ttsProperties = {};
  for (var property in style) {
    var ttsProperty =
        cvox.AuralStyleConverter.convertRule(property, style[property]);
    ttsProperties[ttsProperty.property] = ttsProperty.value;
  }
  return ttsProperties;
};


/**
 * Gets the aural style for a node.
 * @param {Node} node The node.
 * @return {Object} The aural style, converted to tts properties.
*/
cvox.AuralStyleUtil.getStyleForNode = function(node) {
  var style = cvox.AuralStyleUtil.defaultStyles[node.tagName];
  if (!style) {
    return null;
  }
  return cvox.AuralStyleConverter.convertStyle(style);
};


/**
 * A list of default aural styles.
 */
cvox.AuralStyleUtil.defaultStyles = {
  'ARTICLE': {
    PITCH: -0.1
  },
  'ASIDE': {
    PITCH: -0.1
  },
  'FOOTER': {
    PITCH: -0.1
  },
  'H1': {
    PITCH: -0.3
  },
  'H2': {
    PITCH: -0.25
  },
  'H3': {
    PITCH: -0.2
  },
  'H4': {
    PITCH: -0.15
  },
  'H5': {
    PITCH: -0.1
  },
  'H6': {
    PITCH: -0.05
  },
  'HEADER': {
    PITCH: -0.1
  },
  'HGROUP': {
    PITCH: -0.1
  },
  'MARK': {
    PITCH: -0.1
  },
  'NAV': {
    PITCH: -0.1
  },
  'SECTION': {
    PITCH: -0.1
  },
  'TIME': {
    PITCH: -0.1
  }
};
