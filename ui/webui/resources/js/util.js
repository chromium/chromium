// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assertInstanceof} from './assert.m.js';
// #import {dispatchSimpleEvent} from './cr.m.js';

/**
 * Alias for document.getElementById. Found elements must be HTMLElements.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
/* #export */ function $(id) {
  // Disable getElementById restriction here, since we are instructing other
  // places to re-use the $() that is defined here.
  // eslint-disable-next-line no-restricted-properties
  const el = document.getElementById(id);
  return el ? assertInstanceof(el, HTMLElement) : null;
}

// TODO(devlin): This should return SVGElement, but closure compiler is missing
// those externs.
/**
 * Alias for document.getElementById. Found elements must be SVGElements.
 * @param {string} id The ID of the element to find.
 * @return {Element} The found element or null if not found.
 */
/* #export */ function getSVGElement(id) {
  // Disable getElementById restriction here, since it is not suitable for SVG
  // elements.
  // eslint-disable-next-line no-restricted-properties
  const el = document.getElementById(id);
  return el ? assertInstanceof(el, Element) : null;
}

/**
 * @return {?Element} The currently focused element (including elements that are
 *     behind a shadow root), or null if nothing is focused.
 */
/* #export */ function getDeepActiveElement() {
  let a = document.activeElement;
  while (a && a.shadowRoot && a.shadowRoot.activeElement) {
    a = a.shadowRoot.activeElement;
  }
  return a;
}

// <if expr="chromeos">
/**
 * DEPRECATED (if using Polymer): Use Polymer.IronA11yAnnouncer instead.
 * TODO(crbug.com/985410): Replace all existing usages and remove this function.
 * Add an accessible message to the page that will be announced to
 * users who have spoken feedback on, but will be invisible to all
 * other users. It's removed right away so it doesn't clutter the DOM.
 * @param {string} msg The text to be pronounced.
 */
/* #export */ function announceAccessibleMessage(msg) {
  const element = document.createElement('div');
  element.setAttribute('aria-live', 'polite');
  element.style.position = 'fixed';
  element.style.left = '-9999px';
  element.style.height = '0px';
  element.innerText = msg;
  document.body.appendChild(element);
  window.setTimeout(function() {
    document.body.removeChild(element);
  }, 50);
}
// </if>

/**
 * @param {Node} el A node to search for ancestors with |className|.
 * @param {string} className A class to search for.
 * @return {Element} A node with class of |className| or null if none is found.
 */
/* #export */ function findAncestorByClass(el, className) {
  return /** @type {Element} */ (findAncestor(el, function(el) {
    return el.classList && el.classList.contains(className);
  }));
}

/**
 * Return the first ancestor for which the {@code predicate} returns true.
 * @param {Node} node The node to check.
 * @param {function(Node):boolean} predicate The function that tests the
 *     nodes.
 * @param {boolean=} includeShadowHosts
 * @return {Node} The found ancestor or null if not found.
 */
/* #export */ function findAncestor(node, predicate, includeShadowHosts) {
  while (node !== null) {
    if (predicate(node)) {
      break;
    }
    node = includeShadowHosts && node instanceof ShadowRoot ? node.host :
                                                              node.parentNode;
  }
  return node;
}

/**
 * Disables text selection and dragging, with optional callbacks to specify
 * overrides.
 * @param {function(Event):boolean=} opt_allowSelectStart Unless this function
 *    is defined and returns true, the onselectionstart event will be
 *    suppressed.
 * @param {function(Event):boolean=} opt_allowDragStart Unless this function
 *    is defined and returns true, the ondragstart event will be suppressed.
 */
/* #export */ function disableTextSelectAndDrag(
    opt_allowSelectStart, opt_allowDragStart) {
  // Disable text selection.
  document.onselectstart = function(e) {
    if (!(opt_allowSelectStart && opt_allowSelectStart.call(this, e))) {
      e.preventDefault();
    }
  };

  // Disable dragging.
  document.ondragstart = function(e) {
    if (!(opt_allowDragStart && opt_allowDragStart.call(this, e))) {
      e.preventDefault();
    }
  };
}

/**
 * Check the directionality of the page.
 * @return {boolean} True if Chrome is running an RTL UI.
 */
/* #export */ function isRTL() {
  return document.documentElement.dir === 'rtl';
}

/**
 * Get an element that's known to exist by its ID. We use this instead of just
 * calling getElementById and not checking the result because this lets us
 * satisfy the JSCompiler type system.
 * @param {string} id The identifier name.
 * @return {!HTMLElement} the Element.
 */
/* #export */ function getRequiredElement(id) {
  return assertInstanceof(
      $(id), HTMLElement, 'Missing required element: ' + id);
}

/**
 * Query an element that's known to exist by a selector. We use this instead of
 * just calling querySelector and not checking the result because this lets us
 * satisfy the JSCompiler type system.
 * @param {string} selectors CSS selectors to query the element.
 * @param {(!Document|!DocumentFragment|!Element)=} opt_context An optional
 *     context object for querySelector.
 * @return {!HTMLElement} the Element.
 */
/* #export */ function queryRequiredElement(selectors, opt_context) {
  const element = (opt_context || document).querySelector(selectors);
  return assertInstanceof(
      element, HTMLElement, 'Missing required element: ' + selectors);
}

/**
 * Creates a new URL which is the old URL with a GET param of key=value.
 * @param {string} url The base URL. There is not sanity checking on the URL so
 *     it must be passed in a proper format.
 * @param {string} key The key of the param.
 * @param {string} value The value of the param.
 * @return {string} The new URL.
 */
/* #export */ function appendParam(url, key, value) {
  const param = encodeURIComponent(key) + '=' + encodeURIComponent(value);

  if (url.indexOf('?') === -1) {
    return url + '?' + param;
  }
  return url + '&' + param;
}

/**
 * Creates an element of a specified type with a specified class name.
 * @param {string} type The node type.
 * @param {string} className The class name to use.
 * @return {Element} The created element.
 */
/* #export */ function createElementWithClassName(type, className) {
  const elm = document.createElement(type);
  elm.className = className;
  return elm;
}

/**
 * transitionend does not always fire (e.g. when animation is aborted
 * or when no paint happens during the animation). This function sets up
 * a timer and emulate the event if it is not fired when the timer expires.
 * @param {!HTMLElement} el The element to watch for transitionend.
 * @param {number=} opt_timeOut The maximum wait time in milliseconds for the
 *     transitionend to happen. If not specified, it is fetched from |el|
 *     using the transitionDuration style value.
 */
/* #export */ function ensureTransitionEndEvent(el, opt_timeOut) {
  if (opt_timeOut === undefined) {
    const style = getComputedStyle(el);
    opt_timeOut = parseFloat(style.transitionDuration) * 1000;

    // Give an additional 50ms buffer for the animation to complete.
    opt_timeOut += 50;
  }

  let fired = false;
  el.addEventListener('transitionend', function f(e) {
    el.removeEventListener('transitionend', f);
    fired = true;
  });
  window.setTimeout(function() {
    if (!fired) {
      cr.dispatchSimpleEvent(el, 'transitionend', true);
    }
  }, opt_timeOut);
}

/**
 * Alias for document.scrollTop getter.
 * @param {!HTMLDocument} doc The document node where information will be
 *     queried from.
 * @return {number} The Y document scroll offset.
 */
/* #export */ function scrollTopForDocument(doc) {
  return doc.documentElement.scrollTop || doc.body.scrollTop;
}

/**
 * Alias for document.scrollTop setter.
 * @param {!HTMLDocument} doc The document node where information will be
 *     queried from.
 * @param {number} value The target Y scroll offset.
 */
/* #export */ function setScrollTopForDocument(doc, value) {
  doc.documentElement.scrollTop = doc.body.scrollTop = value;
}

/**
 * Alias for document.scrollLeft getter.
 * @param {!HTMLDocument} doc The document node where information will be
 *     queried from.
 * @return {number} The X document scroll offset.
 */
/* #export */ function scrollLeftForDocument(doc) {
  return doc.documentElement.scrollLeft || doc.body.scrollLeft;
}

/**
 * Alias for document.scrollLeft setter.
 * @param {!HTMLDocument} doc The document node where information will be
 *     queried from.
 * @param {number} value The target X scroll offset.
 */
/* #export */ function setScrollLeftForDocument(doc, value) {
  doc.documentElement.scrollLeft = doc.body.scrollLeft = value;
}

/**
 * Replaces '&', '<', '>', '"', and ''' characters with their HTML encoding.
 * @param {string} original The original string.
 * @return {string} The string with all the characters mentioned above replaced.
 */
/* #export */ function HTMLEscape(original) {
  return original.replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
}

/**
 * Shortens the provided string (if necessary) to a string of length at most
 * |maxLength|.
 * @param {string} original The original string.
 * @param {number} maxLength The maximum length allowed for the string.
 * @return {string} The original string if its length does not exceed
 *     |maxLength|. Otherwise the first |maxLength| - 1 characters with '...'
 *     appended.
 */
/* #export */ function elide(original, maxLength) {
  if (original.length <= maxLength) {
    return original;
  }
  return original.substring(0, maxLength - 1) + '\u2026';
}

/**
 * Quote a string so it can be used in a regular expression.
 * @param {string} str The source string.
 * @return {string} The escaped string.
 */
/* #export */ function quoteString(str) {
  return str.replace(/([\\\.\+\*\?\[\^\]\$\(\)\{\}\=\!\<\>\|\:])/g, '\\$1');
}

/**
 * Calls |callback| and stops listening the first time any event in |eventNames|
 * is triggered on |target|.
 * @param {!EventTarget} target
 * @param {!Array<string>|string} eventNames Array or space-delimited string of
 *     event names to listen to (e.g. 'click mousedown').
 * @param {function(!Event)} callback Called at most once. The
 *     optional return value is passed on by the listener.
 */
/* #export */ function listenOnce(target, eventNames, callback) {
  if (!Array.isArray(eventNames)) {
    eventNames = eventNames.split(/ +/);
  }

  const removeAllAndCallCallback = function(event) {
    eventNames.forEach(function(eventName) {
      target.removeEventListener(eventName, removeAllAndCallCallback, false);
    });
    return callback(event);
  };

  eventNames.forEach(function(eventName) {
    target.addEventListener(eventName, removeAllAndCallCallback, false);
  });
}

/**
 * @param {!Event} e
 * @return {boolean} Whether a modifier key was down when processing |e|.
 */
/* #export */ function hasKeyModifiers(e) {
  return !!(e.altKey || e.ctrlKey || e.metaKey || e.shiftKey);
}

/**
 * @param {!Element} el
 * @return {boolean} Whether the element is interactive via text input.
 */
/* #export */ function isTextInputElement(el) {
  return el.tagName === 'INPUT' || el.tagName === 'TEXTAREA';
}

/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');
