// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script loaded into the background page of a component
 * extension under test at runtime to populate testing functionality.
 */

import './test_util.js';

import {assert} from 'chrome://resources/js/assert.js';

import {entriesToURLs} from '../../common/js/entry_utils.js';
import {recordEnum} from '../../common/js/metrics.js';
import {type ElementObject, type KeyModifiers, VolumeType} from '../../common/js/shared_types.js';
import {debug} from '../../common/js/util.js';
import type {MetadataKey} from '../../foreground/js/metadata/metadata_item.js';

import {test} from './test_util_base.js';

export interface RemoteRequest {
  func: string;
  args: unknown[];
  appId: string;
  contentWindow?: Window;
}
/**
 * Extract the information of the given element.
 * @param element Element to be extracted.
 * @param contentWindow Window to be tested.
 * @param styleNames List of CSS property name to be obtained. NOTE: Causes
 *     element style re-calculation.
 * @return Element information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values.
 */
function extractElementInfo(
    element: HTMLElement, contentWindow: Window,
    styleNames?: string[]): ElementObject {
  const attributes: Record<string, string|null> = {};
  for (const attr of element.attributes) {
    attributes[attr.nodeName] = attr.nodeValue;
  }

  const result: ElementObject = {
    attributes: attributes,
    styles: {},
    text: element.textContent,
    innerText: element.innerText,
    value: (element as HTMLInputElement).value,
    // The hidden attribute is not in the element.attributes even if
    // element.hasAttribute('hidden') is true.
    hidden: !!element.hidden,
    hasShadowRoot: !!element.shadowRoot,
  };

  styleNames = styleNames || [];
  assert(Array.isArray(styleNames));
  if (!styleNames.length) {
    return result;
  }

  // Force a style resolve and record the requested style values.
  const size = element.getBoundingClientRect();
  const computedStyle = contentWindow.getComputedStyle(element);
  for (const style of styleNames) {
    result.styles![style] = computedStyle.getPropertyValue(style);
  }

  // These attributes are set when element is <img> or <canvas>.
  result.imageWidth = Number((element as HTMLImageElement).width);
  result.imageHeight = Number((element as HTMLImageElement).height);

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
 * @return Error count.
 */
test.util.sync.getErrorCount = (): number => {
  return window.JSErrorCount;
};

/**
 * Resizes the window to the specified dimensions.
 *
 * @param width Window width.
 * @param height Window height.
 * @return True for success.
 */
test.util.sync.resizeWindow = (width: number, height: number): boolean => {
  window.resizeTo(width, height);
  return true;
};

/**
 * Queries all elements.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @param styleNames List of CSS property name to be obtained.
 * @return Element information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values.
 */
test.util.sync.queryAllElements =
    (contentWindow: Window, targetQuery: string, styleNames?: string[]):
        ElementObject[] => {
          return test.util.sync.deepQueryAllElements(
              contentWindow, targetQuery, styleNames);
        };

/**
 * Queries elements inside shadow DOM.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 *   |targetQuery[0]| specifies the first element(s). |targetQuery[1]| specifies
 *   elements inside the shadow DOM of the first element, and so on.
 * @param styleNames List of CSS property name to be obtained.
 * @return Element information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values.
 */
test.util.sync.deepQueryAllElements =
    (contentWindow: Window, targetQuery: string|string[],
     styleNames?: string[]): ElementObject[] => {
      if (!contentWindow.document) {
        return [];
      }
      if (typeof targetQuery === 'string') {
        targetQuery = [targetQuery];
      }

      const elems = deepQuerySelectorAll(contentWindow.document, targetQuery);
      return elems.map((element: HTMLElement) => {
        return extractElementInfo(element, contentWindow, styleNames);
      });
    };

/**
 * Count elements matching the selector query.
 *
 * This avoid serializing and transmitting the elements to the test extension,
 * which can be time consuming for large elements.
 *
 * @param contentWindow Window to be tested.
 * @param query Query to specify the element.
 *   |query[0]| specifies the first element(s). |query[1]| specifies elements
 *   inside the shadow DOM of the first element, and so on.
 * @param callback Callback function with results if the number of elements
 *     match |count|.
 */
test.util.async.countElements =
    (contentWindow: Window, query: string[], count: number,
     callback: (a: boolean) => void) => {
      // Uses requestIdleCallback so it doesn't interfere with normal operation
      // of Files app UI.
      contentWindow.requestIdleCallback(() => {
        const elements = deepQuerySelectorAll(contentWindow.document, query);
        callback(elements.length === count);
      });
    };

/**
 * Selects elements below |root|, possibly following shadow DOM subtree.
 *
 * @param root Element to search from.
 * @param targetQuery Query to specify the element.
 *   |targetQuery[0]| specifies the first element(s). |targetQuery[1]| specifies
 *   elements inside the shadow DOM of the first element, and so on.
 * @return Matched elements.
 */
function deepQuerySelectorAll(
    root: HTMLElement|Document, targetQuery: string[]): HTMLElement[] {
  const elems =
      Array.prototype.slice.call(root.querySelectorAll(targetQuery[0]!));
  const remaining = targetQuery.slice(1);
  if (remaining.length === 0) {
    return elems;
  }

  let res: HTMLElement[] = [];
  for (const elem of elems) {
    if ('shadowRoot' in elem) {
      res = res.concat(deepQuerySelectorAll(elem.shadowRoot, remaining));
    }
  }
  return res;
}

/**
 * Gets the information of the active element.
 *
 * @param contentWindow Window to be tested.
 * @param styleNames List of CSS property name to be obtained.
 * @return Element information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values. If there is no
 *     active element, returns null.
 */
test.util.sync.getActiveElement =
    (contentWindow: Window, styleNames?: string[]): null|ElementObject => {
      if (!contentWindow.document || !contentWindow.document.activeElement) {
        return null;
      }

      return extractElementInfo(
          contentWindow.document.activeElement as HTMLElement, contentWindow,
          styleNames);
    };

/**
 * Gets the information of the active element. However, unlike the previous
 * helper, the shadow roots are searched as well.
 *
 * @param contentWindow Window to be tested.
 * @param styleNames List of CSS property name to be obtained.
 * @return Element information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values. If there is no
 *     active element, returns null.
 */
test.util.sync.deepGetActiveElement =
    (contentWindow: Window, styleNames?: string[]): null|ElementObject => {
      if (!contentWindow.document || !contentWindow.document.activeElement) {
        return null;
      }

      let activeElement = contentWindow.document.activeElement as HTMLElement;
      while (true) {
        const shadow = activeElement.shadowRoot;
        if (shadow && shadow.activeElement) {
          activeElement = shadow.activeElement as HTMLElement;
        } else {
          break;
        }
      }

      return extractElementInfo(activeElement, contentWindow, styleNames);
    };

/**
 * Gets an array of every activeElement, walking down the shadowRoot of every
 * active element it finds.
 *
 * @param contentWindow Window to be tested.
 * @param styleNames List of CSS property name to be obtained.
 * @return Element information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values. If there is no
 * active element, returns an empty array.
 */
test.util.sync.deepGetActivePath =
    (contentWindow: Window, styleNames?: string[]): ElementObject[] => {
      if (!contentWindow.document || !contentWindow.document.activeElement) {
        return [];
      }

      const path = [contentWindow.document.activeElement];
      while (true) {
        const shadow = path[path.length - 1]?.shadowRoot;
        if (shadow && shadow.activeElement) {
          path.push(shadow.activeElement);
        } else {
          break;
        }
      }

      return path.map(
          el =>
              extractElementInfo(el as HTMLElement, contentWindow, styleNames));
    };

/**
 * Assigns the text to the input element.
 * @param contentWindow Window to be tested.
 * @param query Query for the input element.
 *     If |query| is an array, |query[0]| specifies the first element(s),
 *     |query[1]| specifies elements inside the shadow DOM of the first element,
 *     and so on.
 * @param text Text to be assigned.
 * @return Whether or not the text was assigned.
 */
test.util.sync.inputText =
    (contentWindow: Window, query: string|string[], text: string): boolean => {
      if (typeof query === 'string') {
        query = [query];
      }

      const elems = deepQuerySelectorAll(contentWindow.document, query);
      if (elems.length === 0) {
        console.error(`Input element not found: [${query.join(',')}]`);
        return false;
      }

      const input = elems[0] as HTMLInputElement;
      input.value = text;
      input.dispatchEvent(new Event('change'));
      return true;
    };

/**
 * Sets the left scroll position of an element.
 * @param contentWindow Window to be tested.
 * @param query Query for the test element.
 * @param position scrollLeft position to set.
 * @return True if operation was successful.
 */
test.util.sync.setScrollLeft =
    (contentWindow: Window, query: string, position: number): boolean => {
      contentWindow.document.querySelector(query)!.scrollLeft = position;
      return true;
    };

/**
 * Sets the top scroll position of an element.
 * @param contentWindow Window to be tested.
 * @param query Query for the test element.
 * @param position scrollTop position to set.
 * @return True if operation was successful.
 */
test.util.sync.setScrollTop =
    (contentWindow: Window, query: string, position: number): boolean => {
      contentWindow.document.querySelector(query)!.scrollTop = position;
      return true;
    };

/**
 * Sets style properties for an element using the CSS OM.
 * @param contentWindow Window to be tested.
 * @param query Query for the test element.
 * @param properties CSS Property name/values to set.
 * @return Whether styles were set or not.
 */
test.util.sync.setElementStyles =
    (contentWindow: Window, query: string,
     properties: Record<string, string>): boolean => {
      const element = contentWindow.document.querySelector<HTMLElement>(query);
      if (element === null) {
        console.error(`Failed to locate element using query "${query}"`);
        return false;
      }
      for (const [key, value] of Object.entries(properties)) {
        element.style.setProperty(key, value);
      }
      return true;
    };

/**
 * Sends an event to the element specified by |targetQuery| or active element.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 *     If this value is null, an event is dispatched to active element of the
 *     document.
 *     If targetQuery is an array, |targetQuery[0]| specifies the first
 *     element(s), |targetQuery[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param event Event to be sent.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.sendEvent =
    (contentWindow: Window, targetQuery: null|string|string[], event: Event):
        boolean => {
          if (!contentWindow.document) {
            return false;
          }

          let target;
          if (targetQuery === null) {
            target = contentWindow.document.activeElement;
          } else if (typeof targetQuery === 'string') {
            target = contentWindow.document.querySelector(targetQuery);
          } else if (Array.isArray(targetQuery)) {
            const elements =
                deepQuerySelectorAll(contentWindow.document, targetQuery);
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
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @param eventType Type of event.
 * @param additionalProperties Object containing additional properties.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeEvent =
    (contentWindow: Window, targetQuery: string, eventType: string,
     additionalProperties?: Record<string, string>): boolean => {
      const isCustomEvent = 'detail' in (additionalProperties || {});

      const event = isCustomEvent ?
          new CustomEvent(eventType, additionalProperties || {}) :
          new Event(eventType, additionalProperties || {});
      if (!isCustomEvent && additionalProperties) {
        for (const name in additionalProperties) {
          if (name === 'bubbles') {
            // bubbles is a read-only which, causes an error when assigning.
            continue;
          }
          (event as any)[name] = additionalProperties[name];
        }
      }
      return test.util.sync.sendEvent(contentWindow, targetQuery, event);
    };

/**
 * Sends a fake key event to the element specified by |targetQuery| or active
 * element with the given |key| and optional |ctrl,shift,alt| modifier.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element. If this value is
 *     null, key event is dispatched to active element of the document.
 * @param key DOM UI Events key value.
 * @param ctrl Whether CTRL should be pressed, or not.
 * @param shift whether SHIFT should be pressed, or not.
 * @param alt whether ALT should be pressed, or not.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeKeyDown =
    (contentWindow: Window, targetQuery: null|string, key: string,
     ctrl: boolean, shift: boolean, alt: boolean): boolean => {
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
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 *     If targetQuery is an array, |targetQuery[0]| specifies the first
 *     element(s), |targetQuery[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param keyModifiers Object containing common key modifiers : shift, alt, and
 *     ctrl.
 * @param button Mouse button number as per spec, e.g.: 2 for right-click.
 * @param eventProperties Additional properties to pass to each event, e.g.:
 *     clientX and clientY. right-click.
 * @return True if the all events are sent to the target, false otherwise.
 */
test.util.sync.fakeMouseClick =
    (contentWindow: Window, targetQuery: string|string[],
     keyModifiers?: KeyModifiers, button?: number,
     eventProperties?: Object): boolean => {
      const modifiers = keyModifiers || {};
      eventProperties = eventProperties || {};

      const props: MouseEventInit = Object.assign(
          {
            bubbles: true,
            detail: 1,
            composed: true,  // Allow the event to bubble past shadow DOM root.
            ctrlKey: modifiers.ctrl,
            shiftKey: modifiers.shift,
            altKey: modifiers.alt,
          },
          eventProperties);
      if (button !== undefined) {
        props.button = button;
      }

      if (!targetQuery) {
        return false;
      }
      if (typeof targetQuery === 'string') {
        targetQuery = [targetQuery];
      }
      const elems = deepQuerySelectorAll(contentWindow.document, targetQuery);
      if (elems.length === 0) {
        return false;
      }
      // Only sends the event to the first matched element.
      const target = elems[0]!;

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
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 *     If targetQuery is an array, |targetQuery[0]| specifies the first
 *     element(s), |targetQuery[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param keyModifiers Object containing common key modifiers : shift, alt, and
 *     ctrl.
 * @return True if the event was sent to the target, false otherwise.
 */
test.util.sync.fakeMouseOver =
    (contentWindow: Window, targetQuery: string|string[],
     keyModifiers?: KeyModifiers): boolean => {
      const modifiers = keyModifiers || {};
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
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 *     If targetQuery is an array, |targetQuery[0]| specifies the first
 *     element(s), |targetQuery[1]| specifies elements inside the shadow DOM of
 *     the first element, and so on.
 * @param keyModifiers Object containing common key modifiers : shift, alt, and
 *     ctrl.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseOut =
    (contentWindow: Window, targetQuery: string|string[],
     keyModifiers?: KeyModifiers): boolean => {
      const modifiers = keyModifiers || {};
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
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @param keyModifiers Object containing common key modifiers: shift, alt, and
 *     ctrl.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseRightClick =
    (contentWindow: Window, targetQuery: string, keyModifiers?: KeyModifiers):
        boolean => {
          const clickResult = test.util.sync.fakeMouseClick(
              contentWindow, targetQuery, keyModifiers, 2 /* right button */);
          if (!clickResult) {
            return false;
          }

          return test.util.sync.fakeContextMenu(contentWindow, targetQuery);
        };

/**
 * Simulate a fake contextmenu event without right clicking on the element
 * specified by |targetQuery|. This is mainly to simulate long press on the
 * element.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeContextMenu =
    (contentWindow: Window, targetQuery: string): boolean => {
      const contextMenuEvent =
          new MouseEvent('contextmenu', {bubbles: true, composed: true});
      return test.util.sync.sendEvent(
          contentWindow, targetQuery, contextMenuEvent);
    };

/**
 * Simulates a fake touch event (touch start and touch end) on the element
 * specified by |targetQuery|.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync
    .fakeTouchClick = (contentWindow: Window, targetQuery: string): boolean => {
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
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseDoubleClick =
    (contentWindow: Window, targetQuery: string): boolean => {
      // Double click is always preceded with a single click.
      if (!test.util.sync.fakeMouseClick(contentWindow, targetQuery)) {
        return false;
      }

      // Send the second click event, but with detail equal to 2 (number of
      // clicks) in a row.
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
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseDown =
    (contentWindow: Window, targetQuery: string): boolean => {
      const event =
          new MouseEvent('mousedown', {bubbles: true, composed: true});
      return test.util.sync.sendEvent(contentWindow, targetQuery, event);
    };

/**
 * Sends a fake mouse up event to the element specified by |targetQuery|.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @return True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseUp =
    (contentWindow: Window, targetQuery: string): boolean => {
      const event = new MouseEvent('mouseup', {bubbles: true, composed: true});
      return test.util.sync.sendEvent(contentWindow, targetQuery, event);
    };

/**
 * Simulates a mouse right-click on the element specified by |targetQuery|.
 * Optionally pass X,Y coordinates to be able to choose where the right-click
 * should occur.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @param offsetBottom offset pixels applied to target element bottom, can be
 *     negative to move above the bottom.
 * @param offsetRight offset pixels applied to target element right can be
 *     negative to move inside the element.
 * @return True if the all events are sent to the target, false otherwise.
 */
test.util.sync.rightClickOffset =
    (contentWindow: Window, targetQuery: string, offsetBottom?: number,
     offsetRight?: number): boolean => {
      const target = contentWindow.document &&
          contentWindow.document.querySelector(targetQuery);
      if (!target) {
        return false;
      }

      // Calculate the offsets.
      const targetRect = target.getBoundingClientRect();
      const props = {
        clientX: targetRect.right + (offsetRight ? offsetRight : 0),
        clientY: targetRect.bottom + (offsetBottom ? offsetBottom : 0),
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
 * @param contentWindow Window to be tested.
 * @param sourceQuery Query to specify the source element.
 * @param targetQuery Query to specify the target element.
 * @param skipDrop Set true to drag over (hover) the target only, and not send
 *     target drop or source dragend events.
 * @param callback Function called with result true on success, or false on
 *     failure.
 */
test.util.async.fakeDragAndDrop =
    (contentWindow: Window, sourceQuery: string, targetQuery: string,
     skipDrop: boolean, callback: (a: boolean) => void) => {
      const source = contentWindow.document.querySelector(sourceQuery);
      const target = contentWindow.document.querySelector(targetQuery);

      if (!source || !target) {
        setTimeout(() => {
          callback(false);
        }, 0);
        return;
      }

      const targetOptions: DragEventInit = {
        bubbles: true,
        composed: true,
        dataTransfer: new DataTransfer(),
      };

      // Get the middle of the source element since some of Files app logic
      // requires clientX and clientY.
      const sourceRect = source.getBoundingClientRect();
      const sourceOptions: MouseEventInit = Object.assign({}, targetOptions);
      sourceOptions.clientX = sourceRect.left + (sourceRect.width / 2);
      sourceOptions.clientY = sourceRect.top + (sourceRect.height / 2);

      let dragEventPhase = 0;
      let event = null;

      function sendPhasedDragDropEvents() {
        assert(source);
        assert(target);

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
 * @param contentWindow Window to be tested.
 * @param sourceQuery Query to specify the source element.
 * @param targetQuery Query to specify the target element.
 * @param dragLeave Set true to send a dragleave event to the target instead of
 *     a drop event.
 * @param callback Function called with result true on success, or false on
 *     failure.
 */
test.util.async.fakeDragLeaveOrDrop =
    (contentWindow: Window, sourceQuery: string, targetQuery: string,
     dragLeave: boolean, callback: (a: boolean) => void) => {
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

      // Get the middle of the source element since some of Files app logic
      // requires clientX and clientY.
      const sourceRect = source.getBoundingClientRect();
      const sourceOptions: MouseEventInit = Object.assign({}, targetOptions);
      sourceOptions.clientX = sourceRect.left + (sourceRect.width / 2);
      sourceOptions.clientY = sourceRect.top + (sourceRect.height / 2);

      // Define the target event type.
      const targetType = dragLeave ? 'dragleave' : 'drop';

      let dragEventPhase = 0;
      let event = null;

      function sendPhasedDragEndEvents() {
        let result = false;
        assert(source);
        assert(target);
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
 * @param contentWindow Window to be tested.
 * @param fileName File name.
 * @param fileContent File content.
 * @param fileMimeType File mime type.
 * @param targetQuery Query to specify the target element.
 * @param callback Function called with result true on success, or false on
 *     failure.
 */
test.util.async.fakeDropBrowserFile =
    (contentWindow: Window, fileName: string, fileContent: string,
     fileMimeType: string, targetQuery: string,
     callback: (a: boolean) => void) => {
      const target = contentWindow.document.querySelector(targetQuery);

      if (!target) {
        setTimeout(() => callback(false));
        return;
      }

      const file = new File([fileContent], fileName, {type: fileMimeType});
      const dataTransfer = new DataTransfer();
      dataTransfer.items.add(file);
      // The value for the callback is true if the event has been handled,
      // i.e. event has been received and preventDefault() called.
      callback(target.dispatchEvent(new DragEvent('drop', {
        bubbles: true,
        composed: true,
        dataTransfer: dataTransfer,
      })));
    };

/**
 * Sends a resize event to the content window.
 *
 * @param contentWindow Window to be tested.
 * @return True if the event was sent to the contentWindow.
 */
test.util.sync.fakeResizeEvent = (contentWindow: Window): boolean => {
  const resize = contentWindow.document.createEvent('Event');
  resize.initEvent('resize', false, false);
  return contentWindow.dispatchEvent(resize);
};

/**
 * Focuses to the element specified by |targetQuery|. This method does not
 * provide any guarantee whether the element is actually focused or not.
 *
 * @param contentWindow Window to be tested.
 * @param targetQuery Query to specify the element.
 * @return True if focus method of the element has been called, false otherwise.
 */
test.util.sync.focus =
    (contentWindow: Window, targetQuery: string): boolean => {
      const target = contentWindow.document &&
          contentWindow.document.querySelector<HTMLElement>(targetQuery);

      if (!target) {
        return false;
      }

      target.focus();
      return true;
    };

/**
 * Obtains the list of notification ID.
 * @param _callback Callback function with results returned by the script.
 */
test.util
    .async.getNotificationIDs = (_callback: (a: Record<string, boolean>) => void) => {
  // TODO(TS): Add type for chrome notifications.
  // TODO(b/189173190): Enable
  // TODO(b/296960734): Enable
  // chrome.notifications.getAll(callback);
  throw new Error(
      'See b/189173190 and b/296960734 to renable the tests using this function');
};

/**
 * Gets file entries just under the volume.
 *
 * @param volumeType Volume type.
 * @param names File name list.
 * @param callback Callback function with results returned by
 *     the script.
 */
test.util.async.getFilesUnderVolume = async (
    volumeType: VolumeType, names: string[],
    callback: (a: unknown) => void) => {
  const volumeManager = await window.background.getVolumeManager();
  let volumeInfo = null;
  let displayRoot: DirectoryEntry|null = null;

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
    // TODO(crbug.com/40591990): Remove this conditional.
    if (volumeType === VolumeType.DOWNLOADS) {
      name = 'Downloads/' + name;
    }
    return new Promise(displayRoot!.getFile.bind(displayRoot, name, {}));
  });

  try {
    const urls = await Promise.all(filesPromise);
    const result = entriesToURLs(urls);
    callback(result);
  } catch (error) {
    console.error(error);
    callback([]);
  }
};

/**
 * Unmounts the specified volume.
 *
 * @param volumeType Volume type.
 * @param callback Function receives true on success.
 */
test.util.async.unmount =
    async (volumeType: VolumeType, callback: (a: boolean) => void) => {
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
 */
test.util.executeTestMessage =
    (request: RemoteRequest, sendResponse: (...a: unknown[]) => void): boolean|
    undefined => {
      window.IN_TEST = true;
      // Check the function name.
      if (!request.func || request.func[request.func.length - 1] === '_') {
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
        args[test.util.async[request.func].length - 1] = function(
            ...innerArgs: any[]) {
          debug('Received the result of ' + request.func);
          sendResponse.apply(null, innerArgs);
        };
        debug('Waiting for the result of ' + request.func);
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
 */
test.util.sync.getMetadataStats = (contentWindow: Window) => {
  return contentWindow.fileManager.metadataModel.getStats();
};

/**
 * Calls the metadata model to get the selected file entries in the file list
 * and try to get their metadata properties.
 *
 * @param properties Content metadata properties to get.
 * @param callback Callback with metadata results returned.
 */
test.util.async.getContentMetadata =
    (contentWindow: Window, properties: MetadataKey[],
     callback: (a: unknown) => void) => {
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
test.util.sync.isFileManagerLoaded = (contentWindow: Window) => {
  if (contentWindow && contentWindow.fileManager) {
    try {
      // The test util functions can be loaded prior to the fileManager.ui
      // element being available, this results in an assertion failure. Treat
      // this as file manager not being loaded instead of a hard failure.
      return contentWindow.fileManager.ui.element.hasAttribute('loaded');
    } catch (e) {
      console.warn(e);
      return false;
    }
  }

  return false;
};

/**
 * Returns all a11y messages announced by |FileManagerUI.speakA11yMessage|.
 */
test.util.sync.getA11yAnnounces = (contentWindow: Window): null|string[] => {
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
 * @param callback Callback function to be called with the number of volumes.
 */
test.util.async.getVolumesCount = (callback: (a: number) => void) => {
  return window.background.getVolumeManager().then((volumeManager) => {
    callback(volumeManager.volumeInfoList.length);
  });
};

/**
 * Updates the preferences.
 */
test.util.sync.setPreferences =
    (preferences: chrome.fileManagerPrivate.PreferencesChange) => {
      chrome.fileManagerPrivate.setPreferences(preferences);
      return true;
    };

/**
 * Reports an enum metric.
 * @param name The metric name.
 * @param value The metric enumerator to record.
 * @param validValues An array containing the valid enumerators in order.
 */
test.util.sync.recordEnumMetric =
    (name: string, value: string, validValues: string[]) => {
      recordEnum(name, value, validValues);
      return true;
    };

/**
 * Tells background page progress center to never notify a completed operation.
 */
test.util.sync.progressCenterNeverNotifyCompleted = () => {
  window.background.progressCenter.neverNotifyCompleted();
  return true;
};

/**
 * Waits for the background page to initialize.
 * @param callback Callback function called when background page has finished
 *     initializing.
 */
test.util.async.waitForBackgroundReady = async (callback: VoidCallback) => {
  await window.background.ready();
  callback();
};

/**
 * Isolates a specific banner to be shown. Useful when testing functionality
 * of a banner in isolation.
 *
 * @param contentWindow Window to be tested.
 * @param bannerTagName Tag name of the banner to isolate.
 * @param callback Callback function to be called with a boolean indicating
 *     success or failure.
 */
test.util.async.isolateBannerForTesting = async (
    contentWindow: Window, bannerTagName: string,
    callback: (a: boolean) => void) => {
  try {
    await contentWindow.fileManager.ui.banners!.isolateBannerForTesting(
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
 * @param contentWindow Window the banner controller exists.
 * @param callback Callback function to be called with a boolean indicating
 *     success or failure.
 */
test.util.async.disableBannersForTesting =
    async (contentWindow: Window, callback: (a: boolean) => void) => {
  try {
    await contentWindow.fileManager.ui.banners!.disableBannersForTesting();
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
 * @param contentWindow Window the banner controller exists.
 * @param callback Callback function to be called with a boolean indicating
 *     success or failure.
 */
test.util.async.disableNudgeExpiry =
    async (contentWindow: Window, callback: (a: boolean) => void) => {
  contentWindow.fileManager.ui.nudgeContainer.setExpiryPeriodEnabledForTesting =
      false;
  callback(true);
};
