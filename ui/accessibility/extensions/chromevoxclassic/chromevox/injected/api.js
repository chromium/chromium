// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Public APIs to enable web applications to communicate
 * with ChromeVox.
 */

if (typeof(goog) != 'undefined' && goog.provide) {
  goog.provide('cvox.Api');
  goog.provide('cvox.Api.Math');
}

if (typeof(goog) != 'undefined' && goog.require) {
  goog.require('cvox.ApiImplementation');
}

(function() {
   /*
    * Private data and methods.
    */

   /**
    * The name of the port between the content script and background page.
    * @type {string}
    * @const
    */
   var PORT_NAME = 'cvox.Port';

   /**
    * The name of the message between the page and content script that sets
    * up the bidirectional port between them.
    * @type {string}
    * @const
    */
   var PORT_SETUP_MSG = 'cvox.PortSetup';

   /**
    * The message between content script and the page that indicates the
    * connection to the background page has been lost.
    * @type {string}
    * @const
    */
   var DISCONNECT_MSG = 'cvox.Disconnect';

   /**
    * The channel between the page and content script.
    * @type {MessageChannel}
    */
   var channel;

   /**
    * Tracks whether or not the ChromeVox API should be considered active.
    * @type {boolean}
    */
   var isActive_ = false;

   /**
    * The next id to use for async callbacks.
    * @type {number}
    */
   var nextCallbackId_ = 1;

   /**
    * Map from callback ID to callback function.
    * @type {Object<number, function(*)>}
    */
   var callbackMap_ = {};

   /**
    * Internal function to connect to the content script.
    */
   function connect_() {
     if (channel) {
       // If there is already an existing channel, close the existing ports.
       channel.port1.close();
       channel.port2.close();
       channel = null;
     }

     channel = new MessageChannel();
     window.postMessage(PORT_SETUP_MSG, [channel.port2], '*');
     channel.port1.onmessage = function(event) {
       if (event.data == DISCONNECT_MSG) {
         channel = null;
       }
       try {
         var message = JSON.parse(event.data);
         if (message['id'] && callbackMap_[message['id']]) {
           callbackMap_[message['id']](message);
           delete callbackMap_[message['id']];
         }
       } catch (e) {
       }
     };
   }

   /**
    * Internal function to send a message to the content script and
    * call a callback with the response.
    * @param {Object} message A serializable message.
    * @param {function(*)} callback A callback that will be called
    *     with the response message.
    */
   function callAsync_(message, callback) {
     var id = nextCallbackId_;
     nextCallbackId_++;
     if (message['args'] === undefined) {
       message['args'] = [];
     }
     message['args'] = [id].concat(message['args']);
     callbackMap_[id] = callback;
     channel.port1.postMessage(JSON.stringify(message));
   }

   /**
    * Wraps callAsync_ for sending speak requests.
    * @param {Object} message A serializable message.
    * @param {Object=} properties Speech properties to use for this utterance.
    * @private
    */
   function callSpeakAsync_(message, properties) {
     var callback = null;
     /* Use the user supplied callback as callAsync_'s callback. */
     if (properties && properties['endCallback']) {
       callback = properties['endCallback'];
     }
     callAsync_(message, callback);
   };


   /*
    * Public API.
    */

   if (!window['cvox']) {
     window['cvox'] = {};
   }
   var cvox = window.cvox;


   /**
    * ApiImplementation - this is only visible if all the scripts are compiled
    * together like in the Android case. Otherwise, implementation will remain
    * null which means communication must happen over the bridge.
    *
    * @type {*}
    */
   var implementation = null;
   if (typeof(cvox.ApiImplementation) != 'undefined') {
     implementation = cvox.ApiImplementation;
   }


   /**
    * @constructor
    */
   cvox.Api = function() {
   };

   /**
    * Internal-only function, only to be called by the content script.
    * Enables the API and connects to the content script.
    */
   cvox.Api.internalEnable = function() {
     isActive_ = true;
     if (!implementation) {
       connect_();
     }
     var event = document.createEvent('UIEvents');
     event.initEvent('chromeVoxLoaded', true, false);
     document.dispatchEvent(event);
   };

   /**
    * Internal-only function, only to be called by the content script.
    * Disables the ChromeVox API.
    */
   cvox.Api.internalDisable = function() {
     isActive_ = false;
     channel = null;
     var event = document.createEvent('UIEvents');
     event.initEvent('chromeVoxUnloaded', true, false);
     document.dispatchEvent(event);
   };

   /**
    * Returns true if ChromeVox is currently running. If the API is available
    * in the JavaScript namespace but this method returns false, it means that
    * the user has (temporarily) disabled ChromeVox.
    *
    * You can listen for the 'chromeVoxLoaded' event to be notified when
    * ChromeVox is loaded.
    *
    * @return {boolean} True if ChromeVox is currently active.
    */
   cvox.Api.isChromeVoxActive = function() {
     if (implementation) {
       return isActive_;
     }
     return !!channel;
   };

   /**
    * Speaks the given string using the specified queueMode and properties.
    *
    * @param {string} textString The string of text to be spoken.
    * @param {number=} queueMode Valid modes are 0 for flush; 1 for queue.
    * @param {Object=} properties Speech properties to use for this utterance.
    */
   cvox.Api.speak = function(textString, queueMode, properties) {
     if (!cvox.Api.isChromeVoxActive()) {
       return;
     }

     if (implementation) {
       implementation.speak(textString, queueMode, properties);
     } else {
       var message = {
         'cmd': 'speak',
         'args': [textString, queueMode, properties]
       };
       callSpeakAsync_(message, properties);
     }
   };

   /**
    * Speaks a description of the given node.
    *
    * @param {Node} targetNode A DOM node to speak.
    * @param {number=} queueMode Valid modes are 0 for flush; 1 for queue.
    * @param {Object=} properties Speech properties to use for this utterance.
    */
   cvox.Api.speakNode = function(targetNode, queueMode, properties) {
     if (!cvox.Api.isChromeVoxActive()) {
       return;
     }

     if (implementation) {
       implementation.speak(cvox.DomUtil.getName(targetNode),
           queueMode, properties);
     } else {
       var message = {
         'cmd': 'speakNodeRef',
         'args': [cvox.ApiUtils.makeNodeReference(targetNode), queueMode,
             properties]
       };
       callSpeakAsync_(message, properties);
     }
   };

   /**
    * Stops speech.
    */
   cvox.Api.stop = function() {
     if (!cvox.Api.isChromeVoxActive()) {
       return;
     }

     if (implementation) {
       implementation.stop();
     } else {
       var message = {
         'cmd': 'stop'
       };
       channel.port1.postMessage(JSON.stringify(message));
     }
   };

   /**
    * Plays the specified earcon sound.
    *
    * @param {string} earcon An earcon name.
    * Valid names are:
    *   ALERT_MODAL
    *   ALERT_NONMODAL
    *   BUTTON
    *   CHECK_OFF
    *   CHECK_ON
    *   EDITABLE_TEXT
    *   INVALID_KEYPRESS
    *   LINK
    *   LISTBOX
    *   LIST_ITEM
    *   OBJECT_CLOSE
    *   OBJECT_OPEN
    *   OBJECT_SELECT
    *   PAGE_START_LOADING
    *   RECOVER_FOCUS
    *   SKIP
    *   WRAP
    *   WRAP_EDGE
    * This list may expand over time.
    */
   cvox.Api.playEarcon = function(earcon) {
     if (!cvox.Api.isChromeVoxActive()) {
       return;
     }
     if (implementation) {
       implementation.playEarcon(earcon);
     } else {
       var message = {
         'cmd': 'playEarcon',
         'args': [earcon]
       };
       channel.port1.postMessage(JSON.stringify(message));
     }
   };

   /**
    * Synchronizes ChromeVox's internal cursor to the targetNode.
    * Note that this will NOT trigger reading unless given the
    * optional argument; it is for setting the internal ChromeVox
    * cursor so that when the user resumes reading, they will be
    * starting from a reasonable position.
    *
    * @param {Node} targetNode The node that ChromeVox should be synced to.
    * @param {boolean=} speakNode If true, speaks out the node.
    */
   cvox.Api.syncToNode = function(targetNode, speakNode) {
     if (!cvox.Api.isChromeVoxActive() || !targetNode) {
       return;
     }

     if (implementation) {
       implementation.syncToNode(targetNode, speakNode);
     } else {
       var message = {
         'cmd': 'syncToNodeRef',
         'args': [cvox.ApiUtils.makeNodeReference(targetNode), speakNode]
       };
       channel.port1.postMessage(JSON.stringify(message));
     }
   };

   /**
    * Retrieves the current node and calls the given callback function with it.
    *
    * @param {Function} callback The function to be called.
    */
   cvox.Api.getCurrentNode = function(callback) {
     if (!cvox.Api.isChromeVoxActive() || !callback) {
       return;
     }

     if (implementation) {
       callback(cvox.ChromeVox.navigationManager.getCurrentNode());
     } else {
       callAsync_({'cmd': 'getCurrentNode'}, function(response) {
         callback(cvox.ApiUtils.getNodeFromRef(response['currentNode']));
       });
     }
   };

   /**
    * Specifies how the targetNode should be spoken using an array of
    * NodeDescriptions.
    *
    * @param {Node} targetNode The node that the NodeDescriptions should be
    * spoken using the given NodeDescriptions.
    * @param {Array<Object>} nodeDescriptions The Array of
    * NodeDescriptions for the given node.
    */
   cvox.Api.setSpeechForNode = function(targetNode, nodeDescriptions) {
     if (!cvox.Api.isChromeVoxActive() || !targetNode || !nodeDescriptions) {
       return;
     }
     targetNode.setAttribute('cvoxnodedesc', JSON.stringify(nodeDescriptions));
   };

   /**
    * Simulate a click on an element.
    *
    * @param {Element} targetElement The element that should be clicked.
    * @param {boolean} shiftKey Specifies if shift is held down.
    */
   cvox.Api.click = function(targetElement, shiftKey) {
     if (!cvox.Api.isChromeVoxActive() || !targetElement) {
       return;
     }

     if (implementation) {
       cvox.DomUtil.clickElem(targetElement, shiftKey, true);
     } else {
       var message = {
         'cmd': 'clickNodeRef',
         'args': [cvox.ApiUtils.makeNodeReference(targetElement), shiftKey]
       };
       channel.port1.postMessage(JSON.stringify(message));
     }
   };

   /**
    * Returns the build info.
    *
    * @param {function(string)} callback Function to receive the build info.
    */
   cvox.Api.getBuild = function(callback) {
     if (!cvox.Api.isChromeVoxActive() || !callback) {
       return;
     }
     if (implementation) {
       callback(cvox.BuildInfo.build);
     } else {
       callAsync_({'cmd': 'getBuild'}, function(response) {
           callback(response['build']);
       });
     }
   };

   /**
    * Returns the ChromeVox version, a string of the form 'x.y.z',
    * like '1.18.0'.
    *
    * @param {function(string)} callback Function to receive the version.
    */
   cvox.Api.getVersion = function(callback) {
     if (!cvox.Api.isChromeVoxActive() || !callback) {
       return;
     }
     if (implementation) {
       callback(cvox.ChromeVox.version + '');
     } else {
       callAsync_({'cmd': 'getVersion'}, function(response) {
           callback(response['version']);
       });
     }
   };

   /**
    * Returns the key codes of the ChromeVox modifier keys.
    * @param {function(Array<number>)} callback Function to receive the keys.
    */
   cvox.Api.getCvoxModifierKeys = function(callback) {
     if (!cvox.Api.isChromeVoxActive() || !callback) {
       return;
     }
     if (implementation) {
       callback(cvox.KeyUtil.cvoxModKeyCodes());
     } else {
       callAsync_({'cmd': 'getCvoxModKeys'}, function(response) {
         callback(response['keyCodes']);
       });
     }
   };

   /**
    * Returns if ChromeVox will handle this key event.
    * @param {Event} keyEvent A key event.
    * @param {function(boolean)} callback Function to receive the keys.
    */
   cvox.Api.isKeyShortcut = function(keyEvent, callback) {
     if (!callback) {
       return;
     }
     if (!cvox.Api.isChromeVoxActive()) {
       callback(false);
       return;
     }
     /* TODO(peterxiao): Ignore these keys until we do this in a smarter way. */
     var KEY_IGNORE_LIST = [
      37, /* Left arrow. */
      39  /* Right arrow. */
     ];
     if (KEY_IGNORE_LIST.indexOf(keyEvent.keyCode) && !keyEvent.altKey &&
         !keyEvent.shiftKey && !keyEvent.ctrlKey && !keyEvent.metaKey) {
       callback(false);
       return;
     }

     if (implementation) {
       var keySeq = cvox.KeyUtil.keyEventToKeySequence(keyEvent);
       callback(cvox.ChromeVoxKbHandler.handlerKeyMap.hasKey(keySeq));
     } else {
       var strippedKeyEvent = {};
       /* Blacklist these props so we can safely stringify. */
       var BLACK_LIST_PROPS = ['target', 'srcElement', 'currentTarget', 'view'];
       for (var prop in keyEvent) {
         if (BLACK_LIST_PROPS.indexOf(prop) === -1) {
           strippedKeyEvent[prop] = keyEvent[prop];
         }
       }
       var message = {
         'cmd': 'isKeyShortcut',
         'args': [strippedKeyEvent]
       };
       callAsync_(message, function(response) {
         callback(response['isHandled']);
       });
     }
   };

   /**
    * Set key echoing on key press.
    * @param {boolean} keyEcho Whether key echoing should be on or off.
    */
   cvox.Api.setKeyEcho = function(keyEcho) {
     if (!cvox.Api.isChromeVoxActive()) {
       return;
     }

     if (implementation) {
       implementation.setKeyEcho(keyEcho);
     } else {
       var message = {
         'cmd': 'setKeyEcho',
         'args': [keyEcho]
       };
       channel.port1.postMessage(JSON.stringify(message));
     }
   };

   /**
    * Exports the ChromeVox math API.
    * TODO(dtseng, sorge): Requires more detailed documentation for class
    * members.
    * @constructor
    */
   cvox.Api.Math = function() {};

   // TODO(dtseng, sorge): This need not be specific to math; once speech engine
   // stabilizes, we can generalize.
   // TODO(dtseng, sorge): This API is way too complicated; consolidate args
   // when re-thinking underlying representation. Some of the args don't have a
   // well-defined purpose especially for a caller.
   /**
    * Defines a math speech rule.
    * @param {string} name Rule name.
    * @param {string} dynamic Dynamic constraint annotation. In the case of a
    *      math rule it consists of a domain.style string.
    * @param {string} action An action of rule components.
    * @param {string} prec XPath or custom function constraining match.
    * @param {...string} constraints Additional constraints.
    */
   cvox.Api.Math.defineRule =
       function(name, dynamic, action, prec, constraints) {
     if (!cvox.Api.isChromeVoxActive()) {
       return;
     }
     var constraintList = Array.prototype.slice.call(arguments, 4);
     var args = [name, dynamic, action, prec].concat(constraintList);
     if (implementation) {
       implementation.Math.defineRule.apply(implementation.Math, args);
     } else {
       var msg = {'cmd': 'Math.defineRule', args: args};
       channel.port1.postMessage(JSON.stringify(msg));
     }
   };

   cvox.Api.internalEnable();

   /**
    * NodeDescription
    * Data structure for holding information on how to speak a particular node.
    * NodeDescriptions will be converted into NavDescriptions for ChromeVox.
    *
    * The string data is separated into context, text, userValue, and annotation
    * to enable ChromeVox to speak each of these with the voice settings that
    * are consistent with how ChromeVox normally presents information about
    * nodes to users.
    *
    * @param {string} context Contextual information that the user should
    * hear first which is not part of main content itself. For example,
    * the user/date of a given post.
    * @param {string} text The main content of the node.
    * @param {string} userValue Anything that the user has entered.
    * @param {string} annotation The role and state of the object.
    */
   // TODO (clchen, deboer): Put NodeDescription into externs for developers
   // building ChromeVox extensions.
   cvox.NodeDescription = function(context, text, userValue, annotation) {
     this.context = context ? context : '';
     this.text = text ? text : '';
     this.userValue = userValue ? userValue : '';
     this.annotation = annotation ? annotation : '';
   };
})();
