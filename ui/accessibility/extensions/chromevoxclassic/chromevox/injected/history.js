// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Stores the history of a ChromeVox session.
 */

goog.provide('cvox.History');

goog.require('cvox.DomUtil');
goog.require('cvox.NodeBreadcrumb');

/**
 * A history event is stored in the cvox.History object and contains all the
 * information about a single ChromeVox event.
 * @param {Object=} opt_json A simple initializer object.
 * @constructor
 */
cvox.HistoryEvent = function(opt_json) {
  /**
   * The start time of this event, in msec since epoch.
   * @type {number}
   * @private
   */
  this.startTime_;

  /**
   * The end time of this event, in msec since epoch.
   * @type {number}
   * @private
   */
  this.endTime_;

  /**
   * The user command executed in this event.
   * @type {string}
   * @private
   */
  this.userCommand_;

  /**
   * An array of spoken output.
   * @type {Array<string>}
   * @private
   */
  this.spoken_ = [];

  /**
   * The ChromeVox tag for the current node at the end of this event.
   * @type {number}
   * @private
   */
  this.cvTag_;

  /**
   * True if replayed.
   * @type {boolean}
   * @private
   */
  this.replayed_ = false;

  if (opt_json) {
    this.replayed_ = true;
    this.userCommand_ = opt_json['cmd'];
  } else {
    this.startTime_ = new Date().getTime();
  }
};

/**
 * @param {string} functionName The name of the user command.
 * @return {cvox.HistoryEvent} this for chaining.
 */
cvox.HistoryEvent.prototype.withUserCommand = function(functionName) {
  if (this.userCommand_) {
    window.console.error('Two user commands on ' + functionName, this);
    return this;
  }
  this.userCommand_ = functionName;
  return this;
};

/**
 * @param {string} str The text spoken.
 * @return {cvox.HistoryEvent} this for chaining.
 */
cvox.HistoryEvent.prototype.speak = function(str) {
  this.spoken_.push(str);
  return this;
};

/**
 * Called when the event is done.  We can expect nothing else will be added to
 * the event after this call.
 * @return {cvox.HistoryEvent} this for chaining.
 */
cvox.HistoryEvent.prototype.done = function() {
  this.endTime_ = new Date().getTime();

  this.cvTag_ = cvox.NodeBreadcrumb.getInstance().tagCurrentNode();

  window.console.log('User command done.', this);
  return this;
};

/**
 * Outputs the event as a simple object
 * @return {Object} A object representation of the event.
 */
cvox.HistoryEvent.prototype.outputObject = function() {
  return {
    'start': this.startTime_,
    'end': this.endTime_,
    'cmd': this.userCommand_,
    'spoken': this.spoken_
  };
};

/**
 * Outputs a HTML element that can be added to the DOM.
 * @return {Element} The HTML element.
 */
cvox.HistoryEvent.prototype.outputHTML = function() {
  var div = document.createElement('div');
  div.className = 'cvoxHistoryEvent';
  var dur = this.endTime_ - this.startTime_;
  div.innerHTML = this.userCommand_ + ' (' + dur + 'ms)';
  for (var i = 0; i < this.spoken_.length; i++) {
    var sdiv = document.createElement('div');
    sdiv.className = 'cvoxHistoryEventSpoken';
    sdiv.innerHTML = this.spoken_[i].substr(0, 20);
    if (this.spoken_[i].length > 20) {
      sdiv.innerHTML += '...';
    }
    div.appendChild(sdiv);
  }
  return div;
};

/**
 * Outputs Javascript to replay the command and assert the output.
 * @return {string} The Javascript.
 */
cvox.HistoryEvent.prototype.outputJs = function() {
  var o = 'this.waitForCalm(this.userCommand, \'' + this.userCommand_ + '\')';
  if (this.spoken_.length > 0) {
    o += '\n  .waitForCalm(this.assertSpoken, \'' +
        cvox.DomUtil.collapseWhitespace(this.spoken_.join(' ')) + '\');\n';
  } else {
    o += ';\n';
  }
  return o;
};


/**
 * @constructor
 * @implements {cvox.TtsInterface}
 */
cvox.History = function() {
  this.recording_ = false;

  this.events_ = [];
  this.markers_ = {};
  this.currentEvent_ = null;

  this.mainDiv_ = null;
  this.listDiv_ = null;
  this.styleDiv_ = null;

  this.bigBoxDiv_ = null;

  // NOTE(deboer): Currently we only ever have one cvox.History, but
  // if we ever have more than one, we need multiple NodeBreadcrumbs as well.
  this.nodeBreadcrumb_ = cvox.NodeBreadcrumb.getInstance();

};
goog.addSingletonGetter(cvox.History);

/**
 * Adds a list div to the DOM for debugging.
 * @private
 */
cvox.History.prototype.addListDiv_ = function() {
  this.mainDiv_ = document.createElement('div');
  this.mainDiv_.style.position = 'fixed';
  this.mainDiv_.style.bottom = '0';
  this.mainDiv_.style.right = '0';
  this.mainDiv_.style.zIndex = '999';

  this.listDiv_ = document.createElement('div');
  this.listDiv_.id = 'cvoxEventList';
  this.mainDiv_.appendChild(this.listDiv_);

  var buttonDiv = document.createElement('div');
  var button = document.createElement('button');
  button.onclick = cvox.History.sendToFeedback;
  button.innerHTML = 'Create bug';
  buttonDiv.appendChild(button);
  this.mainDiv_.appendChild(buttonDiv);

  var dumpDiv = document.createElement('div');
  var dumpButton = document.createElement('button');
  dumpButton.onclick = cvox.History.dumpJs;
  dumpButton.innerHTML = 'Dump test case';
  dumpDiv.appendChild(dumpButton);
  this.mainDiv_.appendChild(dumpDiv);

  document.body.appendChild(this.mainDiv_);

  this.styleDiv_ = document.createElement('style');
  this.styleDiv_.innerHTML =
      '.cvoxHistoryEventSpoken { color: gray; font-size: 75% }';
  document.body.appendChild(this.styleDiv_);
};


/**
 * Removes the list div.
 * @private
 */
cvox.History.prototype.removeListDiv_ = function() {
  document.body.removeChild(this.mainDiv_);
  document.body.removeChild(this.styleDiv_);
  this.mainDiv_ = null;
  this.listDiv_ = null;
  this.styleDiv_ = null;
};


/**
 * Adds a big text box in the middle of the screen
 * @private
 */
cvox.History.prototype.addBigTextBox_ = function() {
  var bigBoxDiv = document.createElement('div');
  bigBoxDiv.style.position = 'fixed';
  bigBoxDiv.style.top = '0';
  bigBoxDiv.style.left = '0';
  bigBoxDiv.style.zIndex = '999';

  var textarea = document.createElement('textarea');
  textarea.style.width = '500px';
  textarea.style.height = '500px';
  textarea.innerHTML = this.dumpJsOutput_();
  bigBoxDiv.appendChild(textarea);

  var buttons = document.createElement('div');
  bigBoxDiv.appendChild(buttons);

  function link(name, func) {
    var linkElt = document.createElement('button');
    linkElt.onclick = func;
    linkElt.innerHTML = name;
    buttons.appendChild(linkElt);
  }
  link('Close dialog', function() {
    document.body.removeChild(bigBoxDiv);
  });
  link('Remove fluff', goog.bind(function() {
    textarea.innerHTML = this.dumpJsOutput_(['stopSpeech', 'toggleKeyPrefix']);
  }, this));
  document.body.appendChild(bigBoxDiv);
};



/**
 * Start recording and show the debugging list div.
 */
cvox.History.prototype.startRecording = function() {
  this.recording_ = true;
  this.addListDiv_();
};


/**
 * Stop recording and clear the events array.
 */
cvox.History.prototype.stopRecording = function() {
  this.recording_ = false;
  this.removeListDiv_();
  this.events_ = [];
  this.currentEvent_ = null;
};


/**
 * Called by ChromeVox when it enters a user command.
 * @param {string} functionName The function name.
 */
cvox.History.prototype.enterUserCommand = function(functionName) {
  if (!this.recording_) {
    return;
  }
  if (this.currentEvent_) {
    window.console.error(
        'User command ' + functionName + ' overlaps current event',
        this.currentEvent_);
  }
  this.currentEvent_ = new cvox.HistoryEvent()
      .withUserCommand(functionName);
  this.events_.push(this.currentEvent_);
};


/**
 * Called by ChromeVox when it exits a user command.
 * @param {string} functionName The function name, useful for debugging.
 */
cvox.History.prototype.exitUserCommand = function(functionName) {
  if (!this.recording_ || !this.currentEvent_) {
    return;
  }
  this.currentEvent_.done();
  this.listDiv_.appendChild(this.currentEvent_.outputHTML());
  this.currentEvent_ = null;
};


/** @override */
cvox.History.prototype.speak = function(str, mode, props) {
  if (!this.recording_) {
    return this;
  }
  if (!this.currentEvent_) {
    window.console.error('Speak called outside of a user command.');
    return this;
  }
  this.currentEvent_.speak(str);
  return this;
};


/** @override */
cvox.History.prototype.isSpeaking = function() { return false; };
/** @override */
cvox.History.prototype.stop = function() { };
/** @override */
cvox.History.prototype.addCapturingEventListener = function(listener) { };
/** @override */
cvox.History.prototype.increaseOrDecreaseProperty =
    function(propertyName, increase) { };
/** @override */
cvox.History.prototype.propertyToPercentage = function(property) { };
/** @override */
cvox.History.prototype.getDefaultProperty = function(property) { };


/** TODO: add doc comment. */
cvox.History.dumpJs = function() {
  var history = cvox.History.getInstance();
  history.addBigTextBox_();
  window.console.log(history.dumpJsOutput_());
};


/**
 * @param {Array<string>=} opt_skipCommands
 * @return {string} A string of Javascript output.
 * @private
 */
cvox.History.prototype.dumpJsOutput_ = function(opt_skipCommands) {
  var skipMap = {};
  if (opt_skipCommands) {
    opt_skipCommands.forEach(function(e) { skipMap[e] = 1; });
  }
  // TODO: pretty print
  return ['/*DOC: += ',
          this.nodeBreadcrumb_.dumpWalkedDom().innerHTML, '*/\n']
      .concat(this.events_
          .filter(function(e) { return ! (e.userCommand_ in skipMap); })
          .map(function(e) { return e.outputJs(); })).join('');
};


/**
 * Send the history to Google Feedback.
 */
cvox.History.sendToFeedback = function() {
  var history = cvox.History.getInstance();
  var output = history.events_.map(function(e) {
    return e.outputObject();
  });

  var feedbackScript = document.createElement('script');
  feedbackScript.type = 'text/javascript';
  feedbackScript.src = 'https://www.gstatic.com/feedback/api.js';

  var runFeedbackScript = document.createElement('script');
  runFeedbackScript.type = 'text/javascript';
  runFeedbackScript.innerHTML =
      'userfeedback.api.startFeedback(' +
          '{ productId: \'76092\' }, ' +
          '{ cvoxHistory: ' + cvox.ChromeVoxJSON.stringify(
              cvox.ChromeVoxJSON.stringify(output)) + ' });';

  feedbackScript.onload = function() {
    document.body.appendChild(runFeedbackScript);
  };

  document.body.appendChild(feedbackScript);
};


// Add more events: key press, DOM
