// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-icon-button' is a button which displays an icon with a
 * ripple. It can be interacted with like a normal button using click as well as
 * space and enter to effectively click the button and fire a 'click' event.
 *
 * There are two sources to icons:
 * Option 1: CSS classes defined in cr_icons.css.
 * Option 2: SVG icons defined in a cr-iconset or iron-iconset-svg,
 *     with the name passed to cr-icon-button via the |ironIcon| property.
 *
 * Example of using CSS classes:
 * In the .html.ts template file (if using a .html template file instead, the
 * import should be in the corresponding .ts file):
 * import 'chrome://resources/cr_elements/cr_icons.css.js';
 *
 * export function getHtml() {
 *   return html`
 *     <cr-icon-button class="icon-class-name"></cr-icon-button>`;
 * }
 *
 * When an icon is specified using a class, the expectation is the
 * class will set an image to the --cr-icon-image variable.
 *
 * Example of using a cr-iconset to supply an icon via the iron-icon parameter:
 * In the .html.ts template file (if using a .html template file instead, the
 * import should be in the corresponding .ts file):
 * import 'chrome://resources/cr_elements/icons_lit.html.js';
 *
 * export function getHtml() {
 *   return html`
 *     <cr-icon-button iron-icon="cr:icon-key"></cr-icon-button>`;
 * }
 *
 * The color of the icon can be overridden using CSS variables. When using
 * the ironIcon property to populate cr-icon-button's internal <cr-icon>, the
 * following CSS variables for fill and stroke can be overridden for cr-icon:
 * --iron-icon-button-fill-color
 * --iron-icon-button-stroke-color
 *
 * When not using the ironIcon property, cr-icon-button will not create a
 * <cr-icon>, so the cr-icon related CSS variables above are ignored.
 *
 * When using the ironIcon property, more than one icon can be specified by
 * setting the |ironIcon| property to a comma-delimited list of keys.
 */

import '../cr_icon/cr_icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {CrRippleMixin} from '../cr_ripple/cr_ripple_mixin.js';

import {getCss} from './cr_icon_button.css.js';
import {getHtml} from './cr_icon_button.html.js';

export interface CrIconButtonElement {
  $: {
    icon: HTMLElement,
  };
}

const CrIconbuttonElementBase = CrRippleMixin(CrLitElement);

export class CrIconButtonElement extends CrIconbuttonElementBase {
  static get is() {
    return 'cr-icon-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {
        type: Boolean,
        reflect: true,
      },

      ironIcon: {
        type: String,
        reflect: true,
      },

      suppressRtlFlip: {
        type: Boolean,
        value: false,
        reflect: true,
      },

      multipleIcons_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  disabled: boolean = false;
  ironIcon?: string;
  protected multipleIcons_: boolean = false;

  /**
   * It is possible to activate a tab when the space key is pressed down. When
   * this element has focus, the keyup event for the space key should not
   * perform a 'click'. |spaceKeyDown_| tracks when a space pressed and
   * handled by this element. Space keyup will only result in a 'click' when
   * |spaceKeyDown_| is true. |spaceKeyDown_| is set to false when element
   * loses focus.
   */
  private spaceKeyDown_: boolean = false;

  constructor() {
    super();

    this.addEventListener('blur', this.onBlur_.bind(this));
    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.addEventListener('keyup', this.onKeyUp_.bind(this));
    this.ensureRippleOnPointerdown();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('ironIcon')) {
      const icons = (this.ironIcon || '').split(',');
      this.multipleIcons_ = icons.length > 1;
    }
  }

  override firstUpdated() {
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'button');
    }
    if (!this.hasAttribute('tabindex')) {
      this.setAttribute('tabindex', '0');
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('disabled')) {
      this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
      this.disabledChanged_(this.disabled, changedProperties.get('disabled'));
    }

    if (changedProperties.has('ironIcon')) {
      this.onIronIconChanged_();
    }
  }

  private disabledChanged_(newValue: boolean, oldValue: boolean|undefined) {
    if (!newValue && oldValue === undefined) {
      return;
    }
    if (this.disabled) {
      this.blur();
    }
    this.setAttribute('tabindex', String(this.disabled ? -1 : 0));
  }

  private onBlur_() {
    this.spaceKeyDown_ = false;
  }

  private onClick_(e: Event) {
    if (this.disabled) {
      e.stopImmediatePropagation();
    }
  }

  private async onIronIconChanged_() {
    this.shadowRoot!.querySelectorAll('cr-icon').forEach(el => el.remove());
    if (!this.ironIcon) {
      return;
    }
    const icons = (this.ironIcon || '').split(',');
    icons.forEach(async icon => {
      const crIcon = document.createElement('cr-icon');
      crIcon.icon = icon;
      this.$.icon.appendChild(crIcon);
      await crIcon.updateComplete;
      crIcon.shadowRoot!.querySelectorAll('svg, img')
          .forEach(child => child.setAttribute('role', 'none'));
    });
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    if (e.repeat) {
      return;
    }

    if (e.key === 'Enter') {
      this.click();
    } else if (e.key === ' ') {
      this.spaceKeyDown_ = true;
    }
  }

  private onKeyUp_(e: KeyboardEvent) {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
    }

    if (this.spaceKeyDown_ && e.key === ' ') {
      this.spaceKeyDown_ = false;
      this.click();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-icon-button': CrIconButtonElement;
  }
}

customElements.define(CrIconButtonElement.is, CrIconButtonElement);
