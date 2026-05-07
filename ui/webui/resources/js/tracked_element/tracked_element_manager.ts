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

  /**
   * If this is set, this element will be marked as supporting anchor
   * highlighting, and the method will be invoked when the highlight state
   * (initially false) changes.
   */
  onHighlightChanged?: (highlighted: boolean) => void;
}

interface TrackedElement {
  element: HTMLElement;
  nativeId: string;
  padding: InsetsF;
  fixed: boolean;
  visible: boolean;
  bounds: RectF;
  onVisibilityChanged?: (visible: boolean, bounds: RectF) => void;
  onHighlightChanged?: (highlighted: boolean) => void;
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

  static setInstanceForTesting(instance: TrackedElementManager) {
    TrackedElementManager.instance_ = instance;
  }

  private trackedElementHandler_: TrackedElementHandlerInterface;

  // Mapped from native ID.
  private trackedElements_: Map<string, TrackedElement> = new Map();
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
        if (this.getTrackedElement_(target)) {
          this.onElementVisibilityChanged_(target, computeIsVisible(target));
        }
      }
    });

    // Helper to check if a node or its descendants are tracked elements.
    const checkTrackedNodes = (nodes: NodeList) => {
      nodes.forEach(node => {
        if (node instanceof HTMLElement) {
          // Check if the node is a tracked element.
          if (this.getTrackedElement_(node)) {
            this.onElementVisibilityChanged_(node, computeIsVisible(node));
          }
          // Check if any descendants are tracked elements.
          node.querySelectorAll('*').forEach(descendant => {
            if (descendant instanceof HTMLElement &&
                this.getTrackedElement_(descendant)) {
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

  private getTrackedElement_(element: HTMLElement): TrackedElement|undefined {
    const nativeId = element.dataset['nativeId'];
    if (!nativeId) {
      return undefined;
    }
    const maybeTrackedElement = this.trackedElements_.get(nativeId);
    // Make sure this is what we're actually tracking and not an element with
    // a stale data-native-id.
    if (maybeTrackedElement?.element === element) {
      return maybeTrackedElement;
    }
    return undefined;
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
    if (this.getTrackedElement_(element)) {
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
      onHighlightChanged: options?.onHighlightChanged,
    };
    this.trackedElements_.set(nativeId, trackedElement);

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

    if (trackedElement.onHighlightChanged) {
      this.trackedElementHandler_.trackedElementCanHighlightChanged(
          nativeId, true);
    }
  }

  /**
   * Stops tracking an element.
   * A visibility event with `visible: false` will be sent immediately.
   *
   * @param element The element to stop tracking.
   */
  stopTracking(element: HTMLElement) {
    const trackedElement = this.getTrackedElement_(element);
    if (!trackedElement) {
      return;
    }

    if (trackedElement.onHighlightChanged) {
      this.trackedElementHandler_.trackedElementCanHighlightChanged(
          trackedElement.nativeId, false);
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
    this.trackedElements_.delete(trackedElement.nativeId);

    delete element.dataset['nativeId'];
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
    const trackedElement = this.getTrackedElement_(element);
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
    this.trackedElements_.forEach((trackedElement, _) => {
      const element = trackedElement.element;
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

    const trackedElement = this.getTrackedElement_(element);
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
  private onElementHighlightChanged_(nativeId: string, highlighted: boolean) {
    const trackedElement = this.trackedElements_.get(nativeId);
    const maybeCallback = trackedElement?.onHighlightChanged;
    if (maybeCallback) {
      maybeCallback(highlighted);
    }
  }

  private async waitUntilNotDisabled_(element: HTMLElement, nativeId: string):
      Promise<void> {
    if (!element.hasAttribute('disabled')) {
      return;
    }

    console.info(
        `TrackedElementManager: Element ${nativeId} is disabled, ` +
        `waiting...`);

    return new Promise((resolve) => {
      const observer = new MutationObserver(() => {
        if (!element.hasAttribute('disabled')) {
          observer.disconnect();
          console.info(
              `TrackedElementManager: Element ${nativeId} is no ` +
              `longer disabled.`);
          resolve();
        }
      });
      observer.observe(
          element, {attributes: true, attributeFilter: ['disabled']});
    });
  }

  private async clickElement_(nativeId: string): Promise<{success: boolean}> {
    const trackedElement = this.trackedElements_.get(nativeId);
    if (!trackedElement) {
      console.error(`TrackedElementManager: Click failed, element not found: ${
          nativeId}`);
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

    await this.waitUntilNotDisabled_(target, nativeId);

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

  private focusElement_(nativeId: string): {success: boolean} {
    const trackedElement = this.trackedElements_.get(nativeId);
    if (!trackedElement) {
      console.error(`TrackedElementManager: Focus failed, element not found: ${
          nativeId}`);
      return {success: false};
    }
    trackedElement.element.focus();
    return {success: true};
  }

  private selectTab_(nativeId: string, index: number): {success: boolean} {
    const trackedElement = this.trackedElements_.get(nativeId);
    if (!trackedElement) {
      console.error(
          `TrackedElementManager: SelectTab failed, element not found: ${
              nativeId}`);
      return {success: false};
    }

    const element = trackedElement.element;

    // Special handling for <cr-tabs>
    if (element.tagName === 'CR-TABS') {
      (element as any).selected = index;
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
        index} not found in ${nativeId}`);
    return {success: false};
  }

  private selectDropdownItem_(nativeId: string, index: number):
      {success: boolean} {
    const trackedElement = this.trackedElements_.get(nativeId);
    if (!trackedElement) {
      console.error(
          `TrackedElementManager: SelectDropdownItem failed, element not found: ${
              nativeId}`);
      return {success: false};
    }

    const element = trackedElement.element;

    if (element instanceof HTMLSelectElement) {
      if (index >= element.options.length) {
        console.error(
            `TrackedElementManager: SelectDropdownItem failed, index ${
                index} out of bounds for ${nativeId}`);
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
        nativeId}. Not a supported dropdown type or index ${
        index} out of bounds.`);
    return {success: false};
  }

  private enterText_(nativeId: string, text: string, mode: TextEntryMode):
      {success: boolean} {
    const trackedElement = this.trackedElements_.get(nativeId);
    if (!trackedElement) {
      console.error(
          `TrackedElementManager: EnterText failed, element not found: ${
              nativeId}`);
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
              nativeId}`);
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

  private confirm_(nativeId: string): {success: boolean} {
    const trackedElement = this.trackedElements_.get(nativeId);
    if (!trackedElement) {
      console.error(
          `TrackedElementManager: Confirm failed, element not found: ${
              nativeId}`);
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
