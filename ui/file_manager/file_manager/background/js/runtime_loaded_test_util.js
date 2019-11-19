// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script loaded into the background page of a component
 * extension under test at runtime to populate testing functionality.
 */

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
  if (!styleNames.length) {
    return result;
  }

  const styles = {};
  const size = element.getBoundingClientRect();
  const computedStyles = contentWindow.getComputedStyle(element);
  for (let i = 0; i < styleNames.length; i++) {
    styles[styleNames[i]] = computedStyles[styleNames[i]];
  }

  result.styles = styles;

  // These attributes are set when element is <img> or <canvas>.
  result.imageWidth = Number(element.width);
  result.imageHeight = Number(element.height);

  // These attributes are set in any element.
  result.renderedWidth = size.width;
  result.renderedHeight = size.height;
  result.renderedTop = size.top;
  result.renderedLeft = size.left;

  // Get the scroll position of the element.
  result.scrollLeft = element.scrollLeft;
  result.scrollTop = element.scrollTop;

  return result;
}

/**
 * Obtains window information.
 *
 * @return {Object<{innerWidth:number, innerHeight:number}>} Map window
 *     ID and window information.
 */
test.util.sync.getWindows = () => {
  const windows = {};
  for (var id in window.appWindows) {
    const windowWrapper = window.appWindows[id];
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
test.util.sync.closeWindow = appId => {
  if (appId in window.appWindows && window.appWindows[appId].contentWindow) {
    window.appWindows[appId].close();
    return true;
  }
  return false;
};

/**
 * Gets total Javascript error count from background page and each app window.
 * @return {number} Error count.
 */
test.util.sync.getErrorCount = () => {
  let totalCount = window.JSErrorCount;
  for (const appId in window.appWindows) {
    const contentWindow = window.appWindows[appId].contentWindow;
    if (contentWindow.JSErrorCount) {
      totalCount += contentWindow.JSErrorCount;
    }
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
test.util.sync.resizeWindow = (contentWindow, width, height) => {
  window.appWindows[contentWindow.appID].resizeTo(width, height);
  return true;
};

/**
 * Maximizes the window.
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True for success.
 */
test.util.sync.maximizeWindow = contentWindow => {
  window.appWindows[contentWindow.appID].maximize();
  return true;
};

/**
 * Restores the window state (maximized/minimized/etc...).
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True for success.
 */
test.util.sync.restoreWindow = contentWindow => {
  window.appWindows[contentWindow.appID].restore();
  return true;
};

/**
 * Returns whether the window is miximized or not.
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True if the window is maximized now.
 */
test.util.sync.isWindowMaximized = contentWindow => {
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
test.util.sync.queryAllElements =
    (contentWindow, targetQuery, opt_styleNames) => {
      return test.util.sync.deepQueryAllElements(
          contentWindow, targetQuery, opt_styleNames);
    };

/**
 * Queries elements inside shadow DOM.
 *
 * @param {!Window} contentWindow Window to be tested.
 * @param {string|!Array<string>} targetQuery Query to specify the element.
 *   |targetQuery[0]| specifies the first element(s). |targetQuery[1]| specifies
 *   elements inside the shadow DOM of the first element, and so on.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {!Array<{attributes:Object<string>, text:string,
 *                  styles:Object<string>, hidden:boolean}>} Element
 *     information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values.
 */
test.util.sync.deepQueryAllElements =
    (contentWindow, targetQuery, opt_styleNames) => {
      if (!contentWindow.document) {
        return [];
      }
      if (typeof targetQuery === 'string') {
        targetQuery = [targetQuery];
      }

      const elems = test.util.sync.deepQuerySelectorAll_(
          contentWindow.document, targetQuery);
      return elems.map(element => {
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
test.util.sync.deepQuerySelectorAll_ = (root, targetQuery) => {
  const elems =
      Array.prototype.slice.call(root.querySelectorAll(targetQuery[0]));
  const remaining = targetQuery.slice(1);
  if (remaining.length === 0) {
    return elems;
  }

  let res = [];
  for (let i = 0; i < elems.length; i++) {
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
test.util.async.deepExecuteScriptInWebView =
    (contentWindow, targetQuery, script, callback) => {
      const webviews = test.util.sync.deepQuerySelectorAll_(
          contentWindow.document, targetQuery);
      if (!webviews || webviews.length !== 1) {
        throw new Error('<webview> not found: [' + targetQuery.join(',') + ']');
      }
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
test.util.sync.getActiveElement = (contentWindow, opt_styleNames) => {
  if (!contentWindow.document || !contentWindow.document.activeElement) {
    return null;
  }

  return extractElementInfo(
      contentWindow.document.activeElement, contentWindow, opt_styleNames);
};

/**
 * Assigns the text to the input element.
 * @param {Window} contentWindow Window to be tested.
 * @param {string|!Array<string>} query Query for the input element.
 *     If |query| is an array, |query[0]| specifies the first element(s),
 *     |query[1]| specifies elements inside the shadow DOM of the first element,
 *     and so on.
 * @param {string} text Text to be assigned.
 */
test.util.sync.inputText = (contentWindow, query, text) => {
  if (typeof query === 'string') {
    query = [query];
  }

  const elems =
      test.util.sync.deepQuerySelectorAll_(contentWindow.document, query);
  if (elems.length === 0) {
    console.error(`Input element not found: [${query.join(',')}]`);
    return;
  }

  const input = elems[0];
  input.value = text;
  input.dispatchEvent(new Event('change'));
};

/**
 * Sets the left scroll position of an element.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query for the test element.
 * @param {number} position scrollLeft position to set.
 */
test.util.sync.setScrollLeft = (contentWindow, query, position) => {
  const scrollablElement = contentWindow.document.querySelector(query);
  scrollablElement.scrollLeft = position;
};

/**
 * Sets the top scroll position of an element.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query for the test element.
 * @param {number} position scrollTop position to set.
 */
test.util.sync.setScrollTop = (contentWindow, query, position) => {
  const scrollablElement = contentWindow.document.querySelector(query);
  scrollablElement.scrollTop = position;
};

/**
 * Sets style properties for an element using the CSS OM.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query for the test element.
 * @param {!Object<?, string>} properties CSS Property name/values to set.
 */
test.util.sync.setElementStyles = (contentWindow, query, properties) => {
  const element = contentWindow.document.querySelector(query);
  for (let [prop, value] of Object.entries(properties)) {
    element.style[prop] = value;
  }
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
test.util.sync.sendEvent = (contentWindow, targetQuery, event) => {
  if (!contentWindow.document) {
    return false;
  }

  let target;
  if (targetQuery === null) {
    target = contentWindow.document.activeElement;
  } else if (typeof targetQuery === 'string') {
    target = contentWindow.document.querySelector(targetQuery);
  } else if (Array.isArray(targetQuery)) {
    let elems = test.util.sync.deepQuerySelectorAll_(
        contentWindow.document, targetQuery);
    if (elems.length > 0) {
      target = elems[0];
    }
  }

  if (!target) {
    return false;
  }

  target.dispatchEvent(event);
  return true;
};

/**
 * Sends an fake event having the specified type to the target query.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string} eventType Type of event.
 * @param {Object=} opt_additionalProperties Object containing additional
 *     properties.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeEvent =
    (contentWindow, targetQuery, eventType, opt_additionalProperties) => {
      const event = new Event(
          eventType,
          /** @type {!EventInit} */ (opt_additionalProperties || {}));
      if (opt_additionalProperties) {
        for (const name in opt_additionalProperties) {
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
test.util.sync.fakeKeyDown =
    (contentWindow, targetQuery, key, ctrl, shift, alt) => {
      const event = new KeyboardEvent('keydown', {
        bubbles: true,
        composed: true,  // Allow the event to bubble past shadow DOM root.
        key: key,
        ctrlKey: ctrl,
        shiftKey: shift,
        altKey: alt,
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
 * @param {{shift: boolean, alt: boolean, ctrl: boolean}=} opt_keyModifiers
 *     Object containing common key modifiers : shift, alt, and ctrl.
 * @param {number=} opt_button Mouse button number as per spec, e.g.: 2 for
 *     right-click.
 * @param {Object=} opt_eventProperties Additional properties to pass to each
 *     event, e.g.: clientX and clientY. right-click.
 * @return {boolean} True if the all events are sent to the target, false
 *     otherwise.
 */
test.util.sync.fakeMouseClick =
    (contentWindow, targetQuery, opt_keyModifiers, opt_button,
     opt_eventProperties) => {
      const modifiers = opt_keyModifiers || {};
      const eventProperties = opt_eventProperties || {};

      const props = Object.assign(
          {
            bubbles: true,
            detail: 1,
            composed: true,  // Allow the event to bubble past shadow DOM root.
            ctrlKey: modifiers.ctrl,
            shiftKey: modifiers.shift,
            altKey: modifiers.alt,
          },
          eventProperties);
      if (opt_button !== undefined) {
        props.button = opt_button;
      }

      if (!targetQuery) {
        return false;
      }
      if (typeof targetQuery === 'string') {
        targetQuery = [targetQuery];
      }
      const elems = test.util.sync.deepQuerySelectorAll_(
          contentWindow.document, /** @type !Array<string> */ (targetQuery));
      if (elems.length === 0) {
        return false;
      }
      // Only sends the event to the first matched element.
      const target = elems[0];

      const mouseOverEvent = new MouseEvent('mouseover', props);
      const resultMouseOver = target.dispatchEvent(mouseOverEvent);
      const mouseDownEvent = new MouseEvent('mousedown', props);
      const resultMouseDown = target.dispatchEvent(mouseDownEvent);
      const mouseUpEvent = new MouseEvent('mouseup', props);
      const resultMouseUp = target.dispatchEvent(mouseUpEvent);
      const clickEvent = new MouseEvent('click', props);
      const resultClick = target.dispatchEvent(clickEvent);
      return resultMouseOver && resultMouseDown && resultMouseUp && resultClick;
    };

/**
 * Simulates a mouse hover on an element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string|Array<string>} targetQuery Query to specify the element.
 *     If targetQuery is an array, |targetQuery[0]| specifies the first
 *     element(s), |targetQuery[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param {{shift: boolean, alt: boolean, ctrl: boolean}=} opt_keyModifiers
 *     Object containing common key modifiers : shift, alt, and ctrl.
 * @return {boolean} True if the event was sent to the target, false otherwise.
 */
test.util.sync.fakeMouseOver =
    (contentWindow, targetQuery, opt_keyModifiers) => {
      const modifiers = opt_keyModifiers || {};
      const props = {
        bubbles: true,
        detail: 1,
        composed: true,  // Allow the event to bubble past shadow DOM root.
        ctrlKey: modifiers.ctrl,
        shiftKey: modifiers.shift,
        altKey: modifiers.alt,
      };
      const mouseOverEvent = new MouseEvent('mouseover', props);
      return test.util.sync.sendEvent(
          contentWindow, targetQuery, mouseOverEvent);
    };

/**
 * Simulates a mouseout event on an element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string|Array<string>} targetQuery Query to specify the element.
 *     If targetQuery is an array, |targetQuery[0]| specifies the first
 *     element(s), |targetQuery[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param {{shift: boolean, alt: boolean, ctrl: boolean}=} opt_keyModifiers
 *     Object containing common key modifiers : shift, alt, and ctrl.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseOut =
    (contentWindow, targetQuery, opt_keyModifiers) => {
      const modifiers = opt_keyModifiers || {};
      const props = {
        bubbles: true,
        detail: 1,
        composed: true,  // Allow the event to bubble past shadow DOM root.
        ctrlKey: modifiers.ctrl,
        shiftKey: modifiers.shift,
        altKey: modifiers.alt,
      };
      const mouseOutEvent = new MouseEvent('mouseout', props);
      return test.util.sync.sendEvent(
          contentWindow, targetQuery, mouseOutEvent);
    };

/**
 * Simulates a fake full mouse right-click  on the element specified by
 * |targetQuery|.
 *
 * It generates the sequence of the following MouseEvents:
 * 1. mouseover
 * 2. mousedown
 * 3. mouseup
 * 4. click
 * 5. contextmenu
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {{shift: boolean, alt: boolean, ctrl: boolean}=} opt_keyModifiers
 *     Object containing common key modifiers : shift, alt, and ctrl.
 * @return {boolean} True if the event is sent to the target, false
 *     otherwise.
 */
test.util.sync.fakeMouseRightClick =
    (contentWindow, targetQuery, opt_keyModifiers) => {
      const clickResult = test.util.sync.fakeMouseClick(
          contentWindow, targetQuery, opt_keyModifiers, 2 /* right button */);
      if (!clickResult) {
        return false;
      }

      const contextMenuEvent =
          new MouseEvent('contextmenu', {bubbles: true, composed: true});
      return test.util.sync.sendEvent(
          contentWindow, targetQuery, contextMenuEvent);
    };

/**
 * Simulates a fake touch event (touch start and touch end) on the element
 * specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false
 *     otherwise.
 */
test.util.sync.fakeTouchClick = (contentWindow, targetQuery) => {
  const touchStartEvent = new TouchEvent('touchstart');
  if (!test.util.sync.sendEvent(contentWindow, targetQuery, touchStartEvent)) {
    return false;
  }

  const touchEndEvent = new TouchEvent('touchend');
  if (!test.util.sync.sendEvent(contentWindow, targetQuery, touchEndEvent)) {
    return false;
  }

  return true;
};

/**
 * Simulates a fake double click event (left button) to the element specified by
 * |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseDoubleClick = (contentWindow, targetQuery) => {
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
test.util.sync.fakeMouseDown = (contentWindow, targetQuery) => {
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
test.util.sync.fakeMouseUp = (contentWindow, targetQuery) => {
  const event = new MouseEvent('mouseup', {bubbles: true, composed: true});
  return test.util.sync.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Simulates a mouse right-click on the element specified by |targetQuery|.
 * Optionally pass X,Y coordinates to be able to choose where the right-click
 * should occur.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {number=} opt_offsetBottom offset pixels applied to target element
 *     bottom, can be negative to move above the bottom.
 * @param {number=} opt_offsetRight offset pixels applied to target element
 *     right can be negative to move inside the element.
 * @return {boolean} True if the all events are sent to the target, false
 *     otherwise.
 */
test.util.sync.rightClickOffset =
    (contentWindow, targetQuery, opt_offsetBottom, opt_offsetRight) => {
      const target = contentWindow.document &&
          contentWindow.document.querySelector(targetQuery);
      if (!target) {
        return false;
      }

      // Calculate the offsets.
      const targetRect = target.getBoundingClientRect();
      const props = {
        clientX: targetRect.right + (opt_offsetRight ? opt_offsetRight : 0),
        clientY: targetRect.bottom + (opt_offsetBottom ? opt_offsetBottom : 0),
      };

      const keyModifiers = undefined;
      const rightButton = 2;
      if (!test.util.sync.fakeMouseClick(
              contentWindow, targetQuery, keyModifiers, rightButton, props)) {
        return false;
      }

      return true;
    };

/**
 * Sends a drag'n'drop set of events from |srcTarget| to |dstTarget|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} srcTarget Query to specify the element as the source to be
 *   dragged.
 * @param {string} dstTarget Query to specify the element as the destination
 *   to drop.
 * @param {boolean=} skipDrop True if it should only hover over dstTarget.
 *   to drop.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeDragAndDrop =
    (contentWindow, srcTarget, dstTarget, skipDrop) => {
      const options = {
        bubbles: true,
        composed: true,
        dataTransfer: new DataTransfer(),
      };
      const srcElement = contentWindow.document &&
          contentWindow.document.querySelector(srcTarget);
      const dstElement = contentWindow.document &&
          contentWindow.document.querySelector(dstTarget);

      if (!srcElement || !dstElement) {
        return false;
      }

      // Get the middle of the src element, because some of Files app logic
      // requires clientX and clientY.
      const srcRect = srcElement.getBoundingClientRect();
      const srcOptions = Object.assign(
          {
            clientX: srcRect.left + (srcRect.width / 2),
            clientY: srcRect.top + (srcRect.height / 2),
          },
          options);

      const dragStart = new DragEvent('dragstart', srcOptions);
      const dragEnter = new DragEvent('dragenter', options);
      const dragOver = new DragEvent('dragover', options);
      const drop = new DragEvent('drop', options);
      const dragEnd = new DragEvent('dragEnd', options);

      srcElement.dispatchEvent(dragStart);
      dstElement.dispatchEvent(dragEnter);
      dstElement.dispatchEvent(dragOver);
      if (!skipDrop) {
        dstElement.dispatchEvent(drop);
      }
      srcElement.dispatchEvent(dragEnd);
      return true;
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
test.util.sync.focus = (contentWindow, targetQuery) => {
  const target = contentWindow.document &&
      contentWindow.document.querySelector(targetQuery);

  if (!target) {
    return false;
  }

  target.focus();
  return true;
};

/**
 * Obtains the list of notification ID.
 * @param {function(Object<boolean>)} callback Callback function with
 *     results returned by the script.
 */
test.util.async.getNotificationIDs = callback => {
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
test.util.sync.launcherSearchOpenResult = fileURL => {
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
test.util.async.getFilesUnderVolume = (volumeType, names, callback) => {
  const displayRootPromise =
      volumeManagerFactory.getInstance().then(volumeManager => {
        const volumeInfo =
            volumeManager.getCurrentProfileVolumeInfo(volumeType);
        return volumeInfo.resolveDisplayRoot();
      });

  const retrievePromise = displayRootPromise.then(displayRoot => {
    const filesPromise = names.map(name => {
      // TODO(crbug.com/880130): Remove this conditional.
      if (volumeType === VolumeManagerCommon.VolumeType.DOWNLOADS) {
        name = 'Downloads/' + name;
      }
      return new Promise(displayRoot.getFile.bind(displayRoot, name, {}));
    });
    return Promise.all(filesPromise)
        .then(aa => {
          return util.entriesToURLs(aa);
        })
        .catch(() => {
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
test.util.async.unmount = (volumeType, callback) => {
  volumeManagerFactory.getInstance().then((volumeManager) => {
    const volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
    if (volumeInfo) {
      volumeManager.unmount(
          volumeInfo, callback.bind(null, true), callback.bind(null, false));
    }
  });
};

/**
 * Remote call API handler. When loaded, this replaces the declaration in
 * test_util_base.js.
 * @param {*} request
 * @param {function(*):void} sendResponse
 */
test.util.executeTestMessage = (request, sendResponse) => {
  window.IN_TEST = true;
  // Check the function name.
  if (!request.func || request.func[request.func.length - 1] == '_') {
    request.func = '';
  }
  // Prepare arguments.
  if (!('args' in request)) {
    throw new Error('Invalid request.');
  }

  const args = request.args.slice();  // shallow copy
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
    args[test.util.async[request.func].length - 1] = function(...innerArgs) {
      console.debug('Received the result of ' + request.func);
      sendResponse.apply(null, innerArgs);
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
};

/**
 * Returns the MetadataStats collected in MetadataModel, it will be serialized
 * as a plain object when sending to test extension.
 *
 * @suppress {missingProperties} metadataStats is only defined for foreground
 *   Window so it isn't visible in the background. Here it will return as JSON
 *   object to test extension.
 */
test.util.sync.getMetadataStats = contentWindow => {
  return contentWindow.fileManager.metadataModel.getStats();
};

/**
 * Calls the metadata model to get the selected file entries in the file
 * list and try to get their metadata properties.
 *
 * @param {Array<String>} properties Content metadata properties to get.
 * @param {function(*)} callback Callback with metadata results returned.
 * @suppress {missingProperties} getContentMetadata isn't visible in the
 * background window.
 */
test.util.async.getContentMetadata = (contentWindow, properties, callback) => {
  const entries =
      contentWindow.fileManager.directoryModel.getSelectedEntries_();

  assert(entries.length > 0);
  const metaPromise =
      contentWindow.fileManager.metadataModel.get(entries, properties);
  // Wait for the promise to resolve
  metaPromise.then(resultsList => {
    callback(resultsList);
  });
};

/**
 * Returns true when FileManager has finished loading, by checking the attribute
 * "loaded" on its root element.
 */
test.util.sync.isFileManagerLoaded = contentWindow => {
  if (contentWindow && contentWindow.fileManager &&
      contentWindow.fileManager.ui) {
    return contentWindow.fileManager.ui.element.hasAttribute('loaded');
  }

  return false;
};

/**
 * Returns all a11y messages announced by |FileManagerUI.speakA11yMessage|.
 *
 * @return {Array<string>}
 */
test.util.sync.getA11yAnnounces = contentWindow => {
  if (contentWindow && contentWindow.fileManager &&
      contentWindow.fileManager.ui) {
    return contentWindow.fileManager.ui.a11yAnnounces;
  }

  return null;
};

/**
 * Reports to the given |callback| the number of volumes available in
 * VolumeManager in the background page.
 *
 * @param {function(number)} callback Callback function to be called with the
 *   number of volumes.
 */
test.util.async.getVolumesCount = callback => {
  return volumeManagerFactory.getInstance().then((volumeManager) => {
    callback(volumeManager.volumeInfoList.length);
  });
};

/**
 * Updates the preferences.
 * @param {chrome.fileManagerPrivate.PreferencesChange} preferences Preferences
 *     to set.
 */
test.util.sync.setPreferences = preferences => {
  chrome.fileManagerPrivate.setPreferences(preferences);
};

/**
 * Reports an enum metric.
 * @param {string} name The metric name.
 * @param {string} value The metric enumerator to record.
 * @param {Array<string>} validValues An array containing the valid enumerators
 *     in order.
 *
 */
test.util.sync.recordEnumMetric = (name, value, validValues) => {
  metrics.recordEnum(name, value, validValues);
};

/**
 * Reloads the Files app (Background & Foreground).
 * NOTE: Any foreground window opened before the reload will be killed, so any
 * appId/windowId won't be usable after the reload.
 */
test.util.sync.reload = () => {
  chrome.runtime.reload();
};

/**
 * Tells background page progress center to never notify a completed operation.
 */
test.util.sync.progressCenterNeverNotifyCompleted = () => {
  window.background.progressCenter.neverNotifyCompleted();
};
