// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-lazy-list' is a component optimized for showing a list of
 * items that overflows the view and requires scrolling. For performance
 * reasons, the DOM items are incrementally added to the view as the user
 * scrolls through the list. The component expects a `scrollTarget` property
 * to be specified indicating the scrolling container. This container is
 * used for observing scroll events and resizes. The container should have
 * bounded height so that cr-lazy-list can determine how many HTML elements to
 * render initially.
 * If using a container that can shrink arbitrarily small to the height of the
 * contents, a 'minViewportHeight' property should also be provided specifying
 * the minimum viewport height to try to fill with items.
 * Each list item's HTML element is created using the `template` property,
 * which should be set to a function returning a TemplateResult corresponding
 * to a passed in list item and selection index.
 * Set `listItemHost` to the `this` context for any event handlers in this
 * template. If this property is not provided, cr-lazy-list is assumed to be
 * residing in a ShadowRoot, and the shadowRoot's |host| is used.
 * The `items` property specifies an array of list item data.
 * The `itemSize` property should be set to an estimate of the list item size.
 * This is used when setting contain-intrinsic-size styling for list items.
 * To restore focus to a specific item if it is focused when the items
 * array changes, set `restoreFocusItem` to that HTMLElement. If the element
 * is focused when the items array is updated, focus will be restored.
 */

import {assert} from '//resources/js/assert.js';
import {getDeepActiveElement} from '//resources/js/util.js';
import {CrLitElement, html, render} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues, TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_lazy_list.css.js';

export interface CrLazyListElement {
  $: {
    container: HTMLElement,
    slot: HTMLSlotElement,
  };
}

export class CrLazyListElement<T = object> extends CrLitElement {
  static get is() {
    return 'cr-lazy-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    const host = this.listItemHost === undefined ?
        (this.getRootNode() as ShadowRoot).host :
        this.listItemHost;

    // Render items into light DOM using the client provided template
    render(
        this.items.slice(0, this.numItemsDisplayed_).map((item, index) => {
          return this.template(item, index);
        }),
        this, {
          host: host,
        });

    // Render container + slot into shadow DOM
    return html`<div id="container"><slot id="slot"></slot></div>`;
  }

  static override get properties() {
    return {
      items: {type: Array},
      itemSize: {type: Number},
      listItemHost: {type: Object},
      minViewportHeight: {type: Number},
      scrollOffset: {type: Number},
      scrollTarget: {type: Object},
      restoreFocusElement: {type: Object},
      template: {type: Object},
      numItemsDisplayed_: {
        state: true,
        type: Number,
      },
    };
  }

  items: T[] = [];
  itemSize: number = 100;
  listItemHost?: Node;
  minViewportHeight?: number;
  scrollOffset: number = 0;
  scrollTarget: HTMLElement = document.documentElement;
  restoreFocusElement: Element|null = null;
  template: (item: T, index: number) => TemplateResult = () => html``;
  private numItemsDisplayed_: number = 0;

  // Internal state
  private lastRenderedHeight_: number = 0;
  private resizeObserver_: ResizeObserver|null = null;
  private scrollListener_: EventListener = () => this.onScroll_();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('items')) {
      this.numItemsDisplayed_ = this.items.length === 0 ?
          0 :
          Math.min(this.numItemsDisplayed_, this.items.length);
    }

    if (changedProperties.has('itemSize')) {
      this.style.setProperty('--list-item-size', `${this.itemSize}px`);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    let itemsChanged = false;
    if (changedProperties.has('items') ||
        changedProperties.has('minViewportHeight') ||
        changedProperties.has('scrollOffset')) {
      const previous = changedProperties.get('items');
      if (previous !== undefined || this.items.length !== 0) {
        this.onItemsChanged_();
        itemsChanged = true;
      }
    }

    if (changedProperties.has('scrollTarget')) {
      this.addRemoveScrollTargetListeners_(
          changedProperties.get('scrollTarget') || null);
      // Only re-render if there are items to display and we are not already
      // re-rendering for the items.
      if (this.scrollTarget && this.items.length > 0 && !itemsChanged) {
        this.fillCurrentViewport();
      }
    }
  }

  // Public API

  // Forces the list to fill the current viewport. Called when the viewport
  // size or position changes.
  fillCurrentViewport(): Promise<void> {
    if (this.items.length === 0) {
      return Promise.resolve();
    }

    // Update the height if the previous height calculation was done when this
    // element was not visible or if new DOM items were added.
    return this.update_(this.$.container.style.height === '0px');
  }

  // Forces the list to render |numItems| items. If |numItems| are already
  // rendered, this is a no-op.
  async ensureItemRendered(index: number): Promise<HTMLElement> {
    if (index < this.numItemsDisplayed_) {
      return this.$.slot.assignedElements()[index] as HTMLElement;
    }
    assert(index < this.items.length);
    await this.updateNumItemsDisplayed_(index + 1);
    return this.$.slot.assignedElements()[index] as HTMLElement;
  }

  // Private methods

  private addRemoveScrollTargetListeners_(oldTarget: HTMLElement|null) {
    if (oldTarget) {
      oldTarget.removeEventListener('scroll', this.scrollListener_);
      assert(this.resizeObserver_);
      this.resizeObserver_.disconnect();
    }
    if (this.scrollTarget) {
      this.scrollTarget.addEventListener('scroll', this.scrollListener_);
      this.resizeObserver_ = new ResizeObserver(() => {
        requestAnimationFrame(() => {
          const newHeight = this.getViewHeight_();
          if (newHeight > 0 && newHeight !== this.lastRenderedHeight_) {
            this.fillCurrentViewport();
          }
        });
      });
      this.resizeObserver_.observe(this.scrollTarget);
    }
  }

  private shouldRestoreFocus_(): boolean {
    if (!this.restoreFocusElement) {
      return false;
    }
    const active = getDeepActiveElement();
    return this.restoreFocusElement === active ||
        (!!this.restoreFocusElement.shadowRoot &&
         this.restoreFocusElement.shadowRoot.activeElement === active);
  }

  private async onItemsChanged_() {
    if (this.items.length > 0) {
      const restoreFocus = this.shouldRestoreFocus_();
      await this.update_(true);

      if (restoreFocus) {
        // Async to allow clients to update in response to viewport-filled.
        setTimeout(() => {
          (this.restoreFocusElement as HTMLElement).focus();
          this.fire('focus-restored-for-test');
        }, 0);
      }
    } else {
      // Update the container height to 0 since there are no items.
      this.$.container.style.height = '0px';
      this.fire('viewport-filled');
    }
  }

  private getViewHeight_() {
    return this.scrollTarget.scrollTop - this.scrollOffset +
        Math.max(this.minViewportHeight || 0, this.scrollTarget.offsetHeight);
  }

  private async update_(forceUpdateHeight: boolean): Promise<void> {
    if (!this.scrollTarget) {
      return;
    }

    const height = this.getViewHeight_();
    if (height <= 0) {
      return;
    }

    const added = await this.fillViewHeight_(height);
    if (added || forceUpdateHeight) {
      await this.updateHeight_();
      this.fire('viewport-filled');
    }
  }

  /**
   * @return Whether DOM items were created or not.
   */
  private async fillViewHeight_(height: number): Promise<boolean> {
    this.fire('fill-height-start');
    this.lastRenderedHeight_ = height;

    // Ensure we have added enough DOM items so that we are able to estimate
    // item average height.
    assert(this.items.length);
    const initialDomItemCount = this.$.slot.assignedElements().length;
    if (initialDomItemCount === 0) {
      await this.updateNumItemsDisplayed_(1);
    }

    const itemHeight = this.domItemAverageHeight_();
    // If this happens, the math below will be incorrect and we will render
    // all items. So return early, and correct |lastRenderedHeight_|.
    if (itemHeight === 0) {
      this.lastRenderedHeight_ = 0;
      return false;
    }

    const desiredDomItemCount =
        Math.min(Math.ceil(height / itemHeight), this.items.length);
    if (desiredDomItemCount > this.numItemsDisplayed_) {
      await this.updateNumItemsDisplayed_(desiredDomItemCount);
    }

    const added = initialDomItemCount !== desiredDomItemCount;
    if (added) {
      this.fire('fill-height-end');
    }
    return added;
  }

  private async updateNumItemsDisplayed_(itemsToDisplay: number) {
    this.numItemsDisplayed_ = itemsToDisplay;
    await this.updateComplete;
  }

  /**
   * @return The average DOM item height.
   */
  private domItemAverageHeight_(): number {
    // This logic should only be invoked if the list is non-empty and at least
    // one DOM item has been rendered so that an item average height can be
    // estimated. This is ensured by the callers.
    assert(this.items.length > 0);
    const domItems = this.$.slot.assignedElements();
    assert(domItems.length > 0);
    const lastDomItem = domItems.at(-1) as HTMLElement;
    return (lastDomItem.offsetTop + lastDomItem.offsetHeight) / domItems.length;
  }

  /**
   * Sets the height of the component based on an estimated average DOM item
   * height and the total number of items.
   */
  private async updateHeight_() {
    // Await 1 cycle to ensure any child Lit elements have time to finish
    // rendering, or the height estimated below will be incorrect.
    await new Promise(resolve => setTimeout(resolve, 0));
    const estScrollHeight = this.items.length > 0 ?
        this.items.length * this.domItemAverageHeight_() :
        0;
    this.$.container.style.height = estScrollHeight + 'px';
  }

  /**
   * Adds additional DOM items as needed to fill the view based on user scroll
   * interactions.
   */
  private async onScroll_() {
    const scrollTop = this.scrollTarget.scrollTop;
    if (scrollTop <= 0 || this.numItemsDisplayed_ === this.items.length) {
      return;
    }

    await this.fillCurrentViewport();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-lazy-list': CrLazyListElement;
  }
}

customElements.define(CrLazyListElement.is, CrLazyListElement);
