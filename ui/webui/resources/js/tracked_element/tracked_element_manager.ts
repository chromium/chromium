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
 * ## Change Detection
 *
 * This manager detects element position/visibility changes through:
 * - ResizeObserver: Detects size changes of tracked elements and viewport
 *   resizes (document.body). Note: Does NOT detect pure position changes.
 * - IntersectionObserver: Detects viewport intersection for fixed elements
 * - MutationObserver: Used to detect when tracked elements are moved in the
 *   DOM, or when their 'style' or 'class' attributes change (position changes)
 * - Scroll events: Detects document scrolling (position changes)
 *
 * ### Known Limitations
 *
 * The following changes will NOT be detected:
 * - Style/class attribute changes on parent or ancestor elements
 * - Parent/ancestor elements being moved in the DOM
 * - Direct CSS rule modifications via CSSOM (e.g., modifying
 *   document.styleSheets or adding/removing <style> elements)
 * - Position changes caused by other elements being added/removed nearby
 *
 * Note: Viewport resizes and media query changes triggered by resizing
 * are detected via the document.body ResizeObserver.
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
import {TextEntryMode} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import type {TrackedElementHandlerInterface, TrackedElementIdentifier} from '//resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';

import {assert} from '../assert.js';
import {debounceEnd} from '../util.js';

import {TrackedElementProxyImpl} from './tracked_element_proxy.js';

/**
 * Event type triggered when a tracked element's visibility changes.
 */
export const TRACKED_ELEMENT_VISIBILITY_CHANGED_EVENT =
    'tracked-element-visibility-changed';

/**
 * Provides the current state of a tracked element.
 */
export interface TrackedElement {
  // The element itself.
  element: HTMLElement;
  // Is the element visible?
  visible: boolean;
  // The element's bounds in the viewport.
  bounds: RectF;
}

/**
 * Event type when an element's visibility changes; used when observing all
 * elements with a particular native ID.
 */
export type TrackedElementVisibilityChangedEvent = CustomEvent<TrackedElement>;

/**
 * Callback when observing visibility changes on a specific element.
 */
export type TrackedElementVisibilityChangedCallback =
    (update: TrackedElement) => void;

/**
 * Callback when an element's highlight state changes.
 */
export type HighlightChangedCallback =
    (highlighted: boolean, element: HTMLElement) => void;

/**
 * Options for `TrackedElementManager.startTracking()`.
 */
export interface Options {
  /**
   * If set, explicitly specifies a secondary ID for the element.
   */
  secondaryId?: string;

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

  /**
   * If this is set, this element will be marked as supporting anchor
   * highlighting, and the method will be invoked when the highlight state
   * (initially false) changes.
   */
  onHighlightChanged?: (highlighted: boolean) => void;
}

interface TrackedElementData {
  element: HTMLElement;
  nativeId: string;
  secondaryId: string;
  padding: InsetsF;
  fixed: boolean;
  visible: boolean;
  bounds: RectF;
  onVisibilityChanged?: TrackedElementVisibilityChangedCallback;
  onHighlightChanged?: HighlightChangedCallback;
}

/**
 * Holds all data for a particular native ID, including individual
 * `TrackedElement`s and native-id-wide visibility callbacks.
 */
class ElementData {
  elements: Map<string, TrackedElementData> = new Map();

  /**
   * Callbacks are routed through an `EventTarget` to avoid concurrency and
   * re-entry issues.
   */
  eventTarget = new EventTarget();
}

const NATIVE_ELEMENT_IDENTIFIER_KEY = 'nativeId';
const SECONDARY_ELEMENT_IDENTIFIER_KEY = 'secondaryId';

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

  static setInstance(instance: TrackedElementManager|null) {
    TrackedElementManager.instance_ = instance;
  }

  private trackedElementHandler_: TrackedElementHandlerInterface;

  // Mapped from native ID.
  private trackedElements_: Map<string, ElementData> = new Map();
  private fixedElementObserver_: IntersectionObserver;
  private resizeObserver_: ResizeObserver;
  // Observes attribute changes (style/class) on tracked elements.
  private attributeMutationObserver_: MutationObserver;
  // Observes document subtree for detached elements being added to DOM.
  private documentMutationObserver_: MutationObserver;
  private debouncedUpdateAllBoundsCallback_: () => void;

  private constructor() {
    this.trackedElementHandler_ =
        TrackedElementProxyImpl.getInstance().getHandler();
    const callbackRouter = TrackedElementProxyImpl.getInstance().callbackRouter;
    this.trackedElementHandler_.setManager(
        callbackRouter.$.bindNewPipeAndPassRemote());
    callbackRouter.onElementHighlightChanged.addListener(
        this.onElementHighlightChanged_.bind(this));
    callbackRouter.clickElement.addListener(this.clickElement_.bind(this));
    callbackRouter.focusElement.addListener(this.focusElement_.bind(this));
    callbackRouter.selectTab.addListener(this.selectTab_.bind(this));
    callbackRouter.selectDropdownItem.addListener(
        this.selectDropdownItem_.bind(this));
    callbackRouter.enterText.addListener(this.enterText_.bind(this));
    callbackRouter.confirm.addListener(this.confirm_.bind(this));

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

    // Observer for attribute changes on tracked elements.
    this.attributeMutationObserver_ = new MutationObserver(mutations => {
      for (const mutation of mutations) {
        // Style or class attribute changed on a tracked element.
        const target = mutation.target as HTMLElement;
        if (this.getDataForElement_(target)) {
          this.onElementVisibilityChanged_(target, computeIsVisible(target));
        }
      }
    });

    // Helper to check if a node or its descendants are tracked elements.
    const checkTrackedNodes = (nodes: NodeList) => {
      nodes.forEach(node => {
        if (node instanceof HTMLElement) {
          // Check if the node is a tracked element.
          if (this.getDataForElement_(node)) {
            this.onElementVisibilityChanged_(node, computeIsVisible(node));
          }
          // Check if any descendants are tracked elements.
          node.querySelectorAll('*').forEach(descendant => {
            if (descendant instanceof HTMLElement &&
                this.getDataForElement_(descendant)) {
              this.onElementVisibilityChanged_(
                  descendant, computeIsVisible(descendant));
            }
          });
        }
      });
    };

    // Observer for document-level changes to catch tracked elements being
    // added to or removed from the DOM tree.
    this.documentMutationObserver_ = new MutationObserver(mutations => {
      for (const mutation of mutations) {
        checkTrackedNodes(mutation.removedNodes);
        checkTrackedNodes(mutation.addedNodes);
      }
    });

    document.addEventListener(
        'scroll', this.debouncedUpdateAllBoundsCallback_, {passive: true});
    this.resizeObserver_.observe(document.body);
    // Observe the entire document to catch detached elements being added.
    this.documentMutationObserver_.observe(
        document, {childList: true, subtree: true});
  }

  static getElementId(element: HTMLElement): TrackedElementIdentifier
      |undefined {
    const nativeIdentifier = element.dataset[NATIVE_ELEMENT_IDENTIFIER_KEY];
    const secondaryIdentifier =
        element.dataset[SECONDARY_ELEMENT_IDENTIFIER_KEY];
    if (!nativeIdentifier) {
      return undefined;
    }
    assert(
        secondaryIdentifier,
        'Element has native identifier ' + nativeIdentifier +
            ' but no secondary id.');
    return {nativeIdentifier, secondaryIdentifier};
  }

  private getDataForElement_(element: HTMLElement): TrackedElementData
      |undefined {
    const id = TrackedElementManager.getElementId(element);
    if (!id) {
      return undefined;
    }
    const maybeTrackedElement = this.trackedElements_.get(id.nativeIdentifier)
                                    ?.elements.get(id.secondaryIdentifier);
    if (!maybeTrackedElement) {
      return undefined;
    }
    assert(
        maybeTrackedElement.element === element,
        `Found different element with same native (${
            id.nativeIdentifier}) and secondary (${
            id.secondaryIdentifier}) ids!`);
    return maybeTrackedElement;
  }

  private getDataForId_(id: TrackedElementIdentifier): TrackedElementData
      |undefined {
    const nativeId = id.nativeIdentifier;
    const secondaryId = id.secondaryIdentifier;
    if (!nativeId || !secondaryId) {
      return undefined;
    }
    return this.trackedElements_.get(nativeId)?.elements.get(secondaryId);
  }

  getElementFor(element: HTMLElement): TrackedElement|undefined {
    const id = TrackedElementManager.getElementId(element);
    return id ? this.getElementWithId(id) : undefined;
  }

  getElementWithId(id: TrackedElementIdentifier, visibleOnly: boolean = false):
      TrackedElement|undefined {
    const el = this.getDataForId_(id);
    if (!el || (visibleOnly && !el.visible)) {
      return undefined;
    }
    return {element: el.element, visible: el.visible, bounds: el.bounds};
  }

  getAllElementsWithNativeId(
      nativeIdentifier: string,
      visibleOnly: boolean = false): TrackedElement[] {
    const result = [];
    const data = this.trackedElements_.get(nativeIdentifier);
    if (data) {
      for (const el of data.elements.values()) {
        if (!visibleOnly || el.visible) {
          result.push(
              {element: el.element, visible: el.visible, bounds: el.bounds});
        }
      }
    }
    return result;
  }

  private static idToString_(id: TrackedElementIdentifier) {
    return id.nativeIdentifier + ' - ' + id.secondaryIdentifier;
  }

  reset() {
    this.resizeObserver_.disconnect();
    this.fixedElementObserver_.disconnect();
    this.attributeMutationObserver_.disconnect();
    this.documentMutationObserver_.disconnect();
    document.removeEventListener(
        'scroll', this.debouncedUpdateAllBoundsCallback_);
    this.trackedElements_.clear();

    // Reconnect global observers after clearing.
    this.resizeObserver_.observe(document.body);
    this.documentMutationObserver_.observe(
        document, {childList: true, subtree: true});
  }

  /**
   * Starts tracking an element.
   * A visibility update event will be sent immediately.
   *
   * @param element The element to track.
   * @param nativeId The ElementIdentifier name that C++ uses.
   * @param options Optional options. See `Options` in this file.
   * @param onVisibilityChanged Optional callback for visibility changes for
   *     `element`.
   *
   * Note that `onVisibilityChanged` will only receive updates for `element` and
   * an initial visibility callback will be sent. Contrast this with adding a
   * listener to the result of calling `getVisibilityEventTarget(nativeId)`,
   * which will receive all future events for elements with `nativeId` (and only
   * future events).
   */
  startTracking(
      element: HTMLElement, nativeId: string, options?: Options,
      onVisibilityChanged?: TrackedElementVisibilityChangedCallback) {
    // Remove tracking of the old element before registering the nativeId to a
    // new element.
    if (this.getDataForElement_(element)) {
      this.stopTracking(element);
    }
    const secondaryId = options?.secondaryId ||
        TrackedElementProxyImpl.getAutoGeneratedSecondaryId();
    element.dataset[NATIVE_ELEMENT_IDENTIFIER_KEY] = nativeId;
    element.dataset[SECONDARY_ELEMENT_IDENTIFIER_KEY] = secondaryId;

    const parsedOptions = parseOptions(options);
    const initialVisible = computeIsVisible(element);
    const trackedElement: TrackedElementData = {
      element,
      nativeId,
      secondaryId,
      padding: parsedOptions.padding,
      fixed: parsedOptions.fixed,
      visible: initialVisible,
      bounds: {x: 0, y: 0, width: 0, height: 0},
      onVisibilityChanged: onVisibilityChanged,
      onHighlightChanged: options?.onHighlightChanged,
    };

    if (!this.trackedElements_.has(nativeId)) {
      this.trackedElements_.set(nativeId, new ElementData());
    }
    const data = this.trackedElements_.get(nativeId)!;
    assert(
        !data.elements.has(secondaryId),
        'Attempted to track an element with native id ' + nativeId +
            ' twice, or two elements with conflicting secondary id ' +
            secondaryId);
    data.elements.set(secondaryId, trackedElement);

    if (trackedElement.fixed) {
      this.fixedElementObserver_.observe(element);
    } else {
      this.resizeObserver_.observe(element);
    }

    // Observe the element itself for style/class changes that affect position.
    this.attributeMutationObserver_.observe(element, {
      attributes: true,
      attributeFilter: ['style', 'class', 'hidden'],
    });

    if (options?.onHighlightChanged) {
      this.trackedElementHandler_.trackedElementCanHighlightChanged(
          TrackedElementManager.elementToIdentifier_(trackedElement), true);
    }

    this.onElementVisibilityChanged_(element, initialVisible);
  }

  /**
   * Gets the event target for visibility for a native id. Listeners will only
   * receive events of type `TrackedElementVisibilityChangedEvent`.
   *
   * @param nativeId the native id.
   *
   * @returns the event target
   */
  getVisibilityEventTarget(nativeId: string): EventTarget {
    if (!this.trackedElements_.has(nativeId)) {
      this.trackedElements_.set(nativeId, new ElementData());
    }
    return this.trackedElements_.get(nativeId)!.eventTarget;
  }

  /**
   * Stops tracking an element.
   * A visibility event with `visible: false` will be sent immediately.
   *
   * @param element The element to stop tracking.
   */
  stopTracking(element: HTMLElement) {
    const trackedElement = this.getDataForElement_(element);
    if (!trackedElement) {
      return;
    }

    if (trackedElement.onHighlightChanged) {
      this.trackedElementHandler_.trackedElementCanHighlightChanged(
          TrackedElementManager.elementToIdentifier_(trackedElement), false);
    }
    this.onElementVisibilityChanged_(element, false);
    if (trackedElement.fixed) {
      this.fixedElementObserver_.unobserve(element);
    } else {
      this.resizeObserver_.unobserve(element);
    }

    // Note: MutationObservers don't have unobserve(). The
    // attributeMutationObserver_ and documentMutationObserver_ will still be
    // observing, but since the element is no longer in trackedElements_,
    // callbacks won't trigger.
    this.trackedElements_.get(trackedElement.nativeId)
        ?.elements.delete(trackedElement.secondaryId);

    delete element.dataset[NATIVE_ELEMENT_IDENTIFIER_KEY];
    delete element.dataset[SECONDARY_ELEMENT_IDENTIFIER_KEY];
  }

  notifyElementActivated(element: HTMLElement) {
    const el = this.getDataForElement_(element);
    assert(el);
    this.trackedElementHandler_.trackedElementActivated(
        TrackedElementManager.elementToIdentifier_(el));
  }

  notifyCustomEvent(element: HTMLElement, customEventName: string) {
    const el = this.getDataForElement_(element);
    assert(el);
    this.trackedElementHandler_.trackedElementCustomEvent(
        TrackedElementManager.elementToIdentifier_(el), customEventName);
  }

  private onElementVisibilityChanged_(element: HTMLElement, visible: boolean) {
    const trackedElement = this.getDataForElement_(element);
    if (!trackedElement) {
      // When we stop tracking an element we continue to get events for it. Just
      // ignore these events.
      return;
    }

    const bounds: RectF = visible ? this.getElementBounds_(element) :
                                    {x: 0, y: 0, width: 0, height: 0};


    const update = {visible, bounds, element};

    // Invoke specific visibility changed event.
    if (trackedElement.onVisibilityChanged) {
      trackedElement.onVisibilityChanged(update);
    }

    // Invoke general visibility changed events.
    this.trackedElements_.get(trackedElement.nativeId)!.eventTarget
        .dispatchEvent(new CustomEvent(
            TRACKED_ELEMENT_VISIBILITY_CHANGED_EVENT, {detail: update}));

    const wasVisible = trackedElement.visible;
    trackedElement.visible = visible;
    trackedElement.bounds = bounds;
    this.trackedElementHandler_.trackedElementVisibilityChanged(
        TrackedElementManager.elementToIdentifier_(trackedElement), visible,
        bounds);

    if (visible && !wasVisible && trackedElement.onHighlightChanged) {
      // The C++ tracker drops its state when it is destroyed and recreated
      // during a visibility bounce (e.g., from a 0x0 size during a CSS
      // animation or variable evaluation). We must explicitly restore the
      // highlight capability on the newly recreated C++ tracking object.
      this.trackedElementHandler_.trackedElementCanHighlightChanged(
          TrackedElementManager.elementToIdentifier_(trackedElement), true);
    }
  }

  private updateAllBounds_() {
    this.trackedElements_.forEach((elementData, _) => {
      elementData.elements.forEach((trackedElement, _) => {
        const element = trackedElement.element;
        this.onElementVisibilityChanged_(element, computeIsVisible(element));
      });
    });
  }

  private getElementBounds_(element: HTMLElement): RectF {
    const rect: RectF = {x: 0, y: 0, width: 0, height: 0};
    const bounds = element.getBoundingClientRect();
    rect.x = bounds.x;
    rect.y = bounds.y;
    rect.width = bounds.width;
    rect.height = bounds.height;

    const trackedElement = this.getDataForElement_(element);
    if (trackedElement) {
      const padding = trackedElement.padding;
      rect.x -= padding.left;
      rect.y -= padding.top;
      rect.width += padding.left + padding.right;
      rect.height += padding.top + padding.bottom;
    }
    return rect;
  }

  /* Called from browser to add/remove highlights. */
  private onElementHighlightChanged_(
      id: TrackedElementIdentifier, highlighted: boolean) {
    const trackedElement = this.getDataForId_(id);
    const maybeCallback = trackedElement?.onHighlightChanged;
    if (maybeCallback) {
      maybeCallback(highlighted, trackedElement.element);
    }
  }

  private async waitUntilNotDisabled_(
      element: HTMLElement, id: TrackedElementIdentifier): Promise<void> {
    if (!element.hasAttribute('disabled')) {
      return;
    }

    console.info(
        `TrackedElementManager: Element ${
            TrackedElementManager.idToString_(id)} is disabled, ` +
        `waiting...`);

    return new Promise((resolve) => {
      const observer = new MutationObserver(() => {
        if (!element.hasAttribute('disabled')) {
          observer.disconnect();
          console.info(
              `TrackedElementManager: Element ${
                  TrackedElementManager.idToString_(id)} is no ` +
              `longer disabled.`);
          resolve();
        }
      });
      observer.observe(
          element, {attributes: true, attributeFilter: ['disabled']});
    });
  }

  private async clickElement_(id: TrackedElementIdentifier):
      Promise<{success: boolean}> {
    const trackedElement = this.getDataForId_(id);
    if (!trackedElement) {
      console.error(`TrackedElementManager: Click failed, element not found: ${
          TrackedElementManager.idToString_(id)}`);
      return {success: false};
    }

    let target = trackedElement.element;

    // If the element is a container with a shadow root, try to find the actual
    // interactive element inside.
    if (target.shadowRoot &&
        !['BUTTON', 'INPUT', 'A', 'SELECT'].includes(target.tagName)) {
      const inner = target.shadowRoot.querySelector(
          'button, [role="button"], cr-icon-button, cr-button');
      if (inner) {
        target = inner as HTMLElement;
      }
    }

    await this.waitUntilNotDisabled_(target, id);

    // Some components (like the reload button) listen to pointer events
    // instead of click. We also need to fake pointer capture for some tests.
    const oldPointerCapture = {
      setPointerCapture: target.setPointerCapture,
      hasPointerCapture: target.hasPointerCapture,
      releasePointerCapture: target.releasePointerCapture,
    };
    {
      let hasCapture: number|null = null;
      target.setPointerCapture = (id) => {
        hasCapture = id;
      };
      target.hasPointerCapture = (id) => {
        return id === hasCapture;
      };
      target.releasePointerCapture = (id) => {
        if (id === hasCapture) {
          hasCapture = null;
        }
      };
    }
    const bounds = target.getBoundingClientRect();
    target.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      composed: true,
      button: 0,  // Left
      pointerId: 1,
      isPrimary: true,
      buttons: 1,
      clientX: bounds.left + bounds.width / 2,
      clientY: bounds.top + bounds.height / 2,
    }));
    target.dispatchEvent(new PointerEvent('pointerup', {
      bubbles: true,
      composed: true,
      button: 0,  // Left
      pointerId: 1,
      isPrimary: true,
      buttons: 0,
      clientX: bounds.left + bounds.width / 2,
      clientY: bounds.top + bounds.height / 2,
    }));
    target.dispatchEvent(new MouseEvent('click', {
      bubbles: true,
      composed: true,
      button: 0,  // Left
      detail: 1,  // Single click
      clientX: bounds.left + bounds.width / 2,
      clientY: bounds.top + bounds.height / 2,
    }));
    target.setPointerCapture = oldPointerCapture.setPointerCapture;
    target.hasPointerCapture = oldPointerCapture.hasPointerCapture;
    target.releasePointerCapture = oldPointerCapture.releasePointerCapture;
    return {success: true};
  }

  private focusElement_(id: TrackedElementIdentifier): {success: boolean} {
    const trackedElement = this.getDataForId_(id);
    if (!trackedElement) {
      console.error(`TrackedElementManager: Focus failed, element not found: ${
          TrackedElementManager.idToString_(id)}`);
      return {success: false};
    }
    trackedElement.element.focus();
    return {success: true};
  }

  private selectTab_(id: TrackedElementIdentifier, index: number):
      {success: boolean} {
    const trackedElement = this.getDataForId_(id);
    if (!trackedElement) {
      console.error(
          `TrackedElementManager: SelectTab failed, element not found: ${
              TrackedElementManager.idToString_(id)}`);
      return {success: false};
    }

    const element = trackedElement.element;

    // Special handling for <cr-tabs>
    if (element.tagName === 'CR-TABS') {
      (element as unknown as {selected: number}).selected = index;
      return {success: true};
    }

    // Try to find tabs by ARIA role.
    const tabs = element.querySelectorAll('[role="tab"]');
    if (tabs.length > index) {
      (tabs[index] as HTMLElement).click();
      return {success: true};
    }

    // Fallback: try to find child elements that look like tabs.
    const childTabs = element.children;
    if (childTabs.length > index) {
      (childTabs[index] as HTMLElement).click();
      return {success: true};
    }

    console.error(`TrackedElementManager: SelectTab failed, tab index ${
        index} not found in ${TrackedElementManager.idToString_(id)}`);
    return {success: false};
  }

  private selectDropdownItem_(id: TrackedElementIdentifier, index: number):
      {success: boolean} {
    const trackedElement = this.getDataForId_(id);
    if (!trackedElement) {
      console.error(
          `TrackedElementManager: SelectDropdownItem failed, element not found: ${
              TrackedElementManager.idToString_(id)}`);
      return {success: false};
    }

    const element = trackedElement.element;

    if (element instanceof HTMLSelectElement) {
      if (index >= element.options.length) {
        console.error(
            `TrackedElementManager: SelectDropdownItem failed, index ${
                index} out of bounds for ${
                TrackedElementManager.idToString_(id)}`);
        return {success: false};
      }
      element.selectedIndex = index;
      element.dispatchEvent(new Event('change', {bubbles: true}));
      return {success: true};
    }

    // Special handling for <cr-select>
    if (element.tagName === 'CR-SELECT') {
      const select = element.shadowRoot?.querySelector('select');
      if (select && index < select.options.length) {
        select.selectedIndex = index;
        select.dispatchEvent(new Event('change', {bubbles: true}));
        return {success: true};
      }
    }

    console.error(`TrackedElementManager: SelectDropdownItem failed for ${
        TrackedElementManager.idToString_(
            id)}. Not a supported dropdown type or index ${
        index} out of bounds.`);
    return {success: false};
  }

  private static elementToIdentifier_(element: TrackedElementData):
      TrackedElementIdentifier {
    return {
      nativeIdentifier: element.nativeId,
      secondaryIdentifier: element.secondaryId,
    };
  }

  private enterText_(
      id: TrackedElementIdentifier, text: string,
      mode: TextEntryMode): {success: boolean} {
    const trackedElement = this.getDataForId_(id);
    if (!trackedElement) {
      console.error(
          `TrackedElementManager: EnterText failed, element not found: ${
              TrackedElementManager.idToString_(id)}`);
      return {success: false};
    }

    const element = trackedElement.element;
    if (!(element instanceof HTMLInputElement ||
          element instanceof HTMLTextAreaElement)) {
      // Check if it's a custom element wrapping an input (like <cr-input>)
      const input = element.shadowRoot?.querySelector('input, textarea');
      if (input instanceof HTMLInputElement ||
          input instanceof HTMLTextAreaElement) {
        return this.enterTextIntoInput_(input, text, mode);
      }
      console.error(
          `TrackedElementManager: EnterText failed, element is not an input: ${
              TrackedElementManager.idToString_(id)}`);
      return {success: false};
    }

    return this.enterTextIntoInput_(element, text, mode);
  }

  private enterTextIntoInput_(
      input: HTMLInputElement|HTMLTextAreaElement, text: string,
      mode: TextEntryMode): {success: boolean} {
    switch (mode) {
      case TextEntryMode.kReplaceAll:
        input.value = text;
        break;
      case TextEntryMode.kAppend:
        input.value += text;
        break;
      case TextEntryMode.kInsertOrReplace:
        const start = input.selectionStart || 0;
        const end = input.selectionEnd || 0;
        input.value =
            input.value.substring(0, start) + text + input.value.substring(end);
        input.selectionStart = input.selectionEnd = start + text.length;
        break;
      default:
        console.error(`TrackedElementManager: Invalid TextEntryMode: ${mode}`);
        return {success: false};
    }
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.dispatchEvent(new Event('change', {bubbles: true}));
    return {success: true};
  }

  private confirm_(id: TrackedElementIdentifier): {success: boolean} {
    const trackedElement = this.getDataForId_(id);
    if (!trackedElement) {
      console.error(
          `TrackedElementManager: Confirm failed, element not found: ${
              TrackedElementManager.idToString_(id)}`);
      return {success: false};
    }

    const element = trackedElement.element;
    element.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Enter',
      code: 'Enter',
      keyCode: 13,
      which: 13,
      bubbles: true,
      composed: true,
    }));
    return {success: true};
  }
}
