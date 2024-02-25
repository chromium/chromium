// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';

/**
 * Alias for document.getElementById. Found elements must be HTMLElements.
 */
export function $<T extends HTMLElement = HTMLElement>(id: string): (T|null) {
  const el = document.querySelector<T>(`#${id}`);
  if (el) {
    assert(el instanceof HTMLElement);
    return el;
  }
  return null;
}

/**
 * Alias for document.getElementById. Found elements must be HTMLElements.
 */
export function getRequiredElement<T extends HTMLElement = HTMLElement>(
    id: string): T {
  const el = document.querySelector<T>(`#${id}`);
  assert(el);
  assert(el instanceof HTMLElement);
  return el;
}

/**
 * @return The currently focused element (including elements that are
 *     behind a shadow root), or null if nothing is focused.
 */
export function getDeepActiveElement(): (Element|null) {
  let a = document.activeElement;
  while (a && a.shadowRoot && a.shadowRoot.activeElement) {
    a = a.shadowRoot.activeElement;
  }
  return a;
}

/**
 * Check the directionality of the page.
 * @return True if Chrome is running an RTL UI.
 */
export function isRTL(): boolean {
  return document.documentElement.dir === 'rtl';
}

/**
 * Creates a new URL which is the old URL with a GET param of key=value.
 * @param url The base URL. There is no validation checking on the URL
 *     so it must be passed in a proper format.
 * @param key The key of the param.
 * @param value The value of the param.
 * @return The new URL.
 */
export function appendParam(url: string, key: string, value: string): string {
  const param = encodeURIComponent(key) + '=' + encodeURIComponent(value);

  if (url.indexOf('?') === -1) {
    return url + '?' + param;
  }
  return url + '&' + param;
}

/**
 * transitionend does not always fire (e.g. when animation is aborted
 * or when no paint happens during the animation). This function sets up
 * a timer and emulate the event if it is not fired when the timer expires.
 * @param el The element to watch for transitionend.
 * @param timeOut The maximum wait time in milliseconds for the transitionend
 *     to happen. If not specified, it is fetched from |el| using the
 *     transitionDuration style value.
 */
export function ensureTransitionEndEvent(
    el: HTMLElement, timeOut: number): void {
  if (timeOut === undefined) {
    const style = getComputedStyle(el);
    timeOut = parseFloat(style.transitionDuration) * 1000;

    // Give an additional 50ms buffer for the animation to complete.
    timeOut += 50;
  }

  let fired = false;
  el.addEventListener('transitionend', function f() {
    el.removeEventListener('transitionend', f);
    fired = true;
  });
  window.setTimeout(function() {
    if (!fired) {
      el.dispatchEvent(
          new CustomEvent('transitionend', {bubbles: true, composed: true}));
    }
  }, timeOut);
}

/**
 * Replaces '&', '<', '>', '"', and ''' characters with their HTML encoding.
 * @param original The original string.
 * @return The string with all the characters mentioned above replaced.
 */
export function htmlEscape(original: string): string {
  return original.replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
}

/**
 * Quote a string so it can be used in a regular expression.
 * @param str The source string.
 * @return The escaped string.
 */
export function quoteString(str: string): string {
  return str.replace(/([\\\.\+\*\?\[\^\]\$\(\)\{\}\=\!\<\>\|\:])/g, '\\$1');
}

/**
 * Calls |callback| and stops listening the first time any event in |eventNames|
 * is triggered on |target|.
 * @param eventNames Array or space-delimited string of event names to listen to
 *     (e.g. 'click mousedown').
 * @param callback Called at most once. The optional return value is passed on
 *     by the listener.
 */
export function listenOnce(
    target: EventTarget, eventNames: string[]|string,
    callback: (e: Event) => any) {
  const eventNamesArray: string[] = Array.isArray(eventNames) ?
      eventNames as string[] :
      (eventNames as string).split(/ +/);

  const removeAllAndCallCallback = function(event: Event) {
    eventNamesArray.forEach(function(eventName: string) {
      target.removeEventListener(eventName, removeAllAndCallCallback, false);
    });
    return callback(event);
  };

  eventNamesArray.forEach(function(eventName: string) {
    target.addEventListener(eventName, removeAllAndCallCallback, false);
  });
}

/**
 * @return Whether a modifier key was down when processing |e|.
 */
export function hasKeyModifiers(e: KeyboardEvent): boolean {
  return !!(e.altKey || e.ctrlKey || e.metaKey || e.shiftKey);
}

/**
 * @return Whether a given KeyboardEvent resembles an undo action, on different
 * platforms.
 */

export function isUndoKeyboardEvent(event: KeyboardEvent): boolean {
  if (event.key !== 'z') {
    return false;
  }
  const excludedModifiers = [
    event.altKey, event.shiftKey,
    // <if expr="is_macosx">
    event.ctrlKey,
    // </if>
    // <if expr="not is_macosx">
    event.metaKey,
    // </if>
  ];

  let targetModifier = event.ctrlKey;
  // <if expr="is_macosx">
  targetModifier = event.metaKey;
  // </if>

  return targetModifier && !excludedModifiers.some(modifier => modifier);
}
