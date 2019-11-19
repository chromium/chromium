// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implentation of ChromeVox's public API.
 *
 */

goog.provide('cvox.ApiImplementation');
goog.provide('cvox.ApiImplementation.Math');

goog.require('cvox.ApiUtil');
goog.require('cvox.AriaUtil');
goog.require('cvox.BuildInfo');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxJSON');
goog.require('cvox.DomUtil');
goog.require('cvox.ScriptInstaller');

/**
 * @constructor
 */
cvox.ApiImplementation = function() {
};

/**
 * The URL to the script loader.
 * @type {string}
 */
cvox.ApiImplementation.siteSpecificScriptLoader;

/**
 * The URL base for the site-specific scripts.
 * @type {string}
 */
cvox.ApiImplementation.siteSpecificScriptBase;

/**
 * Inject the API into the page and set up communication with it.
 * @param {function()=} opt_onload A function called when the script is loaded.
 */
cvox.ApiImplementation.init = function(opt_onload) {
  window.addEventListener('message', cvox.ApiImplementation.portSetup, true);
  var scripts = new Array();
  scripts.push(cvox.ChromeVox.host.getFileSrc(
      'chromevox/injected/api_util.js'));
  scripts.push(cvox.ChromeVox.host.getApiSrc());
  scripts.push(cvox.ApiImplementation.siteSpecificScriptLoader);

  var didInstall = cvox.ScriptInstaller.installScript(scripts,
      'cvoxapi', opt_onload, cvox.ApiImplementation.siteSpecificScriptBase);

  if (!didInstall) {
    // If the API script is already installed, just re-enable it.
    if (cvox.Api)
      window.location.href = 'javascript:cvox.Api.internalEnable();';
  }
};

/**
 * This method is called when the content script receives a message from
 * the page.
 * @param {Event} event The DOM event with the message data.
 * @return {boolean} True if default event processing should continue.
 */
cvox.ApiImplementation.portSetup = function(event) {
  if (event.data == 'cvox.PortSetup') {
    cvox.ApiImplementation.port = event.ports[0];
    cvox.ApiImplementation.port.onmessage = function(event) {
      cvox.ApiImplementation.dispatchApiMessage(
          cvox.ChromeVoxJSON.parse(event.data));
    };

    // Stop propagation since it was our message.
    event.stopPropagation();
    return false;
  }
  return true;
};

/**
 * Call the appropriate API function given a message from the page.
 * @param {*} message The message.
 */
cvox.ApiImplementation.dispatchApiMessage = function(message) {
  var method;
  switch (message['cmd']) {
    case 'speak': method = cvox.ApiImplementation.speak; break;
    case 'speakNodeRef': method = cvox.ApiImplementation.speakNodeRef; break;
    case 'stop': method = cvox.ApiImplementation.stop; break;
    case 'playEarcon': method = cvox.ApiImplementation.playEarcon; break;
    case 'syncToNodeRef': method = cvox.ApiImplementation.syncToNodeRef; break;
    case 'clickNodeRef': method = cvox.ApiImplementation.clickNodeRef; break;
    case 'getBuild': method = cvox.ApiImplementation.getBuild; break;
    case 'getVersion': method = cvox.ApiImplementation.getVersion; break;
    case 'getCurrentNode': method = cvox.ApiImplementation.getCurrentNode;
        break;
    case 'getCvoxModKeys': method = cvox.ApiImplementation.getCvoxModKeys;
        break;
    case 'isKeyShortcut': method = cvox.ApiImplementation.isKeyShortcut; break;
    case 'setKeyEcho': method = cvox.ApiImplementation.setKeyEcho; break;
    case 'Math.defineRule':
      method = cvox.ApiImplementation.Math.defineRule; break;
      break;
  }
  if (!method) {
    throw 'Unknown API call: ' + message['cmd'];
  }

  method.apply(cvox.ApiImplementation, message['args']);
};

/**
 * Sets endCallback in properties to call callbackId's function.
 * @param {Object} properties Speech properties to use for this utterance.
 * @param {number} callbackId The callback Id.
 * @private
 */
function setupEndCallback_(properties, callbackId) {
  var endCallback = function() {
    cvox.ApiImplementation.port.postMessage(cvox.ChromeVoxJSON.stringify(
        {
          'id': callbackId
        }));
  };
  if (properties) {
    properties['endCallback'] = endCallback;
  }
}

/**
 * Speaks the given string using the specified queueMode and properties.
 *
 * @param {number} callbackId The callback Id.
 * @param {string} textString The string of text to be spoken.
 * @param {number=} queueMode Valid modes are 0 for flush; 1 for queue.
 * @param {Object=} properties Speech properties to use for this utterance.
 */
cvox.ApiImplementation.speak = function(
    callbackId, textString, queueMode, properties) {
  if (cvox.ChromeVox.isActive) {
    if (!properties) {
      properties = {};
    }
    setupEndCallback_(properties, callbackId);
    cvox.ChromeVox.tts.speak(textString,
                             /** @type {cvox.QueueMode} */ (queueMode),
                             properties);
  }
};

/**
 * Speaks the given node.
 *
 * @param {Node} node The node that ChromeVox should be synced to.
 * @param {number=} queueMode Valid modes are 0 for flush; 1 for queue.
 * @param {Object=} properties Speech properties to use for this utterance.
 */
cvox.ApiImplementation.speakNode = function(node, queueMode, properties) {
  if (cvox.ChromeVox.isActive) {
    cvox.ChromeVox.tts.speak(
        cvox.DomUtil.getName(node),
        /** @type {cvox.QueueMode} */ (queueMode),
        properties);
  }
};

/**
 * Speaks the given node.
 *
 * @param {number} callbackId The callback Id.
 * @param {Object} nodeRef A serializable reference to a node.
 * @param {number=} queueMode Valid modes are 0 for flush; 1 for queue.
 * @param {Object=} properties Speech properties to use for this utterance.
 */
cvox.ApiImplementation.speakNodeRef = function(
    callbackId, nodeRef, queueMode, properties) {
  var node = cvox.ApiUtils.getNodeFromRef(nodeRef);
  if (!properties) {
    properties = {};
  }
  setupEndCallback_(properties, callbackId);
  cvox.ApiImplementation.speakNode(node, queueMode, properties);
};

/**
 * Stops speech.
 */
cvox.ApiImplementation.stop = function() {
  if (cvox.ChromeVox.isActive) {
    cvox.ChromeVox.tts.stop();
  }
};

/**
 * Plays the specified earcon sound.
 *
 * @param {string} earcon An earcon name.
 */
cvox.ApiImplementation.playEarcon = function(earcon) {
  if (cvox.ChromeVox.isActive && cvox.Earcon[earcon]) {
    cvox.ChromeVox.earcons.playEarcon(cvox.Earcon[earcon]);
  }
};

/**
 * Synchronizes ChromeVox's internal cursor to a node by id.
 *
 * @param {Object} nodeRef A serializable reference to a node.
 * @param {boolean=} speakNode If true, speaks out the node.
 */
cvox.ApiImplementation.syncToNodeRef = function(nodeRef, speakNode) {
  var node = cvox.ApiUtils.getNodeFromRef(nodeRef);
  cvox.ApiImplementation.syncToNode(node, speakNode);
};

/**
 * Synchronizes ChromeVox's internal cursor to the targetNode.
 * Note that this will NOT trigger reading unless given the optional argument;
 * it is for setting the internal ChromeVox cursor so that when the user resumes
 * reading, they will be starting from a reasonable position.
 *
 * @param {Node} targetNode The node that ChromeVox should be synced to.
 * @param {boolean=} opt_speakNode If true, speaks out the node.
 * @param {number=} opt_queueMode The queue mode to use for speaking.
 */
cvox.ApiImplementation.syncToNode = function(
    targetNode, opt_speakNode, opt_queueMode) {
  if (!cvox.ChromeVox.isActive) {
    return;
  }

  if (opt_queueMode == undefined) {
    opt_queueMode = cvox.QueueMode.CATEGORY_FLUSH;
  }

  cvox.ChromeVox.navigationManager.updateSelToArbitraryNode(targetNode, true);
  cvox.ChromeVox.navigationManager.updateIndicator();

  if (opt_speakNode == undefined) {
    opt_speakNode = false;
  }

  // Don't speak anything if the node is hidden or invisible.
  if (cvox.AriaUtil.isHiddenRecursive(targetNode)) {
    opt_speakNode = false;
  }

  if (opt_speakNode) {
    cvox.ChromeVox.navigationManager.speakDescriptionArray(
        cvox.ApiImplementation.getDesc_(targetNode),
        /** @type {cvox.QueueMode} */ (opt_queueMode),
        null,
        null,
        cvox.TtsCategory.NAV);
  }

  cvox.ChromeVox.braille.write(cvox.ChromeVox.navigationManager.getBraille());

  cvox.ChromeVox.navigationManager.updatePosition(targetNode);
};

/**
 * Get the current node that ChromeVox is on.
 * @param {number} callbackId The callback Id.
 */
cvox.ApiImplementation.getCurrentNode = function(callbackId) {
  var currentNode = cvox.ChromeVox.navigationManager.getCurrentNode();
  cvox.ApiImplementation.port.postMessage(cvox.ChromeVoxJSON.stringify(
      {
        'id': callbackId,
        'currentNode': cvox.ApiUtils.makeNodeReference(currentNode)
      }));
};

/**
 * Gets the predefined description set on a node by an api call, if such
 * a call was made. Otherwise returns the description that the NavigationManager
 * would speak.
 * @param {Node} node The node for which to get the description.
 * @return {Array<cvox.NavDescription>} The description array.
 * @private
 */
cvox.ApiImplementation.getDesc_ = function(node) {
  if (!node.hasAttribute('cvoxnodedesc')) {
    return cvox.ChromeVox.navigationManager.getDescription();
  }

  var preDesc = cvox.ChromeVoxJSON.parse(node.getAttribute('cvoxnodedesc'));
  var currentDesc = new Array();
  for (var i = 0; i < preDesc.length; ++i) {
    var inDesc = preDesc[i];
    // TODO: this can probably be replaced with just NavDescription(inDesc)
    // need test case to ensure this change will work
    currentDesc.push(new cvox.NavDescription({
      context: inDesc.context,
      text: inDesc.text,
      userValue: inDesc.userValue,
      annotation: inDesc.annotation
    }));
  }
  return currentDesc;
};

/**
 * Simulate a click on an element.
 *
 * @param {Object} nodeRef A serializable reference to a node.
 * @param {boolean} shiftKey Specifies if shift is held down.
 */
cvox.ApiImplementation.clickNodeRef = function(nodeRef, shiftKey) {
  cvox.DomUtil.clickElem(
      cvox.ApiUtils.getNodeFromRef(nodeRef), shiftKey, false);
};

/**
 * Get the ChromeVox build info string.
 * @param {number} callbackId The callback Id.
 */
cvox.ApiImplementation.getBuild = function(callbackId) {
  cvox.ApiImplementation.port.postMessage(cvox.ChromeVoxJSON.stringify(
      {
        'id': callbackId,
        'build': cvox.BuildInfo.build
      }));
};

/**
 * Get the ChromeVox version.
 * @param {number} callbackId The callback Id.
 */
cvox.ApiImplementation.getVersion = function(callbackId) {
  var version = cvox.ChromeVox.version;
  if (version == null) {
    window.setTimeout(function() {
      cvox.ApiImplementation.getVersion(callbackId);
    }, 1000);
    return;
  }
  cvox.ApiImplementation.port.postMessage(cvox.ChromeVoxJSON.stringify(
      {
        'id': callbackId,
        'version': version
      }));
};

/**
 * Get the ChromeVox modifier keys.
 * @param {number} callbackId The callback Id.
 */
cvox.ApiImplementation.getCvoxModKeys = function(callbackId) {
  cvox.ApiImplementation.port.postMessage(cvox.ChromeVoxJSON.stringify(
      {
        'id': callbackId,
        'keyCodes': cvox.KeyUtil.cvoxModKeyCodes()
      }));
};

/**
 * Return if the keyEvent has a key binding.
 * @param {number} callbackId The callback Id.
 * @param {Event} keyEvent A key event.
 */
cvox.ApiImplementation.isKeyShortcut = function(callbackId, keyEvent) {
  var keySeq = cvox.KeyUtil.keyEventToKeySequence(keyEvent);
  cvox.ApiImplementation.port.postMessage(cvox.ChromeVoxJSON.stringify(
      {
        'id': callbackId,
        'isHandled': cvox.ChromeVoxKbHandler.handlerKeyMap.hasKey(keySeq)
      }));
};

/**
* Set key echoing on key press.
* @param {boolean} keyEcho Whether key echoing should be on or off.
*/
cvox.ApiImplementation.setKeyEcho = function(keyEcho) {
  var msg = cvox.ChromeVox.keyEcho;
  msg[document.location.href] = keyEcho;
  cvox.ChromeVox.host.sendToBackgroundPage({
  'target': 'Prefs',
  'action': 'setPref',
  'pref': 'keyEcho',
  'value': JSON.stringify(msg)
  });
};

/**
 * @constructor
 */
cvox.ApiImplementation.Math = function() {};

/**
 * Defines a math speech rule.
 * @param {string} name Rule name.
 * @param {string} dynamic Dynamic constraint annotation. In the case of a
 *      math rule it consists of a domain.style string.
 * @param {string} action An action of rule components.
 * @param {string} prec XPath or custom function constraining match.
 * @param {...string} constraints Additional constraints.
 */
cvox.ApiImplementation.Math.defineRule =
    function(name, dynamic, action, prec, constraints) {
  var mathStore = cvox.MathmlStore.getInstance();
  var constraintList = Array.prototype.slice.call(arguments, 4);
  var args = [name, dynamic, action, prec].concat(constraintList);

  mathStore.defineRule.apply(mathStore, args);
};
