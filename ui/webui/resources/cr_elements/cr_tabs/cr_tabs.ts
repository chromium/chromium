// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-tabs' is a control used for selecting different sections or
 * tabs. cr-tabs was created to replace paper-tabs and paper-tab. cr-tabs
 * displays the name of each tab provided by |tabs|. A 'selected-changed' event
 * is fired any time |selected| is changed.
 *
 * cr-tabs takes its #selectionBar animation from paper-tabs.
 *
 * Keyboard behavior
 *   - Home, End, ArrowLeft and ArrowRight changes the tab selection
 *
 * Known limitations
 *   - no "disabled" state for the cr-tabs as a whole or individual tabs
 *   - cr-tabs does not accept any <slot> (not necessary as of this writing)
 *   - no horizontal scrolling, it is assumed that tabs always fit in the
 *     available space
 */
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_tabs.css.js';
import {getHtml} from './cr_tabs.html.js';

export const NONE_SELECTED: number = -1;

export class CrTabsElement extends CrLitElement {
  static get is() {
    return 'cr-tabs';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // Optional icon urls displayed in each tab.
      tabIcons: {
        type: Array,
      },

      // Tab names displayed in each tab.
      tabNames: {
        type: Array,
      },

      /** Index of the selected tab. */
      selected: {
        type: Number,
        notify: true,
      },
    };
  }

  tabIcons: string[] = [];
  tabNames: string[] = [];
  selected: number = NONE_SELECTED;

  private isRtl_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.isRtl_ = this.matches(':host-context([dir=rtl]) cr-tabs');
  }

  override firstUpdated() {
    this.setAttribute('role', 'tablist');
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('selected')) {
      this.onSelectedChanged_(this.selected, changedProperties.get('selected'));
    }
  }

  protected getAriaSelected_(index: number): string {
    return index === this.selected ? 'true' : 'false';
  }

  protected getIconStyle_(index: number): string {
    const icon = this.tabIcons[index];
    return icon ? `-webkit-mask-image: url(${icon}); display: block;` : '';
  }

  protected getTabindex_(index: number): string {
    return index === this.selected ? '0' : '-1';
  }

  protected getSelectedClass_(index: number): string {
    return index === this.selected ? 'selected' : '';
  }

  private onSelectedChanged_(
      newSelected: number, oldSelected: number|undefined) {
    if (newSelected === NONE_SELECTED || oldSelected === NONE_SELECTED ||
        oldSelected === undefined) {
      return;
    }

    const tabs = this.shadowRoot!.querySelectorAll('.tab');

    if (tabs.length <= oldSelected) {
      return;
    }

    const oldTabRect = tabs[oldSelected]!.getBoundingClientRect();
    const newTabRect = tabs[newSelected]!.getBoundingClientRect();

    const newIndicator =
        tabs[newSelected]!.querySelector<HTMLElement>('.tab-indicator')!;
    newIndicator.classList.remove('expand', 'contract');

    // Make new indicator look like it is the old indicator.
    this.updateIndicator_(
        newIndicator, newTabRect, oldTabRect.left, oldTabRect.width);
    newIndicator.getBoundingClientRect();  // Force repaint.

    // Expand to cover both the previous selected tab, the newly selected tab,
    // and everything in between.
    newIndicator.classList.add('expand');
    newIndicator.addEventListener(
        'transitionend', e => this.onIndicatorTransitionEnd_(e), {once: true});
    const leftmostEdge = Math.min(oldTabRect.left, newTabRect.left);
    const fullWidth = newTabRect.left > oldTabRect.left ?
        newTabRect.right - oldTabRect.left :
        oldTabRect.right - newTabRect.left;
    this.updateIndicator_(newIndicator, newTabRect, leftmostEdge, fullWidth);
  }

  private async onKeyDown_(e: KeyboardEvent) {
    const count = this.tabNames.length;
    let newSelection;
    if (e.key === 'Home') {
      newSelection = 0;
    } else if (e.key === 'End') {
      newSelection = count - 1;
    } else if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
      const delta = e.key === 'ArrowLeft' ? (this.isRtl_ ? 1 : -1) :
                                            (this.isRtl_ ? -1 : 1);
      newSelection = (count + this.selected + delta) % count;
    } else {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    this.selected = newSelection;
    await this.updateComplete;
    this.shadowRoot!.querySelector<HTMLElement>('.tab.selected')!.focus();
  }

  private onIndicatorTransitionEnd_(event: Event) {
    const indicator = event.target as HTMLElement;
    indicator.classList.replace('expand', 'contract');
    indicator.style.transform = `translateX(0) scaleX(1)`;
  }

  protected onTabClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    this.selected = Number(target.dataset['index']);
  }

  private updateIndicator_(
      indicator: HTMLElement, originRect: ClientRect, newLeft: number,
      newWidth: number) {
    const leftDiff = 100 * (newLeft - originRect.left) / originRect.width;
    const widthRatio = newWidth / originRect.width;
    const transform = `translateX(${leftDiff}%) scaleX(${widthRatio})`;
    indicator.style.transform = transform;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-tabs': CrTabsElement;
  }
}

customElements.define(CrTabsElement.is, CrTabsElement);
