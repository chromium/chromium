// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for test related things.
 */
var test = test || {};

/**
 * Extract the information of the given element.
 * @param {Element} element Element to be extracted.
 * @param {Window} contentWindow Window to be tested.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained. NOTE: Causes element style re-calculation.
 * @return {{attributes:Object<string>, text:string,
 *           styles:(Object<string>|undefined), hidden:boolean}} Element
 *     information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values.
 */
function extractElementInfo(element, contentWindow, opt_styleNames) {
  const attributes = {};
  for (let i = 0; i < element.attributes.length; i++) {
    attributes[element.attributes[i].nodeName] =
        element.attributes[i].nodeValue;
  }

  const result = {
    attributes: attributes,
    text: element.textContent,
    value: element.value,
    // The hidden attribute is not in the element.attributes even if
    // element.hasAttribute('hidden') is true.
    hidden: !!element.hidden,
    hasShadowRoot: !!element.shadowRoot
  };

  const styleNames = opt_styleNames || [];
  assert(Array.isArray(styleNames));
  if (!styleNames.length)
    return result;

  const styles = {};
  const size = element.getBoundingClientRect();
  const computedStyles = contentWindow.getComputedStyle(element);
  for (let i = 0; i < styleNames.length; i++) {
    styles[styleNames[i]] = computedStyles[styleNames[i]];
  }

  result.styles = styles;
  // These attributes are set when element is img or canvas.
  result.imageWidth = Number(element.width);
  result.imageHeight = Number(element.height);

  // These attributes are set in any element.
  result.renderedWidth = size.width;
  result.renderedHeight = size.height;
  return result;
}

/**
 * Namespace for test utility functions.
 *
 * Public functions in the test.util.sync and the test.util.async namespaces are
 * published to test cases and can be called by using callRemoteTestUtil. The
 * arguments are serialized as JSON internally. If application ID is passed to
 * callRemoteTestUtil, the content window of the application is added as the
 * first argument. The functions in the test.util.async namespace are passed the
 * callback function as the last argument.
 */
test.util = {};

/**
 * Namespace for synchronous utility functions.
 */
test.util.sync = {};

/**
 * Namespace for asynchronous utility functions.
 */
test.util.async = {};

/**
 * List of extension ID of the testing extension.
 * @type {Array<string>}
 * @const
 */
test.util.TESTING_EXTENSION_IDS = [
  'oobinhbdbiehknkpbpejbbpdbkdjmoco',  // File Manager test extension.
  'ejhcmmdhhpdhhgmifplfmjobgegbibkn',  // Gallery test extension.
  'ljoplibgfehghmibaoaepfagnmbbfiga',  // Video Player test extension.
  'ddabbgbggambiildohfagdkliahiecfl',  // Audio Player test extension.
];

/**
 * Obtains window information.
 *
 * @return {Object<{innerWidth:number, innerHeight:number}>} Map window
 *     ID and window information.
 */
test.util.sync.getWindows = function() {
  var windows = {};
  for (var id in window.appWindows) {
    var windowWrapper = window.appWindows[id];
    windows[id] = {
      outerWidth: windowWrapper.contentWindow.outerWidth,
      outerHeight: windowWrapper.contentWindow.outerHeight
    };
  }
  for (var id in window.background.dialogs) {
    windows[id] = {
      outerWidth: window.background.dialogs[id].outerWidth,
      outerHeight: window.background.dialogs[id].outerHeight
    };
  }
  return windows;
};

/**
 * Closes the specified window.
 *
 * @param {string} appId AppId of window to be closed.
 * @return {boolean} Result: True if success, false otherwise.
 */
test.util.sync.closeWindow = function(appId) {
  if (appId in window.appWindows &&
      window.appWindows[appId].contentWindow) {
    window.appWindows[appId].close();
    return true;
  }
  return false;
};

/**
 * Gets total Javascript error count from background page and each app window.
 * @return {number} Error count.
 */
test.util.sync.getErrorCount = function() {
  var totalCount = window.JSErrorCount;
  for (var appId in window.appWindows) {
    var contentWindow = window.appWindows[appId].contentWindow;
    if (contentWindow.JSErrorCount)
      totalCount += contentWindow.JSErrorCount;
  }
  return totalCount;
};

/**
 * Resizes the window to the specified dimensions.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {number} width Window width.
 * @param {number} height Window height.
 * @return {boolean} True for success.
 */
test.util.sync.resizeWindow = function(contentWindow, width, height) {
  window.appWindows[contentWindow.appID].resizeTo(width, height);
  return true;
};

/**
 * Maximizes the window.
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True for success.
 */
test.util.sync.maximizeWindow = function(contentWindow) {
  window.appWindows[contentWindow.appID].maximize();
  return true;
};

/**
 * Restores the window state (maximized/minimized/etc...).
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True for success.
 */
test.util.sync.restoreWindow = function(contentWindow) {
  window.appWindows[contentWindow.appID].restore();
  return true;
};

/**
 * Returns whether the window is miximized or not.
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True if the window is maximized now.
 */
test.util.sync.isWindowMaximized = function(contentWindow) {
  return window.appWindows[contentWindow.appID].isMaximized();
};

/**
 * Queries all elements.
 *
 * @param {!Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {!Array<{attributes:Object<string>, text:string,
 *                  styles:Object<string>, hidden:boolean}>} Element
 *     information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values.
 */
test.util.sync.queryAllElements = function(
    contentWindow, targetQuery, opt_styleNames) {
  return test.util.sync.deepQueryAllElements(
      contentWindow, [targetQuery], opt_styleNames);
};

/**
 * Queries elements inside shadow DOM.
 *
 * @param {!Window} contentWindow Window to be tested.
 * @param {!Array<string>} targetQuery Query to specify the element.
 *   |targetQuery[0]| specifies the first element(s). |targetQuery[1]| specifies
 *   elements inside the shadow DOM of the first element, and so on.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {!Array<{attributes:Object<string>, text:string,
 *                  styles:Object<string>, hidden:boolean}>} Element
 *     information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values.
 */
test.util.sync.deepQueryAllElements = function(
    contentWindow, targetQuery, opt_styleNames) {
  if (!contentWindow.document)
    return [];

  var elems =
      test.util.sync.deepQuerySelectorAll_(contentWindow.document, targetQuery);
  return elems.map(function(element) {
    return extractElementInfo(element, contentWindow, opt_styleNames);
  });
};

/**
 * Selects elements below |root|, possibly following shadow DOM subtree.
 *
 * @param {(!HTMLElement|!Document)} root Element to search from.
 * @param {!Array<string>} targetQuery Query to specify the element.
 *   |targetQuery[0]| specifies the first element(s). |targetQuery[1]| specifies
 *   elements inside the shadow DOM of the first element, and so on.
 * @return {!Array<!HTMLElement>} Matched elements.
 *
 * @private
 */
test.util.sync.deepQuerySelectorAll_ = function(root, targetQuery) {
  var elems = Array.prototype.slice.call(root.querySelectorAll(targetQuery[0]));
  var remaining = targetQuery.slice(1);
  if (remaining.length === 0)
    return elems;

  var res = [];
  for (var i = 0; i < elems.length; i++) {
    if (elems[i].shadowRoot) {
      res = res.concat(
          test.util.sync.deepQuerySelectorAll_(elems[i].shadowRoot, remaining));
    }
  }
  return res;
};

/**
 * Executes a script in the context of the first <webview> element contained in
 * the window, including shadow DOM subtrees if given, and returns the script
 * result via the callback.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {!Array<string>} targetQuery Query for the <webview> element.
 *   |targetQuery[0]| specifies the first element. |targetQuery[1]| specifies
 *   an element inside the shadow DOM of the first element, etc. The last
 *   targetQuery item must return the <webview> element.
 * @param {string} script Javascript code to be executed within the <webview>.
 * @param {function(*)} callback Callback function to be called with the
 *   result of the |script|.
 */
test.util.async.deepExecuteScriptInWebView = function(
    contentWindow, targetQuery, script, callback) {
  const webviews =
      test.util.sync.deepQuerySelectorAll_(contentWindow.document, targetQuery);
  if (!webviews || webviews.length !== 1)
    throw new Error('<webview> not found: [' + targetQuery.join(',') + ']');
  const webview = /** @type {WebView} */ (webviews[0]);
  webview.executeScript({code: script}, callback);
};

/**
 * Gets the information of the active element.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {?{attributes:Object<string>, text:string,
 *                  styles:(Object<string>|undefined), hidden:boolean}} Element
 *     information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values. If there is no
 *     active element, returns null.
 */
test.util.sync.getActiveElement = function(contentWindow, opt_styleNames) {
  if (!contentWindow.document || !contentWindow.document.activeElement)
    return null;

  return extractElementInfo(
      contentWindow.document.activeElement, contentWindow, opt_styleNames);
};

/**
 * Assigns the text to the input element.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query for the input element.
 * @param {string} text Text to be assigned.
 */
test.util.sync.inputText = function(contentWindow, query, text) {
  var input = contentWindow.document.querySelector(query);
  input.value = text;
};

/**
 * Sends an event to the element specified by |targetQuery| or active element.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {?string|Array<string>} targetQuery Query to specify the element.
 *     If this value is null, an event is dispatched to active element of the
 *     document.
 *     If targetQuery is an array, |targetQuery[0]| specifies the first
 *     element(s), |targetQuery[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param {!Event} event Event to be sent.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.sendEvent = function(contentWindow, targetQuery, event) {
  if (!contentWindow.document)
    return false;

  let target;
  if (targetQuery === null) {
    target = contentWindow.document.activeElement;
  } else if (typeof targetQuery === 'string') {
    target = contentWindow.document.querySelector(targetQuery);
  } else if (Array.isArray(targetQuery)) {
    let elems = test.util.sync.deepQuerySelectorAll_(
        contentWindow.document, targetQuery);
    if (elems.length > 0)
      target = elems[0];
  }

  if (!target)
    return false;

  target.dispatchEvent(event);
  return true;
};

/**
 * Sends an fake event having the specified type to the target query.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string} eventType Type of event.
 * @param {Object=} opt_additionalProperties Object contaning additional
 *     properties.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeEvent = function(contentWindow,
                                    targetQuery,
                                    eventType,
                                    opt_additionalProperties) {
  var event = new Event(eventType,
      /** @type {!EventInit} */ (opt_additionalProperties || {}));
  if (opt_additionalProperties) {
    for (var name in opt_additionalProperties) {
      event[name] = opt_additionalProperties[name];
    }
  }
  return test.util.sync.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Sends a fake key event to the element specified by |targetQuery| or active
 * element with the given |key| and optional |ctrl,shift,alt| modifier.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {?string} targetQuery Query to specify the element. If this value is
 *     null, key event is dispatched to active element of the document.
 * @param {string} key DOM UI Events key value.
 * @param {boolean} ctrl Whether CTRL should be pressed, or not.
 * @param {boolean} shift whether SHIFT should be pressed, or not.
 * @param {boolean} alt whether ALT should be pressed, or not.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeKeyDown = function(
    contentWindow, targetQuery, key, ctrl, shift, alt) {
  const event = new KeyboardEvent('keydown', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
    key: key,
    ctrlKey: ctrl,
    shiftKey: shift,
    altKey: alt
  });
  return test.util.sync.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Simulates a fake mouse click (left button, single click) on the element
 * specified by |targetQuery|. If the element has the click method, just calls
 * it. Otherwise, this sends 'mouseover', 'mousedown', 'mouseup' and 'click'
 * events in turns.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string|Array<string>} targetQuery Query to specify the element.
 *     If targetQuery is an array, |targetQuery[0]| specifies the first
 *     element(s), |targetQuery[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @return {boolean} True if the all events are sent to the target, false
 *     otherwise.
 */
test.util.sync.fakeMouseClick = function(contentWindow, targetQuery) {
  const props = {
    bubbles: true,
    detail: 1,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  };
  const mouseOverEvent = new MouseEvent('mouseover', props);
  const resultMouseOver =
      test.util.sync.sendEvent(contentWindow, targetQuery, mouseOverEvent);
  const mouseDownEvent = new MouseEvent('mousedown', props);
  const resultMouseDown =
      test.util.sync.sendEvent(contentWindow, targetQuery, mouseDownEvent);
  const mouseUpEvent = new MouseEvent('mouseup', props);
  const resultMouseUp =
      test.util.sync.sendEvent(contentWindow, targetQuery, mouseUpEvent);
  const clickEvent = new MouseEvent('click', props);
  const resultClick =
      test.util.sync.sendEvent(contentWindow, targetQuery, clickEvent);
  return resultMouseOver && resultMouseDown && resultMouseUp && resultClick;
};

/**
 * Simulates a fake mouse click (right button, single click) on the element
 * specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false
 *     otherwise.
 */
test.util.sync.fakeMouseRightClick = function(contentWindow, targetQuery) {
  const mouseDownEvent =
      new MouseEvent('mousedown', {bubbles: true, button: 2, composed: true});
  if (!test.util.sync.sendEvent(contentWindow, targetQuery, mouseDownEvent)) {
    return false;
  }

  const contextMenuEvent =
      new MouseEvent('contextmenu', {bubbles: true, composed: true});
  return test.util.sync.sendEvent(contentWindow, targetQuery, contextMenuEvent);
};

/**
 * Simulates a fake touch event (touch start, touch end) on the element
 * specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false
 *     otherwise.
 */
test.util.sync.fakeTouchClick = function(contentWindow, targetQuery) {
  const touchStartEvent = new TouchEvent('touchstart');
  if (!test.util.sync.sendEvent(contentWindow, targetQuery, touchStartEvent)) {
    return false;
  }

  const mouseDownEvent =
      new MouseEvent('mousedown', {bubbles: true, button: 2, composed: true});
  if (!test.util.sync.sendEvent(contentWindow, targetQuery, mouseDownEvent)) {
    return false;
  }

  const touchEndEvent = new TouchEvent('touchend');
  if (!test.util.sync.sendEvent(contentWindow, targetQuery, touchEndEvent)) {
    return false;
  }

  const contextMenuEvent =
      new MouseEvent('contextmenu', {bubbles: true, composed: true});
  return test.util.sync.sendEvent(contentWindow, targetQuery, contextMenuEvent);
};

/**
 * Simulates a fake double click event (left button) to the element specified by
 * |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseDoubleClick = function(contentWindow, targetQuery) {
  // Double click is always preceded with a single click.
  if (!test.util.sync.fakeMouseClick(contentWindow, targetQuery)) {
    return false;
  }

  // Send the second click event, but with detail equal to 2 (number of clicks)
  // in a row.
  let event =
      new MouseEvent('click', {bubbles: true, detail: 2, composed: true});
  if (!test.util.sync.sendEvent(contentWindow, targetQuery, event)) {
    return false;
  }

  // Send the double click event.
  event = new MouseEvent('dblclick', {bubbles: true, composed: true});
  if (!test.util.sync.sendEvent(contentWindow, targetQuery, event)) {
    return false;
  }

  return true;
};

/**
 * Sends a fake mouse down event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseDown = function(contentWindow, targetQuery) {
  const event = new MouseEvent('mousedown', {bubbles: true, composed: true});
  return test.util.sync.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Sends a fake mouse up event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseUp = function(contentWindow, targetQuery) {
  const event = new MouseEvent('mouseup', {bubbles: true, composed: true});
  return test.util.sync.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Focuses to the element specified by |targetQuery|. This method does not
 * provide any guarantee whether the element is actually focused or not.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if focus method of the element has been called, false
 *     otherwise.
 */
test.util.sync.focus = function(contentWindow, targetQuery) {
  var target = contentWindow.document &&
      contentWindow.document.querySelector(targetQuery);

  if (!target)
    return false;

  target.focus();
  return true;
};

/**
 * Obtains the list of notification ID.
 * @param {function(Object<boolean>)} callback Callback function with
 *     results returned by the script.
 */
test.util.async.getNotificationIDs = function(callback) {
  chrome.notifications.getAll(callback);
};

/**
 * Opens the file URL. It emulates the interaction that Launcher search does
 * from a search result, it triggers the background page's event listener that
 * listens to evens from launcher_search_provider API.
 *
 * @param {string} fileURL File URL to open by Files app background dialog.
 * @suppress {accessControls|missingProperties} Closure disallow calling private
 * launcherSearch_, but here we just want to emulate the behaviour, so we don't
 * need to make this attribute public. Also the interface
 * "FileBrowserBackground" doesn't define the attributes "launcherSearch_" so we
 * need to suppress missingProperties.
 */
test.util.sync.launcherSearchOpenResult = function(fileURL) {
  window.background.launcherSearch_.onOpenResult_(fileURL);
};

/**
 * Gets file entries just under the volume.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<string>} names File name list.
 * @param {function(*)} callback Callback function with results returned by the
 *     script.
 */
test.util.async.getFilesUnderVolume = function(volumeType, names, callback) {
  var displayRootPromise =
      volumeManagerFactory.getInstance().then(function(volumeManager) {
    var volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
    return volumeInfo.resolveDisplayRoot();
  });

  var retrievePromise = displayRootPromise.then(function(displayRoot) {
    var filesPromise = names.map(function(name) {
      return new Promise(
          displayRoot.getFile.bind(displayRoot, name, {}));
    });
    return Promise.all(filesPromise).then(function(aa) {
      return util.entriesToURLs(aa);
    }).catch(function() {
      return [];
    });
  });

  retrievePromise.then(callback);
};

/**
 * Unmounts the specified volume.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {function(boolean)} callback Function receives true on success.
 */
test.util.async.unmount = function(volumeType, callback) {
  volumeManagerFactory.getInstance().then((volumeManager) => {
    const volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
    if (volumeInfo) {
      volumeManager.unmount(
          volumeInfo, callback.bind(null, true), callback.bind(null, false));
    }
  });
};

/**
 * Registers message listener, which runs test utility functions.
 */
test.util.registerRemoteTestUtils = function() {
  // Return true for asynchronous functions and false for synchronous.
  chrome.runtime.onMessageExternal.addListener(
      function(request, sender, sendResponse) {
    // Check the sender.
    if (!sender.id ||
        test.util.TESTING_EXTENSION_IDS.indexOf(sender.id) === -1) {
      // Silently return.  Don't return false; that short-circuits the
      // propagation of messages, and there are now other listeners that want to
      // handle external messages.
      return;
    }
    // Set a global flag that we are in tests, so other components are aware
    // of it.
    window.IN_TEST = true;
    // Check the function name.
    if (!request.func || request.func[request.func.length - 1] == '_') {
      request.func = '';
    }
    // Prepare arguments.
    if (!('args' in request))
      throw new Error('Invalid request.');
    var args = request.args.slice();  // shallow copy
    if (request.appId) {
      if (window.appWindows[request.appId]) {
        args.unshift(window.appWindows[request.appId].contentWindow);
      } else if (window.background.dialogs[request.appId]) {
        args.unshift(window.background.dialogs[request.appId]);
      } else {
        console.error('Specified window not found: ' + request.appId);
        return false;
      }
    }
    // Call the test utility function and respond the result.
    if (test.util.async[request.func]) {
      args[test.util.async[request.func].length - 1] = function() {
        console.debug('Received the result of ' + request.func);
        sendResponse.apply(null, arguments);
      };
      console.debug('Waiting for the result of ' + request.func);
      test.util.async[request.func].apply(null, args);
      return true;
    } else if (test.util.sync[request.func]) {
      sendResponse(test.util.sync[request.func].apply(null, args));
      return false;
    } else {
      console.error('Invalid function name.');
      return false;
    }
  });
};

/**
 * Returns the MetadataStats collected in MetadataModel, it will be serialized
 * as a plain object when sending to test extension.
 *
 * @suppress {missingProperties} metadataStats is only defined for foreground
 *   Window so it isn't visible in the background. Here it will return as JSON
 *   object to test extension.
 */
test.util.sync.getMetadataStats = function(contentWindow) {
  return contentWindow.fileManager.metadataModel.getStats();
};

/**
 * Returns true when FileManager has finished loading, by checking the attribute
 * "loaded" on its root element.
 */
test.util.sync.isFileManagerLoaded = function(contentWindow) {
  if (contentWindow && contentWindow.fileManager &&
      contentWindow.fileManager.ui)
    return contentWindow.fileManager.ui.element.hasAttribute('loaded');

  return false;
};

/**
 * Reports to the given |callback| the number of volumes available in
 * VolumeManager in the background page.
 *
 * @param {function(number)} callback Callback function to be called with the
 *   number of volumes.
 */
test.util.async.getVolumesCount = function(callback) {
  return volumeManagerFactory.getInstance().then((volumeManager) => {
    callback(volumeManager.volumeInfoList.length);
  });
};
