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
goog.provide('i18n.input.chrome.inputview.Controller');

goog.require('goog.Disposable');
goog.require('goog.Timer');
goog.require('goog.array');
goog.require('goog.async.Delay');
goog.require('goog.dom.classlist');
goog.require('goog.events.Event');
goog.require('goog.events.EventHandler');
goog.require('goog.events.EventType');
goog.require('goog.events.KeyCodes');
goog.require('goog.fx.easing');
goog.require('goog.i18n.bidi');
goog.require('goog.math.Coordinate');
goog.require('goog.object');
goog.require('i18n.input.chrome.DataSource');
goog.require('i18n.input.chrome.ElementType');
goog.require('i18n.input.chrome.FeatureName');
goog.require('i18n.input.chrome.Statistics');
goog.require('i18n.input.chrome.events.KeyCodes');
goog.require('i18n.input.chrome.inputview.Adapter');
goog.require('i18n.input.chrome.inputview.CandidatesInfo');
goog.require('i18n.input.chrome.inputview.ConditionName');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.KeyboardContainer');
goog.require('i18n.input.chrome.inputview.M17nModel');
goog.require('i18n.input.chrome.inputview.Model');
goog.require('i18n.input.chrome.inputview.PerfTracker');
goog.require('i18n.input.chrome.inputview.ReadyState');
goog.require('i18n.input.chrome.inputview.Settings');
goog.require('i18n.input.chrome.inputview.SizeSpec');
goog.require('i18n.input.chrome.inputview.SpecNodeName');
goog.require('i18n.input.chrome.inputview.StateType');
goog.require('i18n.input.chrome.inputview.SwipeDirection');
goog.require('i18n.input.chrome.inputview.elements.content.Candidate');
goog.require('i18n.input.chrome.inputview.elements.content.CandidateView');
goog.require('i18n.input.chrome.inputview.elements.content.ExpandedCandidateView');
goog.require('i18n.input.chrome.inputview.elements.content.MenuView');
goog.require('i18n.input.chrome.inputview.events.EventType');
goog.require('i18n.input.chrome.inputview.handler.PointerHandler');
goog.require('i18n.input.chrome.inputview.util');
goog.require('i18n.input.chrome.message.ContextType');
goog.require('i18n.input.chrome.message.Name');
goog.require('i18n.input.chrome.message.Type');
goog.require('i18n.input.chrome.sounds.SoundController');
goog.require('i18n.input.lang.InputToolCode');



goog.scope(function() {
var CandidateType = i18n.input.chrome.inputview.elements.content.Candidate.Type;
var CandidateView = i18n.input.chrome.inputview.elements.content.CandidateView;
var ConditionName = i18n.input.chrome.inputview.ConditionName;
var ContextType = i18n.input.chrome.message.ContextType;
var Css = i18n.input.chrome.inputview.Css;
var ElementType = i18n.input.chrome.ElementType;
var EventType = i18n.input.chrome.inputview.events.EventType;
var ExpandedCandidateView = i18n.input.chrome.inputview.elements.content.
    ExpandedCandidateView;
var FeatureName = i18n.input.chrome.FeatureName;
var InputToolCode = i18n.input.lang.InputToolCode;
var KeyCodes = i18n.input.chrome.events.KeyCodes;
var MenuView = i18n.input.chrome.inputview.elements.content.MenuView;
var Name = i18n.input.chrome.message.Name;
var PerfTracker = i18n.input.chrome.inputview.PerfTracker;
var SizeSpec = i18n.input.chrome.inputview.SizeSpec;
var SpecNodeName = i18n.input.chrome.inputview.SpecNodeName;
var StateType = i18n.input.chrome.inputview.StateType;
var SoundController = i18n.input.chrome.sounds.SoundController;
var Type = i18n.input.chrome.message.Type;
var events = i18n.input.chrome.inputview.events;
var util = i18n.input.chrome.inputview.util;


/**
 * Time in milliseconds after which backspace will start autorepeating.
 * @const {number}
 */
var BACKSPACE_REPEAT_START_TIME_MS = 300;


/**
 * Minimum time, in milliseconds, after which backspace can repeat. This
 * prevents deleting entire pages in a go.
 * @const {number}
 */
var MINIMUM_BACKSPACE_REPEAT_TIME_MS = 25;


/**
 * Maximum time, in milliseconds, after which the backspace can repeat.
 * @const {number}
 */
var MAXIMUM_BACKSPACE_REPEAT_TIME_MS = 75;


/**
 * The limit for backspace repeat for speeding up.
 * The backspace repeat beyond this limit will have the maximum speed.
 * @const {number}
 */
var BACKSPACE_REPEAT_LIMIT = 255;


/**
 * The time, in milliseconds, that the gesture preview window lingers before
 * being dismissed.
 *
 * @const {number}
 */
var GESTURE_PREVIEW_LINGER_TIME_MS = 250;


/**
 * The maximum distance from the top of the keyboard that a gesture can move
 * before it cancels the gesture typing gesture.
 *
 * @const {number}
 */
var MAXIMUM_DISTANCE_FROM_TOP_FOR_GESTURES = 40;


/**
 * The hotrod customized layout code.
 *
 * @const {string}
 */
var HOTROD_DEFAULT_KEYSET = 'hotrod';



/**
 * The controller of the input view keyboard.
 *
 * @param {string} keyset The keyboard keyset.
 * @param {string} languageCode The language code for this keyboard.
 * @param {string} passwordLayout The layout for password box.
 * @param {string} name The input tool name.
 * @constructor
 * @extends {goog.Disposable}
 */
i18n.input.chrome.inputview.Controller = function(keyset, languageCode,
    passwordLayout, name) {
  /**
   * The model.
   *
   * @type {!i18n.input.chrome.inputview.Model}
   * @private
   */
  this.model_ = new i18n.input.chrome.inputview.Model();

  /** @private {!i18n.input.chrome.inputview.PerfTracker} */
  this.perfTracker_ = new i18n.input.chrome.inputview.PerfTracker(
      PerfTracker.TickName.HTML_LOADED);

  /**
   * The layout map.
   *
   * @type {!Object.<string, !Object>}
   * @private
   */
  this.layoutDataMap_ = {};

  /**
   * The element map.
   *
   * @private {!Object.<ElementType, !KeyCodes>}
   */
  this.elementTypeToKeyCode_ = goog.object.create(
      ElementType.BOLD, KeyCodes.KEY_B,
      ElementType.ITALICS, KeyCodes.KEY_I,
      ElementType.UNDERLINE, KeyCodes.KEY_U,
      ElementType.COPY, KeyCodes.KEY_C,
      ElementType.PASTE, KeyCodes.KEY_V,
      ElementType.CUT, KeyCodes.KEY_X,
      ElementType.SELECT_ALL, KeyCodes.KEY_A,
      ElementType.REDO, KeyCodes.KEY_Y,
      ElementType.UNDO, KeyCodes.KEY_Z
      );

  /**
   * The keyset data map.
   *
   * @type {!Object.<string, !Object>}
   * @private
   */
  this.keysetDataMap_ = {};

  /**
   * The event handler.
   *
   * @type {!goog.events.EventHandler}
   * @private
   */
  this.handler_ = new goog.events.EventHandler(this);

  /**
   * The m17n model.
   *
   * @type {!i18n.input.chrome.inputview.M17nModel}
   * @private
   */
  this.m17nModel_ = new i18n.input.chrome.inputview.M17nModel();

  /**
   * The pointer handler.
   *
   * @type {!i18n.input.chrome.inputview.handler.PointerHandler}
   * @private
   */
  this.pointerHandler_ = new i18n.input.chrome.inputview.handler.
      PointerHandler();

  /**
   * The statistics object for recording metrics values.
   *
   * @type {!i18n.input.chrome.Statistics}
   * @private
   */
  this.statistics_ = i18n.input.chrome.Statistics.getInstance();

  /** @private {!i18n.input.chrome.inputview.ReadyState} */
  this.readyState_ = new i18n.input.chrome.inputview.ReadyState();

  /** @private {!i18n.input.chrome.inputview.Adapter} */
  this.adapter_ = new i18n.input.chrome.inputview.Adapter(this.readyState_);

  /** @private {!SoundController} */
  this.soundController_ = new SoundController(false);

  /** @private {!i18n.input.chrome.inputview.KeyboardContainer} */
  this.container_ = new i18n.input.chrome.inputview.KeyboardContainer(
      this.adapter_, this.soundController_);

  /**
   * The context type and keyset mapping group by input method id.
   * key: input method id.
   * value: Object
   *    key: context type string.
   *    value: keyset string.
   *
   * @private {!Object.<string, !Object.<string, string>>}
   */
  this.contextTypeToKeysetMap_ = {};


  /**
   * The previous raw keyset code before switched to hwt or emoji layout.
   *  key: context type string.
   *  value: keyset string.
   *
   * @private {!Object.<string, string>}
   */
  this.contextTypeToLastKeysetMap_ = {};

  /**
   * The stats map for input view closing.
   *
   * @type {!Object.<string, !Array.<number>>}
   * @private
   */
  this.statsForClosing_ = {};

  /**
   * The time the last point was accepted.
   *
   * @private {number}
   */
  this.lastPointTime_ = 0;

  /**
   * The last height sent to window.resizeTo to avoid multiple equivalent calls.
   *
   * @private {number}
   */
  this.lastResizeHeight_ = -1;

  /**
   * The activate (show) time stamp for statistics.
   *
   * @type {Date}
   * @private
   */
  this.showTimeStamp_ = new Date();

  this.initialize(keyset, languageCode, passwordLayout, name);
  /**
   * The suggestions.
   * Note: sets a default empty result to avoid null check.
   *
   * @private {!i18n.input.chrome.inputview.CandidatesInfo}
   */
  this.candidatesInfo_ = i18n.input.chrome.inputview.CandidatesInfo.getEmpty();

  this.registerEventHandler_();
};
goog.inherits(i18n.input.chrome.inputview.Controller, goog.Disposable);
var Controller = i18n.input.chrome.inputview.Controller;
var Statistics = i18n.input.chrome.Statistics;

/**
 * @define {boolean} Flag to disable delayed loading of non active keyset. It
 * should only be used in testing.
 */
Controller.DISABLE_DELAY_LOADING_FOR_TEST = false;


/**
 * @define {boolean} Flag to disable handwriting.
 */
Controller.DISABLE_HWT = false;


/**
 * A flag to indicate whether the shift is for auto capital.
 *
 * @private {boolean}
 */
Controller.prototype.shiftForAutoCapital_ = false;


/**
 * @define {boolean} Flag to indicate whether it is debugging.
 */
Controller.DEV = false;


/**
 * The handwriting view code, use the code can switch handwriting panel view.
 *
 * @const {string}
 * @private
 */
Controller.HANDWRITING_VIEW_CODE_ = 'hwt';


/**
 * The emoji view code, use the code can switch to emoji.
 *
 * @const {string}
 * @private
 */
Controller.EMOJI_VIEW_CODE_ = 'emoji';


/**
 * The repeated times of the backspace.
 *
 * @private {number}
 */
Controller.prototype.backspaceRepeated_ = 0;


/**
 * The handwriting input tool code suffix.
 *
 * @const {string}
 * @private
 */
Controller.HANDWRITING_CODE_SUFFIX_ = '-t-i0-handwrit';


/**
 * The US English compact layout qwerty code.
 *
 * @const {string}
 * @private
 */
Controller.US_COMPACT_QWERTY_CODE_ = 'us.compact.qwerty';


/**
 * Time threshold between samples sent to the back end.
 *
 * @const {number}
 */
Controller.SUBSAMPLING_TIME_THRESHOLD = 100;


/**
 * True if the settings is loaded.
 *
 * @type {boolean}
 */
Controller.prototype.isSettingReady = false;


/**
 * True if the keyboard is set up.
 * Note: This flag is only used for automation testing.
 *
 * @type {boolean}
 */
Controller.prototype.isKeyboardReady = false;


/**
 * True if the current keyset is the US english compact layout.
 *
 * @type {boolean}
 * @private
 */
Controller.prototype.isKeysetUSCompact_ = false;


/**
 * The auto repeat timer for backspace hold.
 *
 * @type {goog.async.Delay}
 * @private
 */
Controller.prototype.backspaceAutoRepeat_;


/**
 * The initial keyset determined by inputview url and/or settings.
 *
 * @type {string}
 * @private
 */
Controller.prototype.initialKeyset_ = '';


/**
 * The current raw keyset code.
 *
 * @type {string}
 * @private
 */
Controller.prototype.currentKeyset_ = '';


/**
 * The current input method id.
 *
 * @private {string}
 */
Controller.prototype.currentInputmethod_ = '';


/**
 * The operations on candidates.
 *
 * @enum {number}
 */
Controller.CandidatesOperation = {
  NONE: 0,
  EXPAND: 1,
  SHRINK: 2
};


/**
 * A temporary list to track keysets have customized in material design.
 *
 * @private {!Array.<string>}
 */
Controller.MATERIAL_KEYSETS_ = [
  'emoji',
  'hwt'
];


/**
 * The active language code.
 *
 * @type {string}
 * @private
 */
Controller.prototype.lang_;


/**
 * The password keyset.
 *
 * @private {string}
 */
Controller.prototype.passwordKeyset_ = '';


/**
 * The soft key map, because key configuration is loaded before layout,
 * controller needs this varaible to save it and hook into keyboard view.
 *
 * @type {!Array.<!i18n.input.chrome.inputview.elements.content.SoftKey>}
 * @private
 */
Controller.prototype.softKeyList_;


/**
 * The mapping from soft key id to soft key view id.
 *
 * @type {!Object.<string, string>}
 * @private
 */
Controller.prototype.mapping_;


/**
 * The input tool name.
 *
 * @type {string}
 * @private
 */
Controller.prototype.title_;


/**
 * Whether to return to the standard character keyset after space is touched.
 *
 * @type {boolean}
 * @private
 */
Controller.prototype.returnToLetterKeysetOnSpace_ = false;


/**
 * A cache of the previous gesture results.
 *
 * @private {!Array.<string>}
 */
Controller.prototype.gestureResultsCache_;


/**
 * Registers event handlers.
 * @private
 */
Controller.prototype.registerEventHandler_ = function() {
  this.handler_.
      listen(this.model_,
          EventType.LAYOUT_LOADED,
          this.onLayoutLoaded_).
      listen(this.model_,
          EventType.CONFIG_LOADED,
          this.onConfigLoaded_).
      listen(this.m17nModel_,
          EventType.CONFIG_LOADED,
          this.onConfigLoaded_).
      listen(this.pointerHandler_, [
            EventType.LONG_PRESS,
            EventType.CLICK,
            EventType.DOUBLE_CLICK,
            EventType.DOUBLE_CLICK_END,
            EventType.POINTER_UP,
            EventType.POINTER_DOWN,
            EventType.POINTER_OVER,
            EventType.POINTER_OUT,
            EventType.SWIPE
          ], this.onPointerEvent_).
      listen(this.pointerHandler_,
          EventType.DRAG,
          this.onDragEvent_).
      listen(window, goog.events.EventType.RESIZE, this.resize).
      listen(this.adapter_,
          EventType.SURROUNDING_TEXT_CHANGED, this.onSurroundingTextChanged_).
      listen(this.adapter_,
          i18n.input.chrome.DataSource.EventType.CANDIDATES_BACK,
          this.onCandidatesBack_).
      listen(this.adapter_,
          i18n.input.chrome.DataSource.EventType.GESTURES_BACK,
          this.onGesturesBack_).
      listen(this.adapter_, EventType.URL_CHANGED, this.onURLChanged_).
      listen(this.adapter_, EventType.CONTEXT_FOCUS, this.onContextFocus_).
      listen(this.adapter_, EventType.CONTEXT_BLUR, this.onContextBlur_).
      listen(this.adapter_, EventType.VISIBILITY_CHANGE,
          this.onVisibilityChange_).
      listen(this.adapter_, EventType.SETTINGS_READY, this.onSettingsReady_).
      listen(this.adapter_, EventType.UPDATE_SETTINGS, this.onUpdateSettings_).
      listen(this.adapter_, EventType.FRONT_TOGGLE_LANGUAGE_STATE,
          this.onUpdateToggleLanguageState_).
      listen(this.adapter_, EventType.VOICE_STATE_CHANGE,
          this.onVoiceStateChange_).
      listen(this.adapter_, EventType.REFRESH, this.onRefresh_);
};


/**
 * Handler for voice module state change.
 *
 * @param {!events.MessageEvent} e .
 * @private
 */
Controller.prototype.onVoiceStateChange_ = function(e) {
  if (!e.msg[Name.VOICE_STATE]) {
    this.container_.candidateView.switchToIcon(
        CandidateView.IconType.VOICE, true);
    this.container_.voiceView.stop();
  }
};


/**
 * Handles the refresh event from adapter.
 *
 * @private
 */
Controller.prototype.onRefresh_ = function() {
  window.location.reload();
};


/**
 * Sets the default keyset for context types.
 *
 * @param {string} newKeyset .
 * @private
 */
Controller.prototype.setDefaultKeyset_ = function(newKeyset) {
  var keysetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
  for (var context in keysetMap) {
    if (context != ContextType.DEFAULT &&
        keysetMap[context] == keysetMap[ContextType.DEFAULT]) {
      keysetMap[context] = newKeyset;
    }
  }
  keysetMap[ContextType.DEFAULT] = this.initialKeyset_ = newKeyset;
};


/**
 * Callback for updating settings.
 *
 * @param {!events.MessageEvent} e .
 * @private
 */
Controller.prototype.onUpdateSettings_ = function(e) {
  var settings = this.model_.settings;
  if (goog.isDef(e.msg['autoSpace'])) {
    settings.autoSpace = e.msg['autoSpace'];
  }
  if (goog.isDef(e.msg['autoCapital'])) {
    settings.autoCapital = e.msg['autoCapital'];
  }
  if (goog.isDef(e.msg['candidatesNavigation'])) {
    settings.candidatesNavigation = e.msg['candidatesNavigation'];
    this.container_.candidateView.setNavigation(settings.candidatesNavigation);
  }
  if (goog.isDef(e.msg[Name.KEYSET])) {
    this.setDefaultKeyset_(e.msg[Name.KEYSET]);
  }
  if (goog.isDef(e.msg['enableLongPress'])) {
    settings.enableLongPress = e.msg['enableLongPress'];
  }
  if (goog.isDef(e.msg['doubleSpacePeriod'])) {
    settings.doubleSpacePeriod = e.msg['doubleSpacePeriod'];
  }
  if (goog.isDef(e.msg['soundOnKeypress'])) {
    settings.soundOnKeypress = e.msg['soundOnKeypress'];
    this.soundController_.setEnabled(settings.soundOnKeypress);
  }
  if (goog.isDef(e.msg['gestureEditing'])) {
    settings.gestureEditing = e.msg['gestureEditing'];
    var enabled = settings.gestureEditing && !this.adapter_.isA11yMode &&
        !this.adapter_.isHotrod &&
        !this.adapter_.isFloatingVirtualKeyboardEnabled();
    this.container_.swipeView.enabled = enabled;
    this.container_.selectView.setSettingsEnabled(enabled);
  }
  if (goog.isDef(e.msg['gestureTyping'])) {
    settings.gestureTyping = e.msg['gestureTyping'];
  } else {
    settings.gestureTyping = false;
  }
  this.perfTracker_.tick(PerfTracker.TickName.BACKGROUND_SETTINGS_FETCHED);
  this.model_.stateManager.contextType = this.adapter_.contextType;
  this.maybeCreateViews_();
};


/**
 * Callback for url changed.
 *
 * @private
 */
Controller.prototype.onURLChanged_ = function() {
  this.container_.candidateView.setToolbarVisible(this.shouldShowToolBar_());
};


/**
 * Callback for setting ready.
 *
 * @private
 */
Controller.prototype.onSettingsReady_ = function() {
  if (this.isSettingReady) {
    return;
  }

  this.isSettingReady = true;
  // Don't render container twice.
  if (!this.container_.isInDocument()) {
    this.container_.render();
  }
  var keysetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
  var newKeyset = '';
  if (this.adapter_.isA11yMode) {
    newKeyset = util.getConfigName(keysetMap[ContextType.DEFAULT]);
  } else if (this.adapter_.isHotrod) {
    newKeyset = HOTROD_DEFAULT_KEYSET;
  } else {
    newKeyset = /** @type {string} */ (this.model_.settings.
        getPreference(util.getConfigName(keysetMap[ContextType.DEFAULT])));
  }

  if (newKeyset) {
    this.setDefaultKeyset_(newKeyset);
  }
  this.container_.selectView.setSettingsEnabled(
      this.model_.settings.gestureEditing && !this.adapter_.isA11yMode &&
          !this.adapter_.isHotrod);
  // Loads resources in case the default keyset is changed.
  this.loadAllResources_();
  this.maybeCreateViews_();
};


/**
 * Returns whether or not the gesture typing feature is enabled.
 *
 * @return {boolean}
 * @private
 */
Controller.prototype.gestureTypingEnabled_ = function() {
  return this.isKeysetUSCompact_ && this.model_.settings.gestureTyping &&
      !this.adapter_.isA11yMode && !this.adapter_.isHotrod &&
      !this.adapter_.isChromeVoxOn &&
      !this.adapter_.isPasswordBox() &&
      !this.adapter_.isFloatingVirtualKeyboardEnabled() &&
      !this.container_.altDataView.isVisible() &&
      !this.container_.menuView.isVisible() &&
      !this.container_.voiceView.isVisible();
};


/**
 * Returns the time threshold for subsampling, in ms.
 *
 * @return {number}
 * @private
 */
Controller.prototype.subsamplingThreshold_ = function() {
  return Controller.SUBSAMPLING_TIME_THRESHOLD;
};


/**
 * Gets the data for spatial module.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.SoftKey} key .
 * @param {number} x The x-offset of the touch point.
 * @param {number} y The y-offset of the touch point.
 * @return {!Object} .
 * @private
 */
Controller.prototype.getSpatialData_ = function(key, x, y) {
  var items = [];
  items.push([this.getKeyContent_(key), key.estimator.estimateInLogSpace(x, y)
      ]);
  for (var i = 0; i < key.nearbyKeys.length; i++) {
    var nearByKey = key.nearbyKeys[i];
    var content = this.getKeyContent_(nearByKey);
    if (content && util.REGEX_LANGUAGE_MODEL_CHARACTERS.test(content)) {
      items.push([content, nearByKey.estimator.estimateInLogSpace(x, y)]);
    }
  }
  goog.array.sort(items, function(item1, item2) {
    return item1[1] - item2[1];
  });
  var sources = items.map(function(item) {
    return item[0].toLowerCase();
  });
  var possibilities = items.map(function(item) {
    return item[1];
  });
  return {
    'sources': sources,
    'possibilities': possibilities
  };
};


/**
 * Gets the key content.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.SoftKey} key .
 * @return {string} .
 * @private
 */
Controller.prototype.getKeyContent_ = function(key) {
  if (key.type == i18n.input.chrome.ElementType.
      CHARACTER_KEY) {
    key = /** @type {!i18n.input.chrome.inputview.elements.content.
        CharacterKey} */ (key);
    return key.getActiveCharacter();
  }
  if (key.type == i18n.input.chrome.ElementType.
      COMPACT_KEY) {
    key = /** @type {!i18n.input.chrome.inputview.elements.content.
        FunctionalKey} */ (key);
    return key.text;
  }
  return '';
};


/**
 * Callback for pointer event.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
Controller.prototype.onPointerEvent_ = function(e) {
  if (e.type == EventType.LONG_PRESS) {
    if (this.adapter_.isChromeVoxOn || !this.model_.settings.enableLongPress) {
      return;
    }
    var keyset = this.keysetDataMap_[this.currentKeyset_];
    var layout = keyset && keyset[SpecNodeName.LAYOUT];
    var data = layout && this.layoutDataMap_[layout];
    if (data && data[SpecNodeName.DISABLE_LONGPRESS]) {
      return;
    }
  }

  // POINTER_UP event may be dispatched without a view. This is the case when
  // user selected an accent character which is displayed outside of the
  // keyboard window bounds. For other cases, we expect a view associated with a
  // pointer up event.
  if (e.type == EventType.POINTER_UP && !e.view) {
    if (this.container_.altDataView.isVisible() &&
        e.identifier == this.container_.altDataView.identifier) {
      var altDataView = this.container_.altDataView;
      var ch = altDataView.getHighlightedCharacter();
      if (ch) {
        this.adapter_.sendKeyDownAndUpEvent(ch, altDataView.triggeredBy.id,
            altDataView.triggeredBy.keyCode,
            {'sources': [ch.toLowerCase()], 'possibilities': [1]});
      }
      altDataView.hide();
      this.clearUnstickyState_();
    }
    return;
  }

  if (e.type == EventType.POINTER_UP) {
    this.stopBackspaceAutoRepeat_();
  }

  if (e.view) {
    this.handlePointerAction_(e.view, e);
  }
};


/**
 * Sends the last stroke from the gesture canvas view to the gesture decoder, if
 * the last point was added past a time threshold.
 *
 * @param {boolean=} opt_force Whether or not to force send the gesture event.
 * @private
 */
Controller.prototype.maybeSendLastStroke_ = function(opt_force) {
  // Subsample by returning early if the previous point was added too recently.
  var currentTime = Date.now();
  if (!opt_force && currentTime - this.lastPointTime_ <
      this.subsamplingThreshold_()) {
    return;
  } else {
    this.lastPointTime_ = currentTime;
  }
  var lastStroke = this.container_.gestureCanvasView.getLastStroke();
  if (lastStroke) {
    // This call will set up the necessary callbacks the decoder will use to
    // communicate back to this class.
    this.adapter_.sendGestureEvent(lastStroke.getPoints());
  }
};


/**
 * Handles the drag events. Generally, this will forward the event details to
 * the components that handle drawing, decoding, etc.
 *
 * @param {!i18n.input.chrome.inputview.events.DragEvent} e .
 * @private
 */
Controller.prototype.onDragEvent_ = function(e) {
  if (this.gestureTypingEnabled_() && e.type == EventType.DRAG &&
      !this.container_.swipeView.isVisible()) {
    // Conveniently, the DragEvent has coordinates relative to the gesture
    // canvas view, so we can test it's y coordinate in order to determine if
    // we're off the canvas.
    if (e.y + MAXIMUM_DISTANCE_FROM_TOP_FOR_GESTURES < 0) {
      this.container_.gestureCanvasView.clear();
      this.container_.gesturePreviewWindow.hide();
      this.clearCandidates_();
      return;
    }
    this.container_.gestureCanvasView.addPoint(e);
    if (e.view && this.container_.gestureCanvasView.isGesturing) {
      // Ensure the last touched key is not highlighted.
      if (e.view.type != ElementType.MODIFIER_KEY) {
        e.view.setHighlighted(false);
      }
      this.maybeSendLastStroke_();
      // Reposition the gesture preview window to follow the user's touch point.
      if (this.container_.gesturePreviewWindow &&
          this.container_.gestureCanvasView.isActiveIdentifier(
              e.identifier)) {
        this.container_.gesturePreviewWindow.reposition(
            new goog.math.Coordinate(e.x, e.y));
      }
    }
  }
};


/**
 * Handles the swipe action.
 *
 * @param {!i18n.input.chrome.inputview.elements.Element} view The view, for
 *     swipe event, the view would be the soft key which starts the swipe.
 * @param {!i18n.input.chrome.inputview.events.SwipeEvent} e The swipe event.
 * @private
 */
Controller.prototype.handleSwipeAction_ = function(view, e) {
  var direction = e.direction;
  if (this.container_.altDataView.isVisible()) {
    this.container_.altDataView.highlightItem(e.x, e.y, e.identifier);
    return;
  }
  if (view.type == ElementType.BACKSPACE_KEY) {
    if (this.container_.swipeView.isVisible() ||
        this.container_.swipeView.isArmed()) {
      this.stopBackspaceAutoRepeat_();
      return;
    }
  }

  if (view.type == ElementType.CHARACTER_KEY) {
    view = /** @type {!i18n.input.chrome.inputview.elements.content.
        CharacterKey} */ (view);
    if (direction & i18n.input.chrome.inputview.SwipeDirection.UP ||
        direction & i18n.input.chrome.inputview.SwipeDirection.DOWN) {
      var ch = view.getCharacterByGesture(!!(direction &
          i18n.input.chrome.inputview.SwipeDirection.UP));
      if (ch) {
        view.flickerredCharacter = ch;
      }
    }
  }

  if (view.type == ElementType.COMPACT_KEY) {
    view = /** @type {!i18n.input.chrome.inputview.elements.content.
        CompactKey} */ (view);
    if ((direction & i18n.input.chrome.inputview.SwipeDirection.UP) &&
        view.hintText) {
      view.flickerredCharacter = view.hintText;
    }
  }
};


/**
 * Execute a command.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.MenuView.Command}
 *     command The command that about to be executed.
 * @param {string=} opt_arg The optional command argument.
 * @private
 */
Controller.prototype.executeCommand_ = function(command, opt_arg) {
  var CommandEnum = MenuView.Command;
  switch (command) {
    case CommandEnum.SWITCH_IME:
      var inputMethodId = opt_arg;
      if (inputMethodId) {
        this.adapter_.switchToInputMethod(inputMethodId);
      }
      break;

    case CommandEnum.SWITCH_KEYSET:
      var keyset = opt_arg;
      if (keyset) {
        this.switchToKeyset(keyset);
      }
      break;
    case CommandEnum.OPEN_EMOJI:
      this.contextTypeToLastKeysetMap_[this.adapter_.contextType] =
          this.currentKeyset_;
      this.switchToKeyset(Controller.EMOJI_VIEW_CODE_);
      break;

    case CommandEnum.OPEN_HANDWRITING:
      this.contextTypeToLastKeysetMap_[this.adapter_.contextType] =
          this.currentKeyset_;
      // TODO: remember handwriting keyset.
      this.switchToKeyset(Controller.HANDWRITING_VIEW_CODE_);
      break;

    case CommandEnum.OPEN_SETTING:
      if (window.inputview) {
        inputview.openSettings();
      }
      break;
  }
};


/**
 * Handles the pointer action.
 *
 * @param {!i18n.input.chrome.inputview.elements.Element} view The view.
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
Controller.prototype.handlePointerAction_ = function(view, e) {
  if (this.gestureTypingEnabled_() && !this.container_.swipeView.isVisible()) {
    if (e.type == EventType.POINTER_DOWN) {
      // Do some clean up before starting a new stroke.
      this.container_.gestureCanvasView.removeEmptyStrokes();
      this.container_.gestureCanvasView.startStroke(e);
      if (view.type != ElementType.MODIFIER_KEY) {
        view.setHighlighted(false);
      }
    }

    // Determine if the gestureCanvasView was handling a gesture before calling
    // endStroke, as it ends the current gesture.
    var wasGesturing = this.container_.gestureCanvasView.isGesturing;
    if (e.type == EventType.POINTER_UP && wasGesturing) {
      this.container_.gestureCanvasView.endStroke(e);
      this.maybeSendLastStroke_(true);
    }

    // Do not trigger other activities when gesturing.
    if (wasGesturing) {
      if (view.type != ElementType.MODIFIER_KEY &&
          (e.type == EventType.POINTER_OVER ||
              e.type == EventType.POINTER_OUT)) {
        view.setHighlighted(false);
      }
      return;
    }
  }

  if (e.type == EventType.SWIPE) {
    e =  /** @type {!i18n.input.chrome.inputview.events.SwipeEvent} */ (e);
    this.handleSwipeAction_(view, e);
  }
  switch (view.type) {
    case ElementType.HOTROD_SWITCHER_KEY:
    case ElementType.BACK_BUTTON:
    case ElementType.BACK_TO_KEYBOARD:
      if (view.type == ElementType.HOTROD_SWITCHER_KEY &&
          !this.adapter_.isHotrod) {
        return;
      }
      if (e.type == EventType.POINTER_OUT || e.type == EventType.POINTER_UP) {
        view.setHighlighted(false);
      } else if (e.type == EventType.POINTER_DOWN ||
          e.type == EventType.POINTER_OVER) {
        view.setHighlighted(true);
      }
      if (e.type == EventType.POINTER_UP) {
        var backToKeyset = this.container_.currentKeysetView.fromKeyset;
        if (view.type == ElementType.HOTROD_SWITCHER_KEY &&
            this.adapter_.isHotrod) {
          backToKeyset = Controller.US_COMPACT_QWERTY_CODE_;
        }
        this.switchToKeyset(backToKeyset);
        this.clearCandidates_();
        this.soundController_.onKeyUp(view.type);
      }
      return;
    case ElementType.EXPAND_CANDIDATES:
      if (e.type == EventType.POINTER_UP) {
        this.showCandidates_(this.candidatesInfo_.source,
            this.candidatesInfo_.candidates,
            Controller.CandidatesOperation.EXPAND);
        this.soundController_.onKeyUp(view.type);
      }
      return;
    case ElementType.SHRINK_CANDIDATES:
      if (e.type == EventType.POINTER_UP) {
        this.showCandidates_(this.candidatesInfo_.source,
            this.candidatesInfo_.candidates,
            Controller.CandidatesOperation.SHRINK);
        this.soundController_.onKeyUp(view.type);
      }
      return;
    case ElementType.CANDIDATE:
      view = /** @type {!i18n.input.chrome.inputview.elements.content.
          Candidate} */ (view);
      if (view.candidateType == CandidateType.TOOLTIP)
        return;
      if (e.type == EventType.POINTER_UP) {
        if (view.candidateType == CandidateType.CANDIDATE) {
          this.adapter_.selectCandidate(view.candidate);
        } else if (view.candidateType == CandidateType.NUMBER) {
          this.adapter_.sendKeyDownAndUpEvent(
              view.candidate[Name.CANDIDATE], '');
        }
        this.container_.cleanStroke();
        this.soundController_.onKeyUp(ElementType.CANDIDATE);
      }
      if (e.type == EventType.POINTER_OUT || e.type == EventType.POINTER_UP) {
        view.setHighlighted(false);
      } else if (e.type == EventType.POINTER_DOWN ||
          e.type == EventType.POINTER_OVER) {
        view.setHighlighted(true);
      }
      return;

    case ElementType.ALTDATA_VIEW:
      view = /** @type {!i18n.input.chrome.inputview.elements.content.
          AltDataView} */ (view);
      if (e.type == EventType.POINTER_UP && e.identifier == view.identifier) {
        var ch = view.getHighlightedCharacter();
        if (ch) {
          this.adapter_.sendKeyDownAndUpEvent(ch, view.triggeredBy.id,
              view.triggeredBy.keyCode,
              {'sources': [ch.toLowerCase()], 'possibilities': [1]});
        }
        view.hide();
        this.clearUnstickyState_();
        this.soundController_.onKeyUp(view.type);
      }
      return;

    case ElementType.MENU_ITEM:
      if (this.adapter_.isHotrod) {
        // Disable menu items in hotrod. Fix this if hotrod needs i18n support.
        return;
      }
      view = /** @type {!i18n.input.chrome.inputview.elements.content.
          MenuItem} */ (view);
      if (e.type == EventType.POINTER_UP) {
        this.executeCommand_.apply(this, view.getCommand());
        this.container_.menuView.hide();
        this.soundController_.onKeyUp(view.type);
        this.resetAll();
      }
      view.setHighlighted(e.type == EventType.POINTER_DOWN ||
          e.type == EventType.POINTER_OVER);
      // TODO: Add chrome vox support.
      return;

    case ElementType.MENU_VIEW:
      view = /** @type {!i18n.input.chrome.inputview.elements.content.
          MenuView} */ (view);

      if (e.type == EventType.CLICK &&
          e.target == view.getCoverElement()) {
        view.hide();
      }
      return;

    case ElementType.EMOJI_KEY:
      if (e.type == EventType.CLICK) {
        if (!this.container_.currentKeysetView.isDragging && view.text != '') {
          this.adapter_.commitText(view.text);
          this.soundController_.onKeyUp(view.type);
        }
      }
      return;

    case ElementType.HWT_PRIVACY_GOT_IT:
      // Broadcasts the handwriting privacy confirmed message to let canvas
      // view handle it.
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.dispatchEvent(new goog.events.Event(
            Type.HWT_PRIVACY_GOT_IT));
      }
      return;

    case ElementType.VOICE_PRIVACY_GOT_IT:
      // Broadcasts the voice privacy confirmed message to let voice
      // view handle it.
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.dispatchEvent(new goog.events.Event(
            Type.VOICE_PRIVACY_GOT_IT));
      }
      return;

    case ElementType.VOICE_VIEW:
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendVoiceViewStateChange(false);
        this.container_.candidateView.switchToIcon(
            CandidateView.IconType.VOICE, true);
        this.container_.voiceView.stop();
      }
      return;
    case ElementType.SWIPE_VIEW:
      this.stopBackspaceAutoRepeat_();
      if (e.type == EventType.POINTER_UP ||
          e.type == EventType.POINTER_OUT) {
        this.clearUnstickyState_();
      }
      return;
    case ElementType.DRAG:
      if (e.type == EventType.POINTER_DOWN && this.container_.floatingView) {
        this.container_.floatingView.show();
      }
      return;
    case ElementType.FLOATING_VIEW:
      if (e.type == EventType.POINTER_UP && this.container_.floatingView) {
        this.container_.floatingView.hide();
      }
      return;
    case ElementType.CUT:
    case ElementType.COPY:
    case ElementType.PASTE:
    case ElementType.BOLD:
    case ElementType.ITALICS:
    case ElementType.UNDERLINE:
    case ElementType.REDO:
    case ElementType.UNDO:
    case ElementType.SELECT_ALL:
      view.setHighlighted(e.type == EventType.POINTER_DOWN ||
          e.type == EventType.POINTER_OVER);
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyDownAndUpEvent(
            '', this.elementTypeToKeyCode_[view.type], undefined, undefined, {
              ctrl: true,
              alt: false,
              shift: false
            });
      }
      return;
    case ElementType.SOFT_KEY_VIEW:
      // Delegates the events on the soft key view to its soft key.
      view = /** @type {!i18n.input.chrome.inputview.elements.layout.
          SoftKeyView} */ (view);
      if (!view.softKey) {
        return;
      }
      view = view.softKey;
  }

  if (view.type != ElementType.MODIFIER_KEY &&
      !this.container_.altDataView.isVisible() &&
      !this.container_.menuView.isVisible() &&
      !this.container_.swipeView.isVisible()) {
    // The highlight of the modifier key is depending on the state instead
    // of the key down or up.
    if (e.type == EventType.POINTER_OVER || e.type == EventType.POINTER_DOWN ||
        e.type == EventType.DOUBLE_CLICK) {
      view.setHighlighted(true);
    } else if (e.type == EventType.POINTER_OUT ||
        e.type == EventType.POINTER_UP ||
        e.type == EventType.DOUBLE_CLICK_END) {
      view.setHighlighted(false);
    }
  }
  view = /** @type {!i18n.input.chrome.inputview.elements.content.
      SoftKey} */ (view);
  this.handlePointerEventForSoftKey_(view, e);
  this.updateContextModifierState_();
};


/**
 * Handles softkey of the pointer action.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.SoftKey} softKey .
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
Controller.prototype.handlePointerEventForSoftKey_ = function(softKey, e) {
  var key;
  switch (softKey.type) {
    case ElementType.VOICE_BTN:
      if (e.type == EventType.POINTER_UP) {
        this.container_.candidateView.switchToIcon(
            CandidateView.IconType.VOICE, false);
        this.container_.voiceView.start();
      }
      break;
    case ElementType.CANDIDATES_PAGE_UP:
      if (e.type == EventType.POINTER_UP) {
        this.container_.expandedCandidateView.pageUp();
      }
      break;
    case ElementType.CANDIDATES_PAGE_DOWN:
      if (e.type == EventType.POINTER_UP) {
        this.container_.expandedCandidateView.pageDown();
      }
      break;
    case ElementType.CHARACTER_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          CharacterKey} */ (softKey);
      if (e.type == EventType.LONG_PRESS) {
        this.container_.altDataView.show(
            key, goog.i18n.bidi.isRtlLanguage(this.languageCode_),
            e.identifier);
      } else if (e.type == EventType.POINTER_UP) {
        this.model_.stateManager.triggerChording();
        var ch = key.getActiveCharacter();
        if (ch) {
          this.adapter_.sendKeyDownAndUpEvent(ch, key.id, key.keyCode,
              this.getSpatialData_(key, e.x, e.y));
        }
        this.clearUnstickyState_();
        key.flickerredCharacter = '';
      }
      break;

    case ElementType.MODIFIER_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          ModifierKey} */(softKey);
      var isStateEnabled = this.model_.stateManager.hasState(key.toState);
      var isChording = this.model_.stateManager.isChording(key.toState);
      if (e.type == EventType.POINTER_DOWN) {
        this.changeState_(key.toState, !isStateEnabled, true, false);
        this.model_.stateManager.setKeyDown(key.toState, true);
      } else if (e.type == EventType.POINTER_UP || e.type == EventType.
          POINTER_OUT) {
        if (isChording) {
          this.changeState_(key.toState, false, false);
        } else if (key.toState == StateType.CAPSLOCK) {
          this.changeState_(key.toState, isStateEnabled, true, true);
          // Update the CAPSLOCK state of the system by sending a dummy key.
          this.adapter_.sendKeyDownEvent(
            KeyCodes.SHIFT, KeyCodes.SHIFT_LEFT, goog.events.KeyCodes.SHIFT);
        } else if (this.model_.stateManager.isKeyDown(key.toState)) {
          this.changeState_(key.toState, isStateEnabled, false);
        }
        this.model_.stateManager.setKeyDown(key.toState, false);
      } else if (e.type == EventType.DOUBLE_CLICK) {
        this.changeState_(key.toState, isStateEnabled, true, true);
      } else if (e.type == EventType.LONG_PRESS) {
        if (!isChording) {
          this.changeState_(key.toState, true, true, true);
          this.model_.stateManager.setKeyDown(key.toState, false);
        }
      }
      break;

    case ElementType.BACKSPACE_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          FunctionalKey} */(softKey);
      if (e.type == EventType.POINTER_DOWN) {
        this.backspaceTick_();
      } else if (e.type == EventType.POINTER_UP || e.type == EventType.
          POINTER_OUT) {
        if (!this.container_.swipeView.isVisible()) {
          this.stopBackspaceAutoRepeat_();
        }
      }
      this.returnToLetterKeysetOnSpace_ = false;
      break;

    case ElementType.TAB_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          FunctionalKey} */ (softKey);
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('\u0009', KeyCodes.TAB);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('\u0009', KeyCodes.TAB);
      }
      this.returnToLetterKeysetOnSpace_ = false;
      break;

    case ElementType.ENTER_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          FunctionalKey} */ (softKey);
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyDownAndUpEvent('\u000D', KeyCodes.ENTER);
      }
      break;

    case ElementType.ARROW_UP:
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('', KeyCodes.ARROW_UP);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('', KeyCodes.ARROW_UP);
      }
      break;

    case ElementType.ARROW_DOWN:
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('', KeyCodes.ARROW_DOWN);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('', KeyCodes.ARROW_DOWN);
      }
      break;

    case ElementType.ARROW_LEFT:
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('', KeyCodes.ARROW_LEFT);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('', KeyCodes.ARROW_LEFT);
      }
      break;

    case ElementType.ARROW_RIGHT:
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('', KeyCodes.ARROW_RIGHT);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('', KeyCodes.ARROW_RIGHT);
      }
      break;
    case ElementType.EN_SWITCHER:
      if (e.type == EventType.POINTER_UP) {
        key = /** @type {!i18n.input.chrome.inputview.elements.content.
            EnSwitcherKey} */ (softKey);
        this.adapter_.toggleLanguageState(this.model_.stateManager.isEnMode);
        this.model_.stateManager.isEnMode = !this.model_.stateManager.isEnMode;
        if (!this.updateToggleLanguageKeyset_()) {
          key.update();
        }
      }
      break;
    case ElementType.SPACE_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          SpaceKey} */ (softKey);
      var doubleSpacePeriod = this.model_.settings.doubleSpacePeriod &&
          this.currentKeyset_ != Controller.HANDWRITING_VIEW_CODE_ &&
          this.currentKeyset_ != Controller.EMOJI_VIEW_CODE_;
      if (e.type == EventType.POINTER_UP || (!doubleSpacePeriod && e.type ==
          EventType.DOUBLE_CLICK_END)) {
        this.adapter_.sendKeyDownAndUpEvent(key.getCharacter(),
            KeyCodes.SPACE);
        this.clearUnstickyState_();
      } else if (e.type == EventType.DOUBLE_CLICK && doubleSpacePeriod) {
        this.adapter_.doubleClickOnSpaceKey();
      }
      if (this.returnToLetterKeysetOnSpace_) {
        // Return to the letter keyset.
        this.switchToKeyset(key.toKeyset);
        this.returnToLetterKeysetOnSpace_ = false;
      }
      var isHwt = Controller.HANDWRITING_VIEW_CODE_ == this.currentKeyset_;
      if (isHwt) {
        this.container_.cleanStroke();
      }
      break;

    case ElementType.SWITCHER_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          SwitcherKey} */ (softKey);
      if (e.type == EventType.POINTER_UP) {
        if (this.isSubKeyset_(key.toKeyset, this.currentKeyset_)) {
          this.model_.stateManager.reset();
          this.container_.update();
          this.updateContextModifierState_();
          this.container_.menuView.hide();
        } else {
          this.resetAll();
        }
        // Switch to the specific keyboard.
        this.switchToKeyset(key.toKeyset);
        if (key.record) {
          this.model_.settings.savePreference(
              util.getConfigName(key.toKeyset),
              key.toKeyset);
        }
      }
      this.returnToLetterKeysetOnSpace_ = false;
      break;

    case ElementType.COMPACT_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          CompactKey} */(softKey);
      if (e.type == EventType.LONG_PRESS) {
        this.container_.altDataView.show(
            key, goog.i18n.bidi.isRtlLanguage(this.languageCode_),
            e.identifier);
      } else if (e.type == EventType.POINTER_UP) {
        this.model_.stateManager.triggerChording();
        var ch = key.getActiveCharacter();
        if (ch.length == 1) {
          if (this.currentKeyset_.indexOf('symbol') != -1) {
            this.adapter_.sendKeyDownAndUpEvent(key.getActiveCharacter(),
                KeyCodes.SYMBOL, 0, this.getSpatialData_(key, e.x, e.y));
          }
          else {
            this.adapter_.sendKeyDownAndUpEvent(key.getActiveCharacter(), '', 0,
              this.getSpatialData_(key, e.x, e.y));
          }
        } else if (ch.length > 1) {
          // Some compact keys contains more than 1 characters, such as '.com',
          // 'http://', etc. Those keys should trigger a direct commit text
          // instead of key events.
          this.adapter_.commitText(ch);
        }
        this.clearUnstickyState_();
        key.flickerredCharacter = '';
        if (this.currentKeyset_.indexOf('symbol') != -1) {
          // If this is the symbol keyset, a space as the next input should
          // switch us to the standard keyset.
          this.returnToLetterKeysetOnSpace_ = true;
        }
      }
      break;

    case ElementType.HIDE_KEYBOARD_KEY:
      var defaultKeyset = this.getActiveKeyset_();
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.hideKeyboard();
        if (this.currentKeyset_ != defaultKeyset) {
          this.switchToKeyset(defaultKeyset);
        }
      }
      break;

    case ElementType.MENU_KEY:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          MenuKey} */ (softKey);
      if (e.type == EventType.POINTER_DOWN) {
        var isCompact = this.currentKeyset_.indexOf('compact') != -1;
        // Gets the default full keyboard instead of default keyset because
        // the default keyset can be a compact keyset which would cause problem
        // in MenuView.show().
        var defaultFullKeyset = this.initialKeyset_.split(/\./)[0];
        var enableCompact = !this.adapter_.isA11yMode && goog.array.contains(
            util.KEYSETS_HAVE_COMPACT, defaultFullKeyset);
        if (this.languageCode_ == 'ko') {
          enableCompact = false;
        }
        var hasHwt = !this.adapter_.isPasswordBox() &&
            !Controller.DISABLE_HWT && goog.object.contains(
            InputToolCode, this.getHwtInputToolCode_());
        var hasEmoji = !this.adapter_.isPasswordBox();
        var enableSettings = this.shouldEnableSettings() &&
            !!window.inputview && !!inputview.openSettings;
        this.adapter_.getInputMethods(function(inputMethods) {
          this.container_.menuView.show(key, defaultFullKeyset, isCompact,
              enableCompact, this.currentInputMethod_, inputMethods, hasHwt,
              enableSettings, hasEmoji, this.adapter_.isA11yMode);
        }.bind(this));
      }
      break;

    case ElementType.GLOBE_KEY:
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.clearModifierStates();
        this.adapter_.setModifierState(
            i18n.input.chrome.inputview.StateType.ALT, true);
        this.adapter_.sendKeyDownAndUpEvent(
            KeyCodes.SHIFT, KeyCodes.SHIFT_LEFT, goog.events.KeyCodes.SHIFT);
        this.adapter_.setModifierState(
            i18n.input.chrome.inputview.StateType.ALT, false);
      }
      break;
    case ElementType.IME_SWITCH:
      key = /** @type {!i18n.input.chrome.inputview.elements.content.
          FunctionalKey} */ (softKey);
      this.adapter_.sendKeyDownAndUpEvent('', key.id);
      break;
  }
  // Play key sound on pointer up or double click.
  if (e.type == EventType.POINTER_UP || e.type == EventType.DOUBLE_CLICK)
    this.soundController_.onKeyUp(softKey.type);
};


/**
 * Clears unsticky state.
 *
 * @private
 */
Controller.prototype.clearUnstickyState_ = function() {
  if (this.model_.stateManager.hasUnStickyState()) {
    for (var key in StateType) {
      var value = StateType[key];
      if (this.model_.stateManager.hasState(value) &&
          !this.model_.stateManager.isSticky(value)) {
        this.changeState_(value, false, false);
      }
    }
  }
  this.container_.update();
};


/**
 * Stops the auto-repeat for backspace.
 *
 * @private
 */
Controller.prototype.stopBackspaceAutoRepeat_ = function() {
  if (this.backspaceAutoRepeat_) {
    goog.dispose(this.backspaceAutoRepeat_);
    this.backspaceAutoRepeat_ = null;
    this.adapter_.sendKeyUpEvent('\u0008', KeyCodes.BACKSPACE);
    this.backspaceRepeated_ = 0;
  }
};


/**
 * The tick for the backspace key.
 *
 * @private
 */
Controller.prototype.backspaceTick_ = function() {
  this.backspaceRepeated_++;
  this.backspaceDown_();
  this.soundController_.onKeyRepeat(ElementType.BACKSPACE_KEY);

  if (this.backspaceAutoRepeat_) {
    var delay = MINIMUM_BACKSPACE_REPEAT_TIME_MS;
    if (this.backspaceRepeated_ <= BACKSPACE_REPEAT_LIMIT) {
      var ease = goog.fx.easing.easeOut(
          this.backspaceRepeated_ / BACKSPACE_REPEAT_LIMIT);
      var delta = MAXIMUM_BACKSPACE_REPEAT_TIME_MS -
          MINIMUM_BACKSPACE_REPEAT_TIME_MS;
      delay = MAXIMUM_BACKSPACE_REPEAT_TIME_MS - (delta * ease);
    }
    this.backspaceAutoRepeat_.start(delay);
  } else {
    this.backspaceAutoRepeat_ = new goog.async.Delay(
        goog.bind(this.backspaceTick_, this),
        BACKSPACE_REPEAT_START_TIME_MS);
    this.backspaceAutoRepeat_.start();
  }
};


/**
 * Callback for VISIBILITY_CHANGE.
 *
 * @private
 */
Controller.prototype.onVisibilityChange_ = function() {
  if (!this.adapter_.isVisible) {
    for (var name in this.statsForClosing_) {
      var stat = this.statsForClosing_[name];
      this.statistics_.recordValue(name, stat[0], stat[1], stat[2]);
    }
    this.statistics_.recordValue('InputMethod.VirtualKeyboard.Duration',
        Math.floor((new Date() - this.showTimeStamp_) / 1000), 4096, 50);
    this.statsForClosing_ = {};
    this.showTimeStamp_ = new Date();
    this.resetAll();
  }
};


/**
 * Resets the whole keyboard include clearing candidates,
 * reset modifier state, etc.
 */
Controller.prototype.resetAll = function() {
  this.clearCandidates_();
  this.container_.cleanStroke();
  this.model_.stateManager.reset();
  this.container_.update();
  this.updateContextModifierState_();
  this.resize();
  this.container_.expandedCandidateView.close();
  this.container_.menuView.hide();
  this.container_.swipeView.reset();
  this.container_.altDataView.hide();
  if (this.container_.gesturePreviewWindow) {
    this.container_.gesturePreviewWindow.hide();
  }
  if (this.container_.floatingView) {
    this.container_.floatingView.hide();
  }
  this.stopBackspaceAutoRepeat_();
};


/**
 * Returns whether the toolbar should be shown.
 *
 * @return {boolean}
 * @private
 */
Controller.prototype.shouldShowToolBar_ = function() {
  return this.adapter_.features.isEnabled(FeatureName.OPTIMIZED_LAYOUTS) &&
      this.adapter_.isGoogleDocument() &&
      this.adapter_.contextType == ContextType.DEFAULT;
};


/**
 * Callback when the context is changed.
 *
 * @private
 */
Controller.prototype.onContextFocus_ = function() {
  this.resetAll();
  this.model_.stateManager.contextType = this.adapter_.contextType;
  this.switchToKeyset(this.getActiveKeyset_());
};


/**
 * Callback when surrounding text is changed.
 *
 * @param {!i18n.input.chrome.inputview.events.SurroundingTextChangedEvent} e .
 * @private
 */
Controller.prototype.onSurroundingTextChanged_ = function(e) {
  if (!this.model_.settings.autoCapital || !e.text) {
    return;
  }
  var textBeforeCursor = e.textBeforeCursor.replace(/\u00a0/g, ' ');
  var isShiftEnabled = this.model_.stateManager.hasState(StateType.SHIFT);
  var needAutoCap = this.model_.settings.autoCapital &&
      util.needAutoCap(textBeforeCursor);
  if (needAutoCap && !isShiftEnabled) {
    this.changeState_(StateType.SHIFT, true, false);
    this.shiftForAutoCapital_ = true;
  } else if (!needAutoCap && this.shiftForAutoCapital_) {
    this.changeState_(StateType.SHIFT, false, false);
  }
};


/**
 * Callback for Context blurs.
 *
 * @private
 */
Controller.prototype.onContextBlur_ = function() {
  this.container_.cleanStroke();
  this.container_.menuView.hide();
  this.stopBackspaceAutoRepeat_();
};


/**
 * Backspace key is down.
 *
 * @private
 */
Controller.prototype.backspaceDown_ = function() {
  if (this.container_.hasStrokesOnCanvas()) {
    this.clearCandidates_();
    this.container_.cleanStroke();
  } else {
    this.adapter_.sendKeyDownEvent('\u0008', KeyCodes.BACKSPACE);
  }
  this.recordStatsForClosing_(
    'InputMethod.VirtualKeyboard.BackspaceCount', 1, 4095, 4096);
};


/**
 * Callback for state change.
 *
 * @param {StateType} stateType The state type.
 * @param {boolean} enable True to enable the state.
 * @param {boolean} isSticky True to make the state sticky.
 * @param {boolean=} opt_isFinalSticky .
 * @private
 */
Controller.prototype.changeState_ = function(stateType, enable, isSticky,
    opt_isFinalSticky) {
  if (stateType == StateType.ALTGR) {
    var code = KeyCodes.ALT_RIGHT;
    if (enable) {
      this.adapter_.sendKeyDownEvent('', code);
    } else {
      this.adapter_.sendKeyUpEvent('', code);
    }
  }
  if (stateType == StateType.SHIFT) {
    this.shiftForAutoCapital_ = false;
  }
  var isEnabledBefore = this.model_.stateManager.hasState(stateType);
  var isStickyBefore = this.model_.stateManager.isSticky(stateType);
  this.model_.stateManager.setState(stateType, enable);
  this.model_.stateManager.setSticky(stateType, isSticky);
  var isFinalSticky = goog.isDef(opt_isFinalSticky) ? opt_isFinalSticky :
      false;
  var isFinalStikcyBefore = this.model_.stateManager.isFinalSticky(stateType);
  this.model_.stateManager.setFinalSticky(stateType, isFinalSticky);
  if (isEnabledBefore != enable || isStickyBefore != isSticky ||
      isFinalStikcyBefore != isFinalSticky) {
    this.container_.update();
  }
};


/**
 * Updates the modifier state for context.
 *
 * @private
 */
Controller.prototype.updateContextModifierState_ = function() {
  var stateManager = this.model_.stateManager;
  this.adapter_.setModifierState(StateType.ALT,
      stateManager.hasState(StateType.ALT));
  this.adapter_.setModifierState(StateType.CTRL,
      stateManager.hasState(StateType.CTRL));
  this.adapter_.setModifierState(StateType.CAPSLOCK,
      stateManager.hasState(StateType.CAPSLOCK));
  if (!this.shiftForAutoCapital_) {
    // If shift key is automatically on because of feature - autoCapital,
    // Don't set modifier state to adapter.
    this.adapter_.setModifierState(StateType.SHIFT,
        stateManager.hasState(StateType.SHIFT));
  }
};


/**
 * Callback for AUTO-COMPLETE event.
 *
 * @param {!i18n.input.chrome.DataSource.CandidatesBackEvent} e .
 * @private
 */
Controller.prototype.onCandidatesBack_ = function(e) {
  this.candidatesInfo_ = new i18n.input.chrome.inputview.CandidatesInfo(
      e.source, e.candidates);
  this.showCandidates_(e.source, e.candidates, Controller.CandidatesOperation.
      NONE);
};


/**
 * Converts a word to shifted or all-caps based on the current shift state.
 *
 * @param {string} word The word to potentially convert.
 * @return {string} The converted word.
 * @private
 */
Controller.prototype.convertToShifted_ = function(word) {
  if (this.model_.stateManager.getState() == StateType.SHIFT) {
    if (this.model_.stateManager.isSticky(StateType.SHIFT) &&
        this.model_.stateManager.isFinalSticky(StateType.SHIFT)) {
      word = word.toUpperCase();
    } else {
      word = word.charAt(0).toUpperCase() + word.slice(1);
    }
  }
  return word;
};


/**
 * Callback for gestures results event.
 *
 * @param {!i18n.input.chrome.DataSource.GesturesBackEvent} e .
 * @private
 */
Controller.prototype.onGesturesBack_ = function(e) {
  var response = e.response;
  this.stopBackspaceAutoRepeat_();
  if (!response.commit &&
      goog.array.equals(response.results, this.gestureResultsCache_)) {
    // If gesture results have not updated, do not transmit to the UI.
    return;
  } else {
    this.gestureResultsCache_ = response.results;
  }
  var bestResult = this.convertToShifted_(response.results[0]);
  if (this.container_.gesturePreviewWindow &&
      this.container_.gestureCanvasView.isGesturing) {
    this.container_.gesturePreviewWindow.show(bestResult);
  }
  if (response.commit) {
    // Commit the best result.
    this.adapter_.commitGestureResult(
        bestResult, this.adapter_.isGoogleDocument());
    this.gestureResultsCache_ = [];
    this.statistics_.recordEnum(
        Statistics.GESTURE_TYPING_METRIC_NAME,
        Statistics.GestureTypingEvent.TYPED,
        Statistics.GestureTypingEvent.MAX);
    if (this.container_.gesturePreviewWindow) {
      new goog.async.Delay(
          goog.bind(this.container_.gesturePreviewWindow.hide,
                    this.container_.gesturePreviewWindow),
          GESTURE_PREVIEW_LINGER_TIME_MS).start();
    }
  }
  this.showGestureCandidates_(response.results.slice(1));
};


/**
 * Shows the gesture results as candidates.
 *
 * @param {!Array<string>} results The gesture results to show.
 * @private
 */
Controller.prototype.showGestureCandidates_ = function(results) {
  // Convert the results to the candidate format.
  var candidates = [];
  for (var i = 0; i < results.length; ++i) {
    var candidate = {};
    var result = this.convertToShifted_(results[i]);
    candidate[Name.CANDIDATE] = result;
    candidate[Name.CANDIDATE_ID] = i;
    candidate[Name.IS_EMOJI] = false;
    candidate[Name.MATCHED_LENGTHS] = 0;
    candidates.push(candidate);
  }
  // The source is empty as this is a gesture and not a series of key presses.
  this.showCandidates_(
      '', candidates, Controller.CandidatesOperation.NONE, true);
};


/**
 * Shows the candidates to the candidate view.
 *
 * @param {string} source The source text.
 * @param {!Array.<!Object>} candidates The candidate text list.
 * @param {Controller.CandidatesOperation} operation .
 * @param {boolean=} opt_fromGestures Whether or not the candidates are being
 *     set by gesture typing.
 * @private
 */
Controller.prototype.showCandidates_ = function(source, candidates,
    operation, opt_fromGestures) {
  var state = !!source ? ExpandedCandidateView.State.COMPLETION_CORRECTION :
      ExpandedCandidateView.State.PREDICTION;
  var expandView = this.container_.expandedCandidateView;
  var expand = false;
  if (operation == Controller.CandidatesOperation.NONE) {
    expand = expandView.state == state;
  } else {
    expand = operation == Controller.CandidatesOperation.EXPAND;
  }

  if (candidates.length == 0) {
    this.clearCandidates_();
    expandView.state = ExpandedCandidateView.State.NONE;
    return;
  }

  // The compact pinyin needs full candidates instead of three candidates.
  var isThreeCandidates = this.currentKeyset_.indexOf('compact') != -1 &&
      this.currentKeyset_.indexOf('pinyin-zh-CN') == -1;
  if (isThreeCandidates) {
    if (candidates.length > 1) {
      // Swap the first candidate and the second candidate.
      var tmp = candidates[0];
      candidates[0] = candidates[1];
      candidates[1] = tmp;
    }
  }
  var isHwt = Controller.HANDWRITING_VIEW_CODE_ == this.currentKeyset_;
  this.container_.candidateView.showCandidates(candidates, isThreeCandidates,
      this.model_.settings.candidatesNavigation && !isHwt);

  // Only sum of candidate is greater than top line count. Need to update
  // expand view.
  if (expand && this.container_.candidateView.candidateCount <
      candidates.length) {
    expandView.state = state;
    this.container_.currentKeysetView.setVisible(false);
    expandView.showCandidates(candidates,
        this.container_.candidateView.candidateCount);
    this.container_.candidateView.switchToIcon(CandidateView.IconType.
        SHRINK_CANDIDATES, true);
  } else {
    expandView.state = ExpandedCandidateView.State.NONE;
    expandView.setVisible(false);
    this.container_.candidateView.switchToIcon(CandidateView.IconType.
        SHRINK_CANDIDATES, false);
    this.container_.currentKeysetView.setVisible(true);
  }
};


/**
 * Clears candidates.
 *
 * @private
 */
Controller.prototype.clearCandidates_ = function() {
  this.candidatesInfo_ = i18n.input.chrome.inputview.CandidatesInfo.getEmpty();
  this.container_.candidateView.clearCandidates();
  this.container_.expandedCandidateView.close();
  this.container_.expandedCandidateView.state = ExpandedCandidateView.State.
      NONE;
  if (this.container_.currentKeysetView) {
    this.container_.currentKeysetView.setVisible(true);
  }

  if (this.currentKeyset_ == Controller.HANDWRITING_VIEW_CODE_ ||
      this.currentKeyset_ == Controller.EMOJI_VIEW_CODE_) {
    this.container_.candidateView.switchToIcon(
        CandidateView.IconType.VOICE, false);
    this.container_.candidateView.switchToIcon(
        CandidateView.IconType.EXPAND_CANDIDATES, false);
  } else {
    this.container_.candidateView.switchToIcon(CandidateView.IconType.VOICE,
        this.adapter_.isVoiceInputEnabled);
  }
};


/**
 * Callback when the layout is loaded.
 *
 * @param {!i18n.input.chrome.inputview.events.LayoutLoadedEvent} e The event.
 * @private
 */
Controller.prototype.onLayoutLoaded_ = function(e) {
  var layoutID = e.data['layoutID'];
  this.layoutDataMap_[layoutID] = e.data;
  this.perfTracker_.tick(PerfTracker.TickName.LAYOUT_LOADED);
  this.maybeCreateViews_();
};


/**
 * Creates a keyset view.
 *
 * @param {string} keyset The non-raw keyset.
 * @private
 */
Controller.prototype.createView_ = function(keyset) {
  if (this.isDisposed()) {
    return;
  }
  var keysetData = this.keysetDataMap_[keyset];
  var layoutId = keysetData[SpecNodeName.LAYOUT];
  var layoutData = this.layoutDataMap_[layoutId];
  if (this.container_.keysetViewMap[keyset] || !layoutData) {
    return;
  }
  var conditions = {};
  conditions[ConditionName.SHOW_ALTGR] =
      keysetData[SpecNodeName.HAS_ALTGR_KEY];

  conditions[ConditionName.SHOW_MENU] =
      keysetData[SpecNodeName.SHOW_MENU_KEY];
  // In symbol and more keysets, we want to show a symbol key in the globe
  // SoftKeyView. So this view should alway visible in the two keysets.
  // Currently, SHOW_MENU_KEY is false for the two keysets, so we use
  // !keysetData[SpecNodeName.SHOW_MENU_KEY] here.
  conditions[ConditionName.SHOW_GLOBE_OR_SYMBOL] =
      !keysetData[SpecNodeName.SHOW_MENU_KEY] ||
      this.adapter_.showGlobeKey;
  conditions[ConditionName.SHOW_EN_SWITCHER_KEY] = false;

  this.container_.addKeysetView(keysetData, layoutData, keyset,
      this.languageCode_, this.model_, this.title_, conditions);
  this.perfTracker_.tick(PerfTracker.TickName.KEYBOARD_CREATED);
};


/**
 * Creates the whole view.
 *
 * @private
 */
Controller.prototype.maybeCreateViews_ = function() {
  if (!this.isSettingReady) {
    return;
  }

  // Emoji is temp keyset which is delay loaded. So active keyset can be 'us'
  // while current keyset is 'emoji'. To make sure delay load can work
  // correctly, here need to create/switch to 'emoji' instead of 'us'.
  var activeKeyset = (this.currentKeyset_ == Controller.EMOJI_VIEW_CODE_) ?
      this.currentKeyset_ : this.getActiveKeyset_();
  var remappedActiveKeyset = this.getRemappedKeyset_(activeKeyset);
  var created = false;
  if (this.keysetDataMap_[remappedActiveKeyset]) {
    this.createView_(remappedActiveKeyset);
    this.switchToKeyset(activeKeyset);
    created = true;
  }
  // Async creating the non-active keysets to reduce the latency of showing the
  // active keyset.
  var keyLen = Object.keys(this.keysetDataMap_).length;
  if (created && keyLen > 1 || !created && keyLen > 0) {
    if (Controller.DISABLE_DELAY_LOADING_FOR_TEST) {
      for (var keyset in this.keysetDataMap_) {
        this.createView_(keyset);
      }
    } else {
      goog.Timer.callOnce((function() {
        for (var keyset in this.keysetDataMap_) {
          this.createView_(keyset);
        }
      }).bind(this));
    }
  }
};


/**
 * Switch to a specific keyboard.
 *
 * @param {string} keyset The keyset name.
 */
Controller.prototype.switchToKeyset = function(keyset) {
  if (!this.isSettingReady || this.adapter_.isSwitching()) {
    return;
  }

  var contextType = this.adapter_.contextType;
  var ret = this.container_.switchToKeyset(this.getRemappedKeyset_(keyset),
      this.title_, this.adapter_.isPasswordBox(), this.adapter_.isA11yMode,
      keyset, this.contextTypeToLastKeysetMap_[contextType] ||
      this.getActiveKeyset_(), this.languageCode_);

  // If it is the sub keyset switching, emoji, or in hotrod mode, don't record
  // the keyset.
  if (!this.isSubKeyset_(this.currentKeyset_, keyset) &&
      keyset != Controller.EMOJI_VIEW_CODE_ &&
      !this.adapter_.isHotrod) {
    // Update the keyset of current context type.
    this.contextTypeToKeysetMap_[this.currentInputMethod_][contextType] =
        keyset;
  }

  if (ret) {
    this.updateLanguageState_(this.currentKeyset_, keyset);
    this.currentKeyset_ = keyset;
    this.resize(Controller.DEV);
    this.statistics_.recordLayout(keyset, this.adapter_.isA11yMode);
    this.perfTracker_.tick(PerfTracker.TickName.KEYBOARD_SHOWN);
    this.perfTracker_.stop();
  } else {
    // Sets the current keyset for delay switching.
    this.currentKeyset_ = keyset;
    this.loadResource_(keyset);
  }

  // TODO: The 'us' part of this code is a workaround an issue where other xkb
  // languages seem to be sharing options between each other.
  this.isKeysetUSCompact_ =
      this.currentKeyset_.indexOf(Controller.US_COMPACT_QWERTY_CODE_) >= 0;
  // If we're switching to a new keyset, we don't want spacebar to trigger
  // another keyset switch.
  this.returnToLetterKeysetOnSpace_ = false;
  if (this.gestureTypingEnabled_()) {
    this.container_.gestureCanvasView.clear();
  }
};


/**
 * Callback when the configuration is loaded.
 *
 * @param {!i18n.input.chrome.inputview.events.ConfigLoadedEvent} e The event.
 * @private
 */
Controller.prototype.onConfigLoaded_ = function(e) {
  if (this.isDisposed()) {
    return;
  }
  var data = e.data;
  var keyboardCode = data[i18n.input.chrome.inputview.SpecNodeName.ID];
  this.keysetDataMap_[keyboardCode] = data;
  this.perfTracker_.tick(PerfTracker.TickName.KEYSET_LOADED);
  var context = data[i18n.input.chrome.inputview.SpecNodeName.ON_CONTEXT];
  if (context && !this.adapter_.isA11yMode) {
    var keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
    if (!keySetMap) {
      keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_] = {};
    }
    keySetMap[context] = keyboardCode;
  }

  var layoutId = data[i18n.input.chrome.inputview.SpecNodeName.LAYOUT];
  data[i18n.input.chrome.inputview.SpecNodeName.LAYOUT] = layoutId;
  var layoutData = this.layoutDataMap_[layoutId];
  if (layoutData) {
    this.maybeCreateViews_();
  } else {
    this.model_.loadLayout(layoutId);
  }
};


/**
 * Resizes the whole UI.
 *
 * @param {boolean=} opt_preventResizeTo True if prevent calling
 *     window.resizeTo. Used in tests and local UI debug.
 */
Controller.prototype.resize = function(opt_preventResizeTo) {
  var height;
  var width;
  var widthPercent;
  var candidateViewHeight;
  var isLandScape = screen.width > screen.height;

  if (this.container_.getElement() == null) {
    // Loading settings is not completed yet. Ignore this event.
    return;
  }

  if (isLandScape) {
    goog.dom.classlist.addRemove(this.container_.getElement(),
        Css.PORTRAIT, Css.LANDSCAPE);
  } else {
    goog.dom.classlist.addRemove(this.container_.getElement(),
        Css.LANDSCAPE, Css.PORTRAIT);
  }
  var isWideScreen = (Math.min(screen.width, screen.height) / Math.max(
      screen.width, screen.height)) < 0.6;
  this.model_.stateManager.covariance.update(isWideScreen, isLandScape,
      this.adapter_.isA11yMode);
  if (this.adapter_.isA11yMode) {
    height = SizeSpec.A11Y_HEIGHT;
    widthPercent = screen.width > screen.height ? SizeSpec.A11Y_WIDTH_PERCENT.
        LANDSCAPE : SizeSpec.A11Y_WIDTH_PERCENT.PORTRAIT;
    candidateViewHeight = SizeSpec.A11Y_CANDIDATE_VIEW_HEIGHT;
  } else {
    var keyset = this.keysetDataMap_[this.currentKeyset_];
    var layout = keyset && keyset[SpecNodeName.LAYOUT];
    var data = layout && this.layoutDataMap_[layout];
    var spec = data && data[SpecNodeName.WIDTH_PERCENT] ||
        SizeSpec.NON_A11Y_WIDTH_PERCENT;
    height = SizeSpec.NON_A11Y_HEIGHT;
    if (isLandScape) {
      if (isWideScreen) {
        widthPercent = spec['LANDSCAPE_WIDE_SCREEN'];
      } else {
        widthPercent = spec['LANDSCAPE'];
      }
    } else {
      widthPercent = spec['PORTRAIT'];
    }
    candidateViewHeight = SizeSpec.NON_A11Y_CANDIDATE_VIEW_HEIGHT;
  }
  var isFloatingMode = this.adapter_.isFloatingVirtualKeyboardEnabled();
  width = isFloatingMode ?
      Math.floor(screen.width * widthPercent) : screen.width;
  widthPercent = isFloatingMode ? 1.0 : widthPercent;

  // Floating virtual keyboard needs to be placed in the bottom of screen and
  // centered when initially shows up. innerHeight == 0 is used as heuristic to
  // check if keyboard is showing up for the first time.
  if (isFloatingMode && window.innerHeight == 0) {
    window.moveTo((screen.width - width) / 2, screen.height - height);
  }

  if ((window.innerHeight != height || window.innerWidth != width) &&
      !opt_preventResizeTo) {
    if (this.lastResizeHeight_ != height || window.innerWidth != width) {
      this.lastResizeHeight_ = height;
      window.resizeTo(width, height);
    }
    return;
  }

  this.container_.setContainerSize(width, height, widthPercent,
      candidateViewHeight);
  this.container_.candidateView.setToolbarVisible(this.shouldShowToolBar_());
  if (this.container_.currentKeysetView) {
    this.isKeyboardReady = true;
  }

  // Transmit the new layout to the decoder.
  if (this.gestureTypingEnabled_()) {
    this.adapter_.sendKeyboardLayout(
        this.container_.currentKeysetView.getKeyboardLayoutForGesture());
  }
};


/**
 * Loads the resources, for currentKeyset, passwdKeyset, handwriting,
 * emoji, etc.
 *
 * @private
 */
Controller.prototype.loadAllResources_ = function() {
  var keysetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
  goog.array.forEach([keysetMap[ContextType.DEFAULT],
    keysetMap[ContextType.PASSWORD]], function(keyset) {
    this.loadResource_(keyset);
  }, this);
};


/**
 * Gets the remapped keyset.
 *
 * @param {string} keyset .
 * @return {string} The remapped keyset.
 * @private
 */
Controller.prototype.getRemappedKeyset_ = function(keyset) {
  if (goog.array.contains(util.KEYSETS_USE_US, keyset)) {
    return 'us-ltr';
  }
  var match = keyset.match(/^(.*)-rtl$/);
  if (match && goog.array.contains(util.KEYSETS_USE_US, match[1])) {
    return 'us-rtl';
  }
  return keyset;
};


/**
 * Loads a single resource.
 *
 * @param {string} keyset .
 * loaded.
 * @private
 */
Controller.prototype.loadResource_ = function(keyset) {
  var remapped = this.getRemappedKeyset_(keyset);
  if (!this.keysetDataMap_[remapped]) {
    if (/^m17n:/.test(remapped)) {
      this.m17nModel_.loadConfig(remapped);
    } else {
      this.model_.loadConfig(remapped);
    }
    return;
  }

  var layoutId = this.keysetDataMap_[remapped][SpecNodeName.LAYOUT];
  if (!this.layoutDataMap_[layoutId]) {
    this.model_.loadLayout(layoutId);
    return;
  }
};


/**
 * Sets the keyboard.
 *
 * @param {string} keyset The keyboard keyset.
 * @param {string} languageCode The language code for this keyboard.
 * @param {string} passwordLayout The layout for password box.
 * @param {string} title The title for this keyboard.
 */
Controller.prototype.initialize = function(keyset, languageCode, passwordLayout,
    title) {
  this.perfTracker_.restart();
  this.adapter_.getCurrentInputMethod(function(currentInputMethod) {
    // TODO: remove this hack as soon as the manifest is fixed in chromium.
    if (languageCode == 'ko') {
      if (currentInputMethod.indexOf('hangul_2set') > 0) {
        keyset = 'm17n:ko_2set';
      }
    }
    this.languageCode_ = languageCode;
    // If can't get the current input method, set the original keyset as
    // current input method.
    this.currentInputMethod_ = currentInputMethod || keyset;
    var keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
    if (!keySetMap) {
      keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_] = {};
    }
    keySetMap[ContextType.PASSWORD] = passwordLayout;
    keySetMap[ContextType.DEFAULT] = keyset;

    this.initialKeyset_ = keyset;
    this.title_ = title;
    this.isSettingReady = false;
    this.model_.settings = new i18n.input.chrome.inputview.Settings();
    this.model_.stateManager.isEnMode = false;
    this.adapter_.initialize();
    this.loadAllResources_();
    this.switchToKeyset(this.getActiveKeyset_());

    // Set language attribute and font of body.
    document.body.setAttribute('lang', this.languageCode_);
    goog.dom.classlist.add(document.body, Css.FONT);
  }.bind(this));
};


/** @override */
Controller.prototype.disposeInternal = function() {
  goog.dispose(this.container_);
  goog.dispose(this.adapter_);
  goog.dispose(this.handler_);
  goog.dispose(this.soundController_);

  goog.base(this, 'disposeInternal');
};


/**
 * Gets the handwriting Input Tool code of current language code.
 *
 * @return {string} The handwriting Input Tool code.
 * @private
 */
Controller.prototype.getHwtInputToolCode_ = function() {
  return this.languageCode_.split(/_|-/)[0] +
      Controller.HANDWRITING_CODE_SUFFIX_;
};


/**
 * True to enable settings link.
 *
 * @return {boolean} .
 */
Controller.prototype.shouldEnableSettings = function() {
  return !this.adapter_.screen || this.adapter_.screen == 'normal';
};


/**
 * Gets the active keyset, if there is a keyset to switch, return it.
 * otherwise if it's a password box, return the password keyset,
 * otherwise return the current keyset.
 *
 * @return {string} .
 * @private
 */
Controller.prototype.getActiveKeyset_ = function() {
  var keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
  return keySetMap[this.adapter_.contextType] || this.initialKeyset_;
};


/**
 * True if keysetB is the sub keyset of keysetA.
 *
 * @param {string} keysetA .
 * @param {string} keysetB .
 * @return {boolean} .
 * @private
 */
Controller.prototype.isSubKeyset_ = function(keysetA, keysetB) {
  var segmentsA = keysetA.split('.');
  var segmentsB = keysetB.split('.');
  return segmentsA.length >= 2 && segmentsB.length >= 2 &&
      segmentsA[0] == segmentsB[0] && segmentsA[1] == segmentsB[1];
};


/**
 * Updates the compact pinyin to set the inputcode for english and pinyin.
 *
 * @param {string} fromRawKeyset .
 * @param {string} toRawKeyset .
 * @private
 */
Controller.prototype.updateLanguageState_ =
    function(fromRawKeyset, toRawKeyset) {
  var toggle = false;
  var toggleState = false;
  if (fromRawKeyset != toRawKeyset) {
    // Deal with the switch logic to/from English within the compact layout.
    if (fromRawKeyset.indexOf('en.compact') *
        toRawKeyset.indexOf('en.compact') < 0) { // Switches between non-en/en.
      toggle = true;
      toggleState = toRawKeyset.indexOf('en.compact') == -1;
    } else if (fromRawKeyset.indexOf(toRawKeyset) == 0 &&
        fromRawKeyset.indexOf('.compact') > 0 &&
        goog.array.contains(util.KEYSETS_HAVE_EN_SWTICHER, toRawKeyset) ||
        fromRawKeyset && toRawKeyset.indexOf(fromRawKeyset) == 0 &&
        toRawKeyset.indexOf('.compact') > 0) {
      // Switch between full/compact layouts, reset the default button and
      // language.
      toggle = true;
      toggleState = true;
    }
  }
  if (toggle) {
    this.adapter_.toggleLanguageState(toggleState);
    this.model_.stateManager.isEnMode = !toggleState;
    this.container_.currentKeysetView.update();
  }
};


/**
 * Records the stats which will be reported when input view is closing.
 *
 * @param {string} name The metrics name.
 * @param {number} count The count value for histogram.
 * @param {number} max .
 * @param {number} bucketCount .
 * @private
 */
Controller.prototype.recordStatsForClosing_ = function(
    name, count, max, bucketCount) {
  if (!this.statsForClosing_[name]) {
    this.statsForClosing_[name] = [count, max, bucketCount];
  } else {
    this.statsForClosing_[name][0] += count;
    this.statsForClosing_[name][1] = max;
    this.statsForClosing_[name][2] = bucketCount;
  }
};


/**
 * Handles language state changing event.
 *
 * @param {!events.MessageEvent} e .
 * @private
 */
Controller.prototype.onUpdateToggleLanguageState_ = function(e) {
  if (this.adapter_.isA11yMode || this.currentKeyset_.indexOf('.compact') < 0) {
    // e.msg value means whether is Chinese mode now.
    if (this.model_.stateManager.isEnMode == e.msg) {
      this.model_.stateManager.isEnMode = !e.msg;
      this.updateToggleLanguageKeyset_();
      this.container_.currentKeysetView.update();
    }
  } else {
    var pos = this.currentKeyset_.indexOf('en.compact');
    var toKeyset;
    if (pos > 0) { // Means en mode
      if (e.msg) { // Needs switch cn mode
        toKeyset = this.currentKeyset_.replace('en.compact', 'compact');
      }
    } else {
      if (!e.msg) { // Needs switch en mode
        toKeyset = this.currentKeyset_.replace('compact', 'en.compact');
      }
    }
    if (toKeyset) {
      this.resetAll();
      this.switchToKeyset(toKeyset);
    }
  }
};


/**
 * Update keyset when language state changes.
 *
 * @return {boolean}
 * @private
 */
Controller.prototype.updateToggleLanguageKeyset_ = function() {
  var pos = this.currentKeyset_.indexOf('.us');
  var toKeyset;
  if (pos > 0) {
    toKeyset = this.currentKeyset_.replace('.us', '');
    if (goog.array.contains(util.KEYSETS_SWITCH_WITH_US, toKeyset)) {
      this.switchToKeyset(toKeyset);
      return true;
    }
  }
  else if (goog.array.contains(util.KEYSETS_SWITCH_WITH_US,
      this.currentKeyset_)) {
    toKeyset = this.currentKeyset_ + '.us';
    this.switchToKeyset(toKeyset);
      return true;
  }
  return false;
};

});  // goog.scope
