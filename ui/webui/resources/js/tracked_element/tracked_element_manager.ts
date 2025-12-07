// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Allows a WebUI page to report the visibility and bounds of HTML elements to
 * the browser process. This is the frontend counterpart to
 * ui::TrackedElementWebUI.
 *
 * The primary use case is to anchor secondary UIs (e.g. App Menu) to a HTML
 * element in WebUI.
 * TODO(crbug.com/40243115): Use TrackedElementManager in Help Bubbles.
 *
 * ## Usage
 *
 * In C++, declare your ui::ElementIdentifier. Make sure it is registered as a
 * known identifier.
 *
 * ```C++
 * DECLARE_ELEMENT_IDENTIFIER_VALUE(kMyElementIdentifier);
 * // TODO(crbug.com/40243115): explain how to register as a known identifier.
 * ```
 *
 * In your WebUI component:
 *
 * 1.  Get the singleton instance of `TrackedElementManager` in your component
 *     class.
 *
 *     ```ts
 *     // in your component class:
 *     private trackedElementManager: TrackedElementManager;
 *
 *     constructor() {
 *       super();
 *       this.trackedElementManager = TrackedElementManager.getInstance();
 *       // ...
 *     }
 *     ```
 *
 * 2.  Call `startTracking()` to begin tracking an element. You probably want
 *     to do this in `connectedCallback()`.
 *
 *     ```ts
 *     override connectedCallback() {
 *       super.connectedCallback();
 *     this.trackedElementManager_.startTracking(
 *         this.$.myElement,
 *         'kMyElementIdentifier',
 *         {...options});
 *     ```
 *
 *     The first parameter is the HTMLElement to track. The second is the
 *     string identifier name that C++ uses. The third is an optional
 *     options object. See `Options` in this file.
 *
 * 3.  Call `stopTracking()` to stop tracking an element.
 *
 *     ```ts
 *     this.trackedElementManager_.stopTracking(this.$.myElement);
 *     ```
 *
 * 4.  To report that the user has activated an element (e.g. by clicking on
 *     it), call `notifyElementActivated()`.
 *
 *     ```ts
 *     this.trackedElementManager_.notifyElementActivated(this.$.myElement);
 *     ```
 *
 * 5.  To report a custom event, call `notifyCustomEvent()`.
 *
 *     ```ts
 *     this.trackedElementManager_.notifyCustomEvent(
 *         this.$.myElement,
 *         'my-custom-event-name');
 *     ```
 */

import type {InsetsF, RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {TrackedElementHandlerInterface} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';

import {assert} from '../assert.js';
import {debounceEnd} from '../util.js';

import {TrackedElementProxyImpl} from './tracked_element_proxy.js';

/**
 * Options for `TrackedElementManager.startTracking()`.
 */
export interface Options {
  /**
   * Padding added to the element bounds.
   * These values are clamped in the range [0, 20].
   */
  paddingTop?: number;
  paddingLeft?: number;
  paddingBottom?: number;
  paddingRight?: number;
  /**
   * Set this to true if the element is fixed positioned.
   * By default, this class detects tracked elements when they are rendered
   * within the document. This breaks with fix-positioned elements since they
   * are not in the regular flow of the document but they are always visible.
   */
  fixed?: boolean;
}

interface TrackedElement {
  element: HTMLElement;
  nativeId: string;
  padding: InsetsF;
  fixed: boolean;
  visible: boolean;
  bounds: RectF;
  onVisibilityChanged?: (visible: boolean, bounds: RectF) => void;
}

function parseOptions(options?: Options) {
  if (!options) {
    return {
      padding: {top: 0, bottom: 0, left: 0, right: 0},
      fixed: false,
    };
  }

  const padding: InsetsF = {top: 0, bottom: 0, left: 0, right: 0};
  padding.top = clampPadding(options.paddingTop);
  padding.left = clampPadding(options.paddingLeft);
  padding.bottom = clampPadding(options.paddingBottom);
  padding.right = clampPadding(options.paddingRight);
  return {
    padding,
    fixed: !!options.fixed,
  };
}

function clampPadding(n: number = 0) {
  return Math.max(0, Math.min(20, n));
}

function computeIsVisible(element: Element): boolean {
  const rect = element.getBoundingClientRect();
  return rect.height > 0 && rect.width > 0;
}

export class TrackedElementManager {
  private static instance_: TrackedElementManager|null = null;

  static getInstance(): TrackedElementManager {
    if (TrackedElementManager.instance_ === null) {
      TrackedElementManager.instance_ = new TrackedElementManager();
    }
    return TrackedElementManager.instance_;
  }

  private trackedElementHandler_: TrackedElementHandlerInterface;
  private trackedElements_: Map<HTMLElement, TrackedElement> = new Map();
  private fixedElementObserver_: IntersectionObserver;
  private resizeObserver_: ResizeObserver;
  private debouncedUpdateAllBoundsCallback_: () => void;

  private constructor() {
    this.trackedElementHandler_ =
        TrackedElementProxyImpl.getInstance().getHandler();

    this.debouncedUpdateAllBoundsCallback_ =
        debounceEnd(this.updateAllBounds_.bind(this), 50);

    this.resizeObserver_ =
        new ResizeObserver(entries => entries.forEach(({target}) => {
          if (target === document.body) {
            this.debouncedUpdateAllBoundsCallback_();
          } else {
            this.onElementVisibilityChanged_(
                target as HTMLElement, computeIsVisible(target));
          }
        }));
    this.fixedElementObserver_ = new IntersectionObserver(
        entries => entries.forEach(
            ({target, isIntersecting}) => this.onElementVisibilityChanged_(
                target as HTMLElement, isIntersecting)),
        {root: null});

    document.addEventListener(
        'scroll', this.debouncedUpdateAllBoundsCallback_, {passive: true});
    this.resizeObserver_.observe(document.body);
  }

  reset() {
    this.resizeObserver_.disconnect();
    this.fixedElementObserver_.disconnect();
    document.removeEventListener(
        'scroll', this.debouncedUpdateAllBoundsCallback_);
    this.trackedElements_.clear();
  }

  /**
   * Starts tracking an element.
   * A visibility update event will be sent immediately.
   *
   * @param element The element to track.
   * @param nativeId The ElementIdentifier name that C++ uses.
   * @param options Optional options. See `Options` in this file.
   * @param onVisibilityChanged Optional callback that is called when the
   *     visibility of the element changes. The callback is called with two
   *     parameters:
   *       - visible: Whether the element is visible.
   *       - bounds: The bounds of the element.
   */
  startTracking(
      element: HTMLElement, nativeId: string, options?: Options,
      onVisibilityChanged?: (visible: boolean, bounds: RectF) => void) {
    element.dataset['nativeId'] = nativeId;

    // Remove tracking of the old element before registering the nativeId to a
    // new element.
    if (this.trackedElements_.has(element)) {
      this.stopTracking(element);
    }

    const parsedOptions = parseOptions(options);
    const trackedElement: TrackedElement = {
      element,
      nativeId,
      padding: parsedOptions.padding,
      fixed: parsedOptions.fixed,
      visible: false,
      bounds: {x: 0, y: 0, width: 0, height: 0},
      onVisibilityChanged,
    };
    this.trackedElements_.set(element, trackedElement);

    if (trackedElement.fixed) {
      this.fixedElementObserver_.observe(element);
    } else {
      this.resizeObserver_.observe(element);
    }
  }

  /**
   * Stops tracking an element.
   * A visibility event with `visible: false` will be sent immediately.
   *
   * @param element The element to stop tracking.
   */
  stopTracking(element: HTMLElement) {
    const trackedElement = this.trackedElements_.get(element);
    if (!trackedElement) {
      return;
    }

    this.onElementVisibilityChanged_(element, false);
    if (trackedElement.fixed) {
      this.fixedElementObserver_.unobserve(element);
    } else {
      this.resizeObserver_.unobserve(element);
    }
    this.trackedElements_.delete(element);

    element.dataset['nativeId'] = '';
  }

  notifyElementActivated(element: HTMLElement) {
    const nativeId = element.dataset['nativeId'];
    assert(nativeId);
    this.trackedElementHandler_.trackedElementActivated(nativeId);
  }

  notifyCustomEvent(element: HTMLElement, customEventName: string) {
    const nativeId = element.dataset['nativeId'];
    assert(nativeId);
    this.trackedElementHandler_.trackedElementCustomEvent(
        nativeId, customEventName);
  }

  private onElementVisibilityChanged_(
      element: HTMLElement, isVisible: boolean) {
    const trackedElement = this.trackedElements_.get(element);
    assert(trackedElement);

    const bounds: RectF = isVisible ? this.getElementBounds_(element) :
                                      {x: 0, y: 0, width: 0, height: 0};

    if (trackedElement.onVisibilityChanged) {
      trackedElement.onVisibilityChanged(isVisible, bounds);
    }

    trackedElement.visible = isVisible;
    trackedElement.bounds = bounds;
    this.trackedElementHandler_.trackedElementVisibilityChanged(
        trackedElement.nativeId, isVisible, bounds);
  }

  private updateAllBounds_() {
    this.trackedElements_.forEach((_, element) => {
      this.onElementVisibilityChanged_(element, computeIsVisible(element));
    });
  }

  private getElementBounds_(element: HTMLElement): RectF {
    const rect: RectF = {x: 0, y: 0, width: 0, height: 0};
    const bounds = element.getBoundingClientRect();
    rect.x = bounds.x;
    rect.y = bounds.y;
    rect.width = bounds.width;
    rect.height = bounds.height;

    const trackedElement = this.trackedElements_.get(element);
    if (trackedElement) {
      const padding = trackedElement.padding;
      rect.x -= padding.left;
      rect.y -= padding.top;
      rect.width += padding.left + padding.right;
      rect.height += padding.top + padding.bottom;
    }
    return rect;
  }
}
