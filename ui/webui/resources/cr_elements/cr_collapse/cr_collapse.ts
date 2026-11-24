// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_collapse.css.js';
import {getHtml} from './cr_collapse.html.js';

export class CrCollapseElement extends CrLitElement {
  static get is() {
    return 'cr-collapse';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      opened: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      noAnimation: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor opened: boolean = false;
  accessor noAnimation: boolean = false;

  toggle() {
    this.opened = !this.opened;
  }

  show() {
    this.opened = true;
  }

  hide() {
    this.opened = false;
  }

  override firstUpdated() {
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'group');
    }
    this.setAttribute('aria-hidden', 'true');
    this.addEventListener(
        'transitionend', (e: TransitionEvent) => this.onTransitionEnd_(e));
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (!changedProperties.has('opened')) {
      return;
    }

    this.setAttribute('aria-hidden', this.opened ? 'false' : 'true');
    this.classList.toggle('collapse-closed', false);
    this.classList.toggle('collapse-opened', false);
    this.updateHeight_(this.opened, changedProperties.get('opened'));

    // Focus the current collapse.
    if (this.opened) {
      this.focus();
    }
  }

  private hasHeightTransition_(): boolean {
    const {transitionProperty, transitionDuration, transitionDelay} =
        getComputedStyle(this);
    const properties = transitionProperty.split(',').map(p => p.trim());
    const durations =
        transitionDuration.split(',').map(d => parseFloat(d) || 0);
    const delays = transitionDelay.split(',').map(d => parseFloat(d) || 0);

    // Walk from the end because later entries in transition-property override
    // earlier ones for the same property.
    for (let i = properties.length - 1; i >= 0; --i) {
      const prop = properties[i];
      if (prop === 'max-height' || prop === 'all') {
        const duration = durations[i % durations.length] ?? 0;
        const delay = delays[i % delays.length] ?? 0;
        return duration + delay > 0;
      }
    }
    return false;
  }

  private updateHeight_(opening: boolean, lastOpened?: boolean) {
    const finalMaxHeight = opening ? '' : '0px';
    const animationStartSize = `${this.getBoundingClientRect().height}px`;
    const animationEndSize = opening ? `${this.scrollHeight}px` : '0px';

    const willAnimate = lastOpened !== undefined && !this.noAnimation &&
        this.style.maxHeight !== finalMaxHeight &&
        animationStartSize !== animationEndSize && this.hasHeightTransition_();

    if (willAnimate && !opening) {
      // Force layout to ensure transition will go. Set maxHeight to a px
      // value and scrollTop to itself.
      this.style.maxHeight = animationStartSize;
      this.scrollTop = this.scrollTop;
    }

    // Set the final size.
    this.style.maxHeight = animationEndSize;

    // If it won't animate, set correct classes. Otherwise these are set in
    // onTransitionEnd_().
    if (!willAnimate) {
      this.updateStyles_();
    }
  }

  private onTransitionEnd_(e: TransitionEvent) {
    if (e.composedPath()[0] === this && e.propertyName === 'max-height') {
      this.updateStyles_();
    }
  }

  private updateStyles_() {
    this.style.maxHeight = this.opened ? '' : '0px';
    this.classList.toggle('collapse-closed', !this.opened);
    this.classList.toggle('collapse-opened', this.opened);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-collapse': CrCollapseElement;
  }
}

customElements.define(CrCollapseElement.is, CrCollapseElement);
