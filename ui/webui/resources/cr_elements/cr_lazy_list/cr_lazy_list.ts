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
 * To set content-visibility on chunks of elements rather than on individual
 * elements, use the `chunkSize` property and specify the number of elements
 * to group. This is useful when rendering large numbers of short items, as
 * the intersection observers added by content-visibility: auto can slow down
 * the UI for very large numbers of elements.
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
    if (this.chunkSize === 0) {
      render(
          this.items.slice(0, this.numItemsDisplayed_).map((item, index) => {
            return this.template(item, index);
          }),
          this, {host});
    } else {
      const chunks = Math.ceil(this.numItemsDisplayed_ / this.chunkSize);
      const chunkArray = new Array(chunks).fill(0);

      // Render chunk divs.
      render(
          chunkArray.map(
              (_item, index) => html`<div id="chunk-${index}" class="chunk">
                                     </div>`),
          this, {host});

      // Render items into chunk divs.
      for (let chunkIndex = 0; chunkIndex < chunks; chunkIndex++) {
        const start = chunkIndex * this.chunkSize;
        const end = Math.min(
            this.numItemsDisplayed_, (chunkIndex + 1) * this.chunkSize);
        const chunk = this.querySelector<HTMLElement>(`#chunk-${chunkIndex}`);
        assert(chunk);
        render(
            this.items.slice(start, end).map((item, index) => {
              return this.template(item, start + index);
            }),
            chunk, {host});
      }
    }

    // Render container + slot into shadow DOM
    return html`<div id="container"><slot id="slot"></slot></div>`;
  }

  static override get properties() {
    return {
      chunkSize: {
        type: Number,
        reflect: true,
      },
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

  accessor items: T[] = [];
  accessor itemSize: number|undefined = undefined;
  accessor listItemHost: Node|undefined;
  accessor minViewportHeight: number|undefined;
  accessor scrollOffset: number = 0;
  accessor scrollTarget: HTMLElement = document.documentElement;
  accessor restoreFocusElement: Element|null = null;
  accessor template: (item: T, index: number) => TemplateResult = () => html``;
  accessor chunkSize: number = 0;
  private accessor numItemsDisplayed_: number = 0;

  // Internal state
  private lastItemsLength_: number = 0;
  private lastRenderedHeight_: number = 0;
  private resizeObserver_: ResizeObserver|null = null;
  private scrollListener_: EventListener = () => this.onScroll_();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('items')) {
      this.lastItemsLength_ = this.items.length;
      this.numItemsDisplayed_ = this.items.length === 0 ?
          0 :
          Math.min(this.numItemsDisplayed_, this.items.length);
    } else {
      assert(
          this.items.length === this.lastItemsLength_,
          'Items array changed in place; rendered result may be incorrect.');
    }

    if (changedProperties.has('itemSize')) {
      this.style.setProperty('--list-item-size', `${this.itemSize}px`);
    }

    if (changedProperties.has('chunkSize')) {
      this.style.setProperty('--chunk-size', `${this.chunkSize}`);
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
      return this.domItems()[index] as HTMLElement;
    }
    assert(index < this.items.length);
    await this.updateNumItemsDisplayed_(index + 1);
    return this.domItems()[index] as HTMLElement;
  }

  // Private methods

  private addRemoveScrollTargetListeners_(oldTarget: HTMLElement|null) {
    if (oldTarget) {
      const target =
          oldTarget === document.documentElement ? window : oldTarget;
      target.removeEventListener('scroll', this.scrollListener_);
      assert(this.resizeObserver_);
      this.resizeObserver_.disconnect();
    }
    if (this.scrollTarget) {
      const target = this.scrollTarget === document.documentElement ?
          window :
          this.scrollTarget;
      target.addEventListener('scroll', this.scrollListener_);
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
          // The element may have been removed from the DOM by the client.
          if (!this.restoreFocusElement) {
            return;
          }
          (this.restoreFocusElement as HTMLElement).focus();
          this.fire('focus-restored-for-test');
        }, 0);
      }
    } else {
      // Update the container height to 0 since there are no items.
      this.$.container.style.height = '0px';
      this.fire('items-rendered');
      this.fire('viewport-filled');
    }
  }

  private getScrollTop_(): number {
    return this.scrollTarget === document.documentElement ?
        window.pageYOffset :
        this.scrollTarget.scrollTop;
  }

  private getViewHeight_() {
    const offsetHeight = this.scrollTarget === document.documentElement ?
        window.innerHeight :
        this.scrollTarget.offsetHeight;
    return this.getScrollTop_() - this.scrollOffset +
        Math.max(this.minViewportHeight || 0, offsetHeight);
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
    this.fire('items-rendered');

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
    const initialDomItemCount = this.domItems().length;
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
    if (this.numItemsDisplayed_ > 200 && this.chunkSize < 2) {
      console.warn(
          `cr-lazy-list: ${this.numItemsDisplayed_} list items rendered. ` +
          'If this is expected, consider chunking mode (chunkSize > 1) ' +
          'to improve scrolling performance.');
    }
    await this.updateComplete;
  }

  /**
   * @return The currently rendered list items, particularly useful for clients
   *     using chunking mode.
   */
  domItems(): Element[] {
    return this.chunkSize === 0 ?
        this.$.slot.assignedElements() :
        Array.from(this.querySelectorAll('.chunk > *'));
  }

  /**
   * @return The average DOM item height.
   */
  private domItemAverageHeight_(): number {
    // This logic should only be invoked if the list is non-empty and at least
    // one DOM item has been rendered so that an item average height can be
    // estimated. This is ensured by the callers.
    assert(this.items.length > 0);
    const domItems = this.domItems();
    assert(domItems.length > 0);
    const firstDomItem = domItems.at(0) as HTMLElement;
    const lastDomItem = domItems.at(-1) as HTMLElement;
    const lastDomItemHeight = lastDomItem.offsetHeight;
    if (firstDomItem === lastDomItem && lastDomItemHeight === 0) {
      // If there is only 1 item and it has a height of 0, return early. This
      // likely means the UI is still hidden or there is no content.
      return 0;
    } else if (this.itemSize) {
      // Once items are actually visible and have a height > 0, assume that it
      // is an accurate representation of the average item size.
      return this.itemSize;
    }
    let totalHeight = lastDomItem.offsetTop + lastDomItemHeight;
    if (this.chunkSize > 0) {
      // Add the parent's offsetTop. The offsetParent will be the chunk div.
      // Subtract the offsetTop of the first chunk div to avoid counting any
      // padding.
      totalHeight += (lastDomItem.offsetParent as HTMLElement).offsetTop -
          (firstDomItem.offsetParent as HTMLElement).offsetTop;
    } else {
      // Subtract the offsetTop of the first item to avoid counting any padding.
      totalHeight -= firstDomItem.offsetTop;
    }
    return totalHeight / domItems.length;
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
    const scrollTop = this.getScrollTop_();
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
