// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../../js/assert_ts.js';
import {FocusOutlineManager} from '../../js/cr/ui/focus_outline_manager.m.js';
import {CustomElement} from '../../js/custom_element.js';
import {getTrustedHTML} from '../../js/static_types.js';

export class CrTabBoxElement extends CustomElement {
  static override get template() {
    return getTrustedHTML`{__html_template__}`;
  }

  private selectedIndex_: number = -1;
  private tabs_: HTMLElement;
  private panels_: HTMLElement;
  private focusOutlineManager_: FocusOutlineManager;

  constructor() {
    super();

    const tabs = this.$<HTMLElement>('#tablist');
    assert(tabs);
    this.tabs_ = tabs;
    this.tabs_.addEventListener('keydown', e => this.onKeydown_(e));
    const panels = this.$<HTMLElement>('#tabpanels');
    assert(panels);
    this.panels_ = panels;
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
  }

  connectedCallback() {
    this.updateSelected(0);

    this.getTabs_().forEach((panel: Element, index: number) => {
      panel.addEventListener('click', this.updateSelected.bind(this, index));
    });
  }

  private getTabs_(): HTMLElement[] {
    return Array.from(this.tabs_.querySelector('slot')!.assignedElements()) as
        HTMLElement[];
  }

  private getPanels_(): Element[] {
    return Array.from(this.panels_.querySelector('slot')!.assignedElements());
  }

  updateSelected(selected: number) {
    if (selected === this.selectedIndex_) {
      return;
    }

    this.getPanels_().forEach((panel: Element, index: number) => {
      panel.toggleAttribute('selected', index === selected);
    });
    this.getTabs_().forEach((tab: HTMLElement, index: number) => {
      const isSelected = index === selected;
      tab.toggleAttribute('selected', isSelected);
      // Update tabIndex for a11y
      tab.setAttribute('tabindex', isSelected ? '0' : '-1');
      // Update aria-selected attribute for a11y
      const firstSelection = !tab.hasAttribute('aria-selected');
      tab.setAttribute('aria-selected', isSelected ? 'true' : 'false');
      // Update focus, but don't override initial focus.
      if (isSelected && !firstSelection) {
        tab.focus();
      }
    });
    this.selectedIndex_ = selected;
  }

  private onKeydown_(e: KeyboardEvent) {
    let delta = 0;
    switch (e.key) {
      case 'ArrowLeft':
      case 'ArrowUp':
        delta = -1;
        break;
      case 'ArrowRight':
      case 'ArrowDown':
        delta = 1;
        break;
    }

    if (!delta) {
      return;
    }

    if (document.documentElement.dir === 'rtl') {
      delta *= -1;
    }

    const count = this.getTabs_().length;
    this.updateSelected((this.selectedIndex_ + delta + count) % count);

    // Show focus outline since we used the keyboard.
    this.focusOutlineManager_.visible = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-tab-box': CrTabBoxElement;
  }
}

customElements.define('cr-tab-box', CrTabBoxElement);
