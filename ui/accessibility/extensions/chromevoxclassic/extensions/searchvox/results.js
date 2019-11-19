// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Provides different rules for each type of result.
 */

goog.provide('cvox.SearchResults');
goog.provide('cvox.UnknownResult');

goog.require('cvox.AbstractResult');
goog.require('cvox.ChromeVox');
goog.require('cvox.SearchUtil');

/**
 * @constructor
 */
cvox.SearchResults = function() {
};

/**
 * Speaks a result based on given selectors.
 * @param {Node} result Search result to be spoken.
 * @param {Array} selectTexts Array of selectors or text to speak.
 */
cvox.SearchResults.speakResultBySelectTexts = function(result, selectTexts) {
  for (var j = 0; j < selectTexts.length; j++) {
    var selectText = selectTexts[j];
    if (selectText.select) {
      var elems = result.querySelectorAll(selectText.select);
      for (var i = 0; i < elems.length; i++) {
        cvox.ChromeVox.speakNode(elems.item(i), cvox.QueueMode.QUEUE);
      }
    }
    if (selectText.text) {
      cvox.ChromeVox.tts.speak(selectText.text, cvox.QueueMode.QUEUE);
    }
  }
};

/**
 * Unknown Result Type. This is used if we don't know what to do.
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.UnknownResult = function() {
};
goog.inherits(cvox.UnknownResult, cvox.AbstractResult);

/* Normal Result Type. */
/**
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.NormalResult = function() {
};
goog.inherits(cvox.NormalResult, cvox.AbstractResult);

/**
 * Checks the result if it is a normal result.
 * @override
 */
cvox.NormalResult.prototype.isType = function(result) {
  var NORMAL_SELECT = '.rc';
  return result.querySelector(NORMAL_SELECT) !== null;
};

/**
 * Speak a normal search result.
 * @override
 */
cvox.NormalResult.prototype.speak = function(result) {
  if (!result) {
    return false;
  }
  var NORMAL_TITLE_SELECT = '.rc .r';
  var NORMAL_URL_SELECT = '.kv';
  var NORMAL_DESC_SELECT = '.rc .st';
  var SITE_LINK_SELECT = '.osl';
  var MORE_RESULTS_SELECT = '.sld';
  var MORE_RESULTS_LINK_SELECT = '.mrf';

  var NORMAL_SELECTORS = [
    { select: NORMAL_TITLE_SELECT },
    { select: NORMAL_DESC_SELECT },
    { select: NORMAL_URL_SELECT },
    { select: SITE_LINK_SELECT },
    { select: MORE_RESULTS_SELECT },
    { select: MORE_RESULTS_LINK_SELECT }];
  cvox.SearchResults.speakResultBySelectTexts(result, NORMAL_SELECTORS);

  var DISCUSS_TITLE_SELECT = '.mas-1st-col div';
  var DISCUSS_DATE_SELECT = '.mas-col div';
  var discussTitles = result.querySelectorAll(DISCUSS_TITLE_SELECT);
  var discussDates = result.querySelectorAll(DISCUSS_DATE_SELECT);
  for (var i = 0; i < discussTitles.length; i++) {
    cvox.ChromeVox.speakNode(discussTitles.item(i), cvox.QueueMode.QUEUE);
    cvox.ChromeVox.speakNode(discussDates.item(i), cvox.QueueMode.QUEUE);
  }
  return true;
};

/* Weather Result */
/**
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.WeatherResult = function() {
};
goog.inherits(cvox.WeatherResult, cvox.AbstractResult);

/**
 * Checks the result if it is a weather result.
 * @override
 */
cvox.WeatherResult.prototype.isType = function(result) {
  var WEATHER_SELECT = '#wob_wc';
  return result.querySelector(WEATHER_SELECT) !== null;
};

/**
 * Speak a weather forecast.
 * @param {Element} forecast Weather forecast to be spoken.
 */
cvox.WeatherResult.speakForecast = function(forecast) {
  if (!forecast) {
    return;
  }
  var FORE_DAY_SELECT = '.vk_lgy';
  var FORE_COND_SELECT = 'img';
  var FORE_HIGH_SELECT = '.vk_gy';
  var FORE_LOW_SELECT = '.vk_lgy';

  var FORE_SELECTORS = [
    { select: FORE_DAY_SELECT },
    { select: FORE_COND_SELECT },
    { select: FORE_HIGH_SELECT },
    { select: FORE_LOW_SELECT }
  ];
  cvox.SearchResults.speakResultBySelectTexts(forecast, FORE_SELECTORS);
};

/**
 * Speak a weather search result.
 * @override
 */
cvox.WeatherResult.prototype.speak = function(result) {
  if (!result) {
    return false;
  }
  /* TODO(peterxiao): Internationalization? */
  var WEATHER_INTRO = 'The weather forcast for';
  var WEATHER_TEMP_UNITS = 'degrees fahrenheit';
  var WEATHER_PREC_INTRO = 'precipitation is';
  var WEATHER_HUMID_INTRO = 'humidity is';
  var WEATHER_WIND_INTRO = 'wind is';
  var FORE_INTRO = 'Forecasts for this week';
  var WEATHER_LOC_SELECT = '.vk_h';
  var WEATHER_WHEN_SELECT = '#wob_dts';
  var WEATHER_COND_SELECT = '#wob_dc';
  var WEATHER_TEMP_SELECT = '#wob_tm';
  var WEATHER_PREC_SELECT = '#wob_pp';
  var WEATHER_HUMID_SELECT = '#wob_hm';
  var WEATHER_WIND_SELECT = '#wob_ws';

  var WEATHER_SELECT_TEXTS = [
    { text: WEATHER_INTRO },
    { select: WEATHER_LOC_SELECT },
    { select: WEATHER_WHEN_SELECT },
    { select: WEATHER_COND_SELECT },
    { select: WEATHER_TEMP_SELECT },
    { text: WEATHER_TEMP_UNITS },
    { text: WEATHER_PREC_INTRO },
    { select: WEATHER_PREC_SELECT },
    { text: WEATHER_HUMID_INTRO },
    { select: WEATHER_HUMID_SELECT },
    { text: WEATHER_WIND_INTRO },
    { select: WEATHER_WIND_SELECT }
  ];
  cvox.SearchResults.speakResultBySelectTexts(result, WEATHER_SELECT_TEXTS);

  var WEATHER_FORCAST_CLASS = 'wob_df';
  var forecasts = result.getElementsByClassName(WEATHER_FORCAST_CLASS);
  cvox.ChromeVox.tts.speak(FORE_INTRO, cvox.QueueMode.QUEUE);
  for (var i = 0; i < forecasts.length; i++) {
    var forecast = forecasts.item(i);
    cvox.WeatherResult.speakForecast(forecast);
  }
  return true;
};

/* Knowledge Panel Result */
/**
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.KnowResult = function() {
};
goog.inherits(cvox.KnowResult, cvox.AbstractResult);

/**
 * Checks the result if it is a know result.
 * @override
 */
cvox.KnowResult.prototype.isType = function(result) {
  var KNOP_SELECT = '.kno-ec';
  return result.querySelector(KNOP_SELECT) !== null;
};

/**
 * Speak a knowledge panel search result.
 * @override
 */
cvox.KnowResult.prototype.speak = function(result) {
  cvox.ChromeVox.speakNode(result, cvox.QueueMode.QUEUE);
  return true;
};

/**
 * Extracts the wikipedia URL from knowledge panel.
 * @override
 */
cvox.KnowResult.prototype.getURL = function(result) {
  var LINK_SELECTOR = '.q';
  return cvox.SearchUtil.extractURL(result.querySelector(LINK_SELECTOR));
};

/**
 * @override
 */
cvox.KnowResult.prototype.getSyncNode = function(result) {
  var HEADER_SELECTOR = '.kno-ecr-pt';
  return result.querySelector(HEADER_SELECTOR);
};

/* Calculator Type */
/**
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.CalcResult = function() {
};
goog.inherits(cvox.CalcResult, cvox.AbstractResult);

/**
 * Checks the result if it is a calculator result.
 * @override
 */
cvox.CalcResult.prototype.isType = function(result) {
  var CALC_SELECT = '#cwmcwd';
  return result.querySelector(CALC_SELECT) !== null;
};

/**
 * Speak a calculator search result.
 * @override
 */
cvox.CalcResult.prototype.speak = function(result) {
  if (!result) {
    return false;
  }
  var CALC_QUERY_SELECT = '#cwles';
  var CALC_RESULT_SELECT = '#cwos';
  var CALC_SELECTORS = [
    { select: CALC_QUERY_SELECT },
    { select: CALC_RESULT_SELECT }
  ];
  cvox.SearchResults.speakResultBySelectTexts(result, CALC_SELECTORS);
  return true;
};

/* Game Type */
/**
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.GameResult = function() {
};
goog.inherits(cvox.GameResult, cvox.AbstractResult);

/**
 * Checks the result if it is a game result.
 * @override
 */
cvox.GameResult.prototype.isType = function(result) {
  var GAME_SELECT = '.xpdbox';
  return result.querySelector(GAME_SELECT) !== null;
};

/* Image Type */
/**
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.ImageResult = function() {
};
goog.inherits(cvox.ImageResult, cvox.AbstractResult);

/**
 * Checks the result if it is a image result.
 * @override
 */
cvox.ImageResult.prototype.isType = function(result) {
  var IMAGE_CLASSES = 'rg_di';
  return result.className === IMAGE_CLASSES;
};

/**
 * Speak an image result.
 * @override
 */
cvox.ImageResult.prototype.speak = function(result) {
  if (!result) {
    return false;
  }
  /* Grab image result metadata. */
  var META_CLASS = 'rg_meta';
  var metaDiv = result.querySelector('.' + META_CLASS);
  var metaJSON = metaDiv.innerHTML;
  var metaData = JSON.parse(metaJSON);

  var imageSelectTexts = [];

  var filename = metaData['fn'];
  if (filename) {
    imageSelectTexts.push({ text: filename });
  }

  var rawDimensions = metaData['is'];
  if (rawDimensions) {
    /* Dimensions contain HTML codes, so we convert them. */
    var tmpDiv = document.createElement('div');
    tmpDiv.innerHTML = rawDimensions;
    var dimensions = tmpDiv.textContent || tmpDiv.innerText;
    imageSelectTexts.push({ text: dimensions });
  }

  var url = metaData['isu'];
  if (url) {
    imageSelectTexts.push({ text: url});
  }
  cvox.SearchResults.speakResultBySelectTexts(result, imageSelectTexts);
  return true;
};

/* Category Result */
/**
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.CategoryResult = function() {
};
goog.inherits(cvox.CategoryResult, cvox.AbstractResult);

/**
 * Checks the result if it is a category result.
 * @param {Element} result Result to be checked.
 * @return {boolean} Whether or not the element is a category result.
 * @override
 */
cvox.CategoryResult.prototype.isType = function(result) {
  var CATEGORY_CLASSES = 'rg_fbl nj';
  return result.className === CATEGORY_CLASSES;
};

/**
 * Speak a category result.
 * @override
 */
cvox.CategoryResult.prototype.speak = function(result) {
  if (!result) {
    return false;
  }
  var LABEL_SELECT = '.rg_bb_label';
  var label = result.querySelector(LABEL_SELECT);
  cvox.ChromeVox.speakNode(label, cvox.QueueMode.QUEUE);
  return true;
};

/* Ad Result */
/**
 * @constructor
 * @extends {cvox.AbstractResult}
 */
cvox.AdResult = function() {
};
goog.inherits(cvox.AdResult, cvox.AbstractResult);

/**
 * Checks the result if it is an ad result.
 * @override
 */
cvox.AdResult.prototype.isType = function(result) {
  var ADS_CLASS = 'ads-ad';
  return result.className === ADS_CLASS;
};

/**
 * Speak an ad result.
 * @override
 */
cvox.AdResult.prototype.speak = function(result) {
  if (!result) {
    return false;
  }
  var HEADER_SELECT = 'h3';
  var DESC_SELECT = '.ads-creative';
  var URL_SELECT = '.ads-visurl';
  var AD_SELECTS = [
    { select: HEADER_SELECT },
    { select: DESC_SELECT },
    { select: URL_SELECT }];
  cvox.SearchResults.speakResultBySelectTexts(result, AD_SELECTS);
  return true;
};

/**
 * To add new result types, create a new object with the following properties:
 * isType: Function to indicate if an element is the object's type.
 * speak: Function that takes in a result and speaks the type to the user.
 * getURL: Function that takes in a result and extracts the URL to follow.
 */
cvox.SearchResults.RESULT_TYPES = [
  cvox.UnknownResult,
  cvox.NormalResult,
  cvox.KnowResult,
  cvox.WeatherResult,
  cvox.AdResult,
  cvox.CalcResult,
  cvox.GameResult,
  cvox.ImageResult,
  cvox.CategoryResult
];
