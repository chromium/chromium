// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script loaded into the background page of a component
 * extension under test at runtime to populate testing functionality.
 */

import {assert} from 'chrome://resources/ash/common/assert.js';

import {metrics} from '../../common/js/metrics.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FileManagerBaseInterface} from '../../externs/background/file_manager_base.js';

import {test} from './test_util_base.js';

/** @type {!FileManagerBaseInterface} */
window.background;

/**
 * @typedef {{
 *   attributes:Object<string>,
 *   text:string,
 *   styles:(Object<string>|undefined),
 *   hidden:boolean,
 *   hasShadowRoot: boolean,
 *   imageWidth: (number|undefined),
 *   imageHeight: (number|undefined),
 *   renderedWidth: (number|undefined),
 *   renderedHeight: (number|undefined),
 *   renderedTop: (number|undefined),
 *   renderedLeft: (number|undefined),
 *   scrollLeft: (number|undefined),
 *   scrollTop: (number|undefined),
 *   scrollWidth: (number|undefined),
 *   scrollHeight: (number|undefined),
 *  }}
 */
export let ElementObject;

/**
 * Object containing common key modifiers: shift, alt, and ctrl.
 *
 * @typedef {{
 *   shift: (boolean|undefined),
 *   alt: (boolean|undefined),
 *   ctrl: (boolean|undefined),
 * }}
 */
export let KeyModifiers;

/**
 * @typedef {{
 *   fromCache: number,
 *   fullFetch: number,
 *   invalidateCount: number,
 *   clearCacheCount: number,
 *   clearAllCount: number,
 * }}
 */
let MetadataStatsType;

/**
 * Extract the information of the given element.
 * @param {Element} element Element to be extracted.
 * @param {Window} contentWindow Window to be tested.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained. NOTE: Causes element style re-calculation.
 * @return {!ElementObject} Element information that contains contentText,
 *     attribute names and values, hidden attribute, and style names and values.
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
    innerText: element.innerText,
    value: element.value,
    // The hidden attribute is not in the element.attributes even if
    // element.hasAttribute('hidden') is true.
    hidden: !!element.hidden,
    hasShadowRoot: !!element.shadowRoot,
  };

  const styleNames = opt_styleNames || [];
  assert(Array.isArray(styleNames));
  if (!styleNames.length) {
    return result;
  }

  // Force a style resolve and record the requested style values.
  result.styles = {};
  const size = element.getBoundingClientRect();
  const computedStyle = contentWindow.getComputedStyle(element);
  for (let i = 0; i < styleNames.length; i++) {
    result.styles[styleNames[i]] = computedStyle[styleNames[i]];
  }

  // These attributes are set when element is <img> or <canvas>.
  result.imageWidth = Number(element.width);
  result.imageHeight = Number(element.height);

  // Get the element client rectangle properties.
  result.renderedWidth = size.width;
  result.renderedHeight = size.height;
  result.renderedTop = size.top;
  result.renderedLeft = size.left;

  // Get the element scroll properties.
  result.scrollLeft = element.scrollLeft;
  result.scrollTop = element.scrollTop;
  result.scrollWidth = element.scrollWidth;
  result.scrollHeight = element.scrollHeight;

  return result;
}

/**
 * Gets total Javascript error count from background page and each app window.
 * @return {number} Error count.
 */
test.util.sync.getErrorCount = () => {
  return window.JSErrorCount;
};

/**
 * Resizes the window to the specified dimensions.
 *
 * @param {number} width Window width.
 * @param {number} height Window height.
 * @return {boolean} True for success.
 */
test.util.sync.resizeWindow = (width, height) => {
  window.resizeTo(width, height);
  return true;
};

/**
 * Queries all elements.
 *
 * @param {!Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {!Array<!ElementObject>} Element information that contains
 *     contentText, attribute names and values, hidden attribute, and style
 *     names and values.
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
 * @return {!Array<!ElementObject>} Element information that contains
 *     contentText, attribute names and values, hidden attribute, and style
 *     names and values.
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
 * Count elements matching the selector query.
 *
 * This avoid serializing and transmitting the elements to the test extension,
 * which can be time consuming for large elements.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {!Array<string>} query Query to specify the element.
 *   |query[0]| specifies the first element(s). |query[1]| specifies elements
 *   inside the shadow DOM of the first element, and so on.
 * @param {function(boolean)} callback Callback function with results if the
 *    number of elements match |count|.
 */
test.util.async.countElements = (contentWindow, query, count, callback) => {
  // Uses requestIdleCallback so it doesn't interfere with normal operation of
  // Files app UI.
  contentWindow.requestIdleCallback(() => {
    const elements =
        test.util.sync.deepQuerySelectorAll_(contentWindow.document, query);
    callback(elements.length === count);
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
 * Gets the information of the active element.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {?ElementObject} Element information that contains contentText,
 *     attribute names and values, hidden attribute, and style names and values.
 *     If there is no active element, returns null.
 */
test.util.sync.getActiveElement = (contentWindow, opt_styleNames) => {
  if (!contentWindow.document || !contentWindow.document.activeElement) {
    return null;
  }

  return extractElementInfo(
      contentWindow.document.activeElement, contentWindow, opt_styleNames);
};

/**
 * Gets the information of the active element. However, unlike the previous
 * helper, the shadow roots are searched as well.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {?ElementObject} Element information that contains contentText,
 *     attribute names and values, hidden attribute, and style names and values.
 *     If there is no active element, returns null.
 */
test.util.sync.deepGetActiveElement = (contentWindow, opt_styleNames) => {
  if (!contentWindow.document || !contentWindow.document.activeElement) {
    return null;
  }

  let activeElement = contentWindow.document.activeElement;
  while (true) {
    const shadow = activeElement.shadowRoot;
    if (shadow && shadow.activeElement) {
      activeElement = shadow.activeElement;
    } else {
      break;
    }
  }

  return extractElementInfo(activeElement, contentWindow, opt_styleNames);
};

/**
 * Gets an array of every activeElement, walking down the shadowRoot of every
 * active element it finds.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {Array<ElementObject>} Element information that contains contentText,
 *     attribute names and values, hidden attribute, and style names and values.
 *     If there is no active element, returns an empty array.
 */
test.util.sync.deepGetActivePath = (contentWindow, opt_styleNames) => {
  if (!contentWindow.document || !contentWindow.document.activeElement) {
    return [];
  }

  const path = [contentWindow.document.activeElement];
  while (true) {
    const shadow = path[path.length - 1].shadowRoot;
    if (shadow && shadow.activeElement) {
      path.push(shadow.activeElement);
    } else {
      break;
    }
  }

  return path.map(el => extractElementInfo(el, contentWindow, opt_styleNames));
};

/**
 * Assigns the text to the input element.
 * @param {Window} contentWindow Window to be tested.
 * @param {string|!Array<string>} query Query for the input element.
 *     If |query| is an array, |query[0]| specifies the first element(s),
 *     |query[1]| specifies elements inside the shadow DOM of the first element,
 *     and so on.
 * @param {string} text Text to be assigned.
 * @return {boolean} Whether or not the text was assigned.
 */
test.util.sync.inputText = (contentWindow, query, text) => {
  if (typeof query === 'string') {
    query = [query];
  }

  const elems =
      test.util.sync.deepQuerySelectorAll_(contentWindow.document, query);
  if (elems.length === 0) {
    console.error(`Input element not found: [${query.join(',')}]`);
    return false;
  }

  const input = elems[0];
  input.value = text;
  input.dispatchEvent(new Event('change'));
  return true;
};

/**
 * Sets the left scroll position of an element.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query for the test element.
 * @param {number} position scrollLeft position to set.
 * @return {boolean} True if operation was successful.
 */
test.util.sync.setScrollLeft = (contentWindow, query, position) => {
  contentWindow.document.querySelector(query).scrollLeft = position;
  return true;
};

/**
 * Sets the top scroll position of an element.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query for the test element.
 * @param {number} position scrollTop position to set.
 * @return {boolean} True if operation was successful.
 */
test.util.sync.setScrollTop = (contentWindow, query, position) => {
  contentWindow.document.querySelector(query).scrollTop = position;
  return true;
};

/**
 * Sets style properties for an element using the CSS OM.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query for the test element.
 * @param {!Object<?, string>} properties CSS Property name/values to set.
 * @return {boolean} Whether styles were set or not.
 */
test.util.sync.setElementStyles = (contentWindow, query, properties) => {
  const element = contentWindow.document.querySelector(query);
  if (element === null) {
    console.error(`Failed to locate element using query "${query}"`);
    return false;
  }
  for (const [key, value] of Object.entries(properties)) {
    element.style[key] = value;
  }
  return true;
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
    const elements = test.util.sync.deepQuerySelectorAll_(
        contentWindow.document, targetQuery);
    if (elements.length > 0) {
      target = elements[0];
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
          if (name === 'bubbles') {
            // bubbles is a read-only which, causes an error when assigning.
            continue;
          }
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
 * @param {KeyModifiers=} opt_keyModifiers Object containing common key
 *     modifiers : shift, alt, and ctrl.
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
 * @param {KeyModifiers=} opt_keyModifiers Object containing common key
 *     modifiers : shift, alt, and ctrl.
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
 * @param {KeyModifiers=} opt_keyModifiers Object containing common key
 *     modifiers : shift, alt, and ctrl.
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
 * @param {KeyModifiers=} opt_keyModifiers Object containing common key
 *     modifiers : shift, alt, and ctrl.
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
 * Sends drag and drop events to simulate dragging a source over a target.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} sourceQuery Query to specify the source element.
 * @param {string} targetQuery Query to specify the target element.
 * @param {boolean} skipDrop Set true to drag over (hover) the target
 *    only, and not send target drop or source dragend events.
 * @param {function(boolean)} callback Function called with result
 *    true on success, or false on failure.
 */
test.util.async.fakeDragAndDrop =
    (contentWindow, sourceQuery, targetQuery, skipDrop, callback) => {
      const source = contentWindow.document.querySelector(sourceQuery);
      const target = contentWindow.document.querySelector(targetQuery);

      if (!source || !target) {
        setTimeout(() => {
          callback(false);
        }, 0);
        return;
      }

      const targetOptions = {
        bubbles: true,
        composed: true,
        dataTransfer: new DataTransfer(),
      };

      // Get the middle of the source element since some of Files app
      // logic requires clientX and clientY.
      const sourceRect = source.getBoundingClientRect();
      const sourceOptions = Object.assign({}, targetOptions);
      sourceOptions.clientX = sourceRect.left + (sourceRect.width / 2);
      sourceOptions.clientY = sourceRect.top + (sourceRect.height / 2);

      let dragEventPhase = 0;
      let event = null;

      function sendPhasedDragDropEvents() {
        let result = true;
        switch (dragEventPhase) {
          case 0:
            event = new DragEvent('dragstart', sourceOptions);
            result = source.dispatchEvent(event);
            break;
          case 1:
            targetOptions.relatedTarget = source;
            event = new DragEvent('dragenter', targetOptions);
            result = target.dispatchEvent(event);
            break;
          case 2:
            targetOptions.relatedTarget = null;
            event = new DragEvent('dragover', targetOptions);
            result = target.dispatchEvent(event);
            break;
          case 3:
            if (!skipDrop) {
              targetOptions.relatedTarget = null;
              event = new DragEvent('drop', targetOptions);
              result = target.dispatchEvent(event);
            }
            break;
          case 4:
            if (!skipDrop) {
              event = new DragEvent('dragend', sourceOptions);
              result = source.dispatchEvent(event);
            }
            break;
          default:
            result = false;
            break;
        }

        if (!result) {
          callback(false);
        } else if (++dragEventPhase <= 4) {
          contentWindow.requestIdleCallback(sendPhasedDragDropEvents);
        } else {
          callback(true);
        }
      }

      sendPhasedDragDropEvents();
    };

/**
 * Sends a target dragleave or drop event, and source dragend event, to finish
 * the drag a source over target simulation started by fakeDragAndDrop for the
 * case where the target was hovered.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} sourceQuery Query to specify the source element.
 * @param {string} targetQuery Query to specify the target element.
 * @param {boolean} dragLeave Set true to send a dragleave event to
 *    the target instead of a drop event.
 * @param {function(boolean)} callback Function called with result
 *    true on success, or false on failure.
 */
test.util.async.fakeDragLeaveOrDrop =
    (contentWindow, sourceQuery, targetQuery, dragLeave, callback) => {
      const source = contentWindow.document.querySelector(sourceQuery);
      const target = contentWindow.document.querySelector(targetQuery);

      if (!source || !target) {
        setTimeout(() => {
          callback(false);
        }, 0);
        return;
      }

      const targetOptions = {
        bubbles: true,
        composed: true,
        dataTransfer: new DataTransfer(),
      };

      // Get the middle of the source element since some of Files app
      // logic requires clientX and clientY.
      const sourceRect = source.getBoundingClientRect();
      const sourceOptions = Object.assign({}, targetOptions);
      sourceOptions.clientX = sourceRect.left + (sourceRect.width / 2);
      sourceOptions.clientY = sourceRect.top + (sourceRect.height / 2);

      // Define the target event type.
      const targetType = dragLeave ? 'dragleave' : 'drop';

      let dragEventPhase = 0;
      let event = null;

      function sendPhasedDragEndEvents() {
        let result = false;
        switch (dragEventPhase) {
          case 0:
            event = new DragEvent(targetType, targetOptions);
            result = target.dispatchEvent(event);
            break;
          case 1:
            event = new DragEvent('dragend', sourceOptions);
            result = source.dispatchEvent(event);
            break;
        }

        if (!result) {
          callback(false);
        } else if (++dragEventPhase <= 1) {
          contentWindow.requestIdleCallback(sendPhasedDragEndEvents);
        } else {
          callback(true);
        }
      }

      sendPhasedDragEndEvents();
    };

/**
 * Sends a drop event to simulate dropping a file originating in the browser to
 * a target.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} fileName File name.
 * @param {string} fileContent File content.
 * @param {string} fileMimeType File mime type.
 * @param {string} targetQuery Query to specify the target element.
 * @param {function(boolean)} callback Function called with result
 *    true on success, or false on failure.
 */
test.util.async.fakeDropBrowserFile =
    (contentWindow, fileName, fileContent, fileMimeType, targetQuery,
     callback) => {
      const target = contentWindow.document.querySelector(targetQuery);

      if (!target) {
        setTimeout(() => callback(false));
        return;
      }

      const file = new File([fileContent], fileName, {type: fileMimeType});
      const dataTransfer = new DataTransfer();
      dataTransfer.items.add(file);
      // The value for the callback is true if the event has been handled, i.e.
      // event has been received and preventDefault() called.
      callback(target.dispatchEvent(new DragEvent('drop', {
        bubbles: true,
        composed: true,
        dataTransfer: dataTransfer,
      })));
    };

/**
 * Sends a resize event to the content window.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {boolean} True if the event was sent to the contentWindow.
 */
test.util.sync.fakeResizeEvent = (contentWindow) => {
  const resize = contentWindow.document.createEvent('Event');
  resize.initEvent('resize', false, false);
  return contentWindow.dispatchEvent(resize);
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
 * Gets file entries just under the volume.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<string>} names File name list.
 * @param {function(*)} callback Callback function with results returned by the
 *     script.
 */
test.util.async.getFilesUnderVolume = async (volumeType, names, callback) => {
  const volumeManager = await window.background.getVolumeManager();
  let volumeInfo = null;
  let displayRoot = null;

  // Wait for the volume to initialize.
  while (!(volumeInfo && displayRoot)) {
    volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
    if (volumeInfo) {
      displayRoot = await volumeInfo.resolveDisplayRoot();
    }
    if (!displayRoot) {
      await new Promise(resolve => setTimeout(resolve, 100));
    }
  }

  const filesPromise = names.map(name => {
    // TODO(crbug.com/880130): Remove this conditional.
    if (volumeType === VolumeManagerCommon.VolumeType.DOWNLOADS) {
      name = 'Downloads/' + name;
    }
    return new Promise(displayRoot.getFile.bind(displayRoot, name, {}));
  });

  try {
    const urls = await Promise.all(filesPromise);
    const result = util.entriesToURLs(urls);
    callback(result);
  } catch (error) {
    console.error(error);
    callback([]);
  }
};

/**
 * Unmounts the specified volume.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {function(boolean)} callback Function receives true on success.
 */
test.util.async.unmount = async (volumeType, callback) => {
  const volumeManager = await window.background.getVolumeManager();
  const volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
  try {
    if (volumeInfo) {
      await volumeManager.unmount(volumeInfo);
      callback(true);
      return;
    }
  } catch (error) {
    console.error(error);
  }
  callback(false);
};

/**
 * Remote call API handler. When loaded, this replaces the declaration in
 * test_util_base.js.
 * @param {*} request
 * @param {function(*): void} sendResponse
 * @return {boolean|undefined}
 */
test.util.executeTestMessage = (request, sendResponse) => {
  window.IN_TEST = true;
  // Check the function name.
  if (!request.func || request.func[request.func.length - 1] == '_') {
    request.func = '';
  }
  // Prepare arguments.
  if (!('args' in request)) {
    throw new Error('Invalid request: no args provided.');
  }

  const args = request.args.slice();  // shallow copy
  if (request.appId) {
    if (request.contentWindow) {
      // request.contentWindow is present if this function was called via
      // test.swaTestMessageListener.
      args.unshift(request.contentWindow);
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
    try {
      sendResponse(test.util.sync[request.func].apply(null, args));
    } catch (e) {
      console.error(`Failure executing ${request.func}: ${e}`);
      sendResponse(null);
    }
    return false;
  } else {
    console.error('Invalid function name: ' + request.func);
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
  return window.background.getVolumeManager().then((volumeManager) => {
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
  return true;
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
  return true;
};

/**
 * Tells background page progress center to never notify a completed operation.
 * @suppress {checkTypes} Remove suppress when migrating Files app. This is only
 *     used for Files app.
 */
test.util.sync.progressCenterNeverNotifyCompleted = () => {
  window.background.progressCenter.neverNotifyCompleted();
  return true;
};

/**
 * Waits for the background page to initialize.
 * @param {function()} callback Callback function called when background page
 *      has finished initializing.
 * @suppress {missingProperties}: ready() isn't available for Audio and Video
 * Player.
 */
test.util.async.waitForBackgroundReady = callback => {
  window.background.ready(callback);
};

/**
 * Isolates a specific banner to be shown. Useful when testing functionality of
 * a banner in isolation.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} bannerTagName Tag name of the banner to isolate.
 * @param {function(boolean)} callback Callback function to be called with a
 *    boolean indicating success or failure.
 * @suppress {missingProperties} banners is only defined for foreground
 *    Window so it isn't visible in the background.
 */
test.util.async.isolateBannerForTesting =
    async (contentWindow, bannerTagName, callback) => {
  try {
    await contentWindow.fileManager.ui_.banners.isolateBannerForTesting(
        bannerTagName);
    callback(true);
    return;
  } catch (e) {
    console.error(`Error isolating banner with tagName ${
        bannerTagName} for testing: ${e}`);
  }
  callback(false);
};

/**
 * Disable banners from attaching themselves to the DOM.
 *
 * @param {Window} contentWindow Window the banner controller exists.
 * @param {function(boolean)} callback Callback function to be called with a
 *    boolean indicating success or failure.
 * @suppress {missingProperties} banners is only defined for foreground
 *    Window so it isn't visible in the background.
 */
test.util.async.disableBannersForTesting = async (contentWindow, callback) => {
  try {
    await contentWindow.fileManager.ui_.banners.disableBannersForTesting();
    callback(true);
    return;
  } catch (e) {
    console.error(`Error disabling banners for testing: ${e}`);
  }
  callback(false);
};

/**
 * Disables the nudge expiry period for testing.
 *
 * @param {Window} contentWindow Window the banner controller exists.
 * @param {function(boolean)} callback Callback function to be called with a
 *    boolean indicating success or failure.
 * @suppress {missingProperties} nudgeContainer is only defined for foreground
 *    Window so it isn't visible in the background.
 */
test.util.async.disableNudgeExpiry = async (contentWindow, callback) => {
  contentWindow.fileManager.ui_.nudgeContainer
      .setExpiryPeriodEnabledForTesting = false;
  callback(true);
};
