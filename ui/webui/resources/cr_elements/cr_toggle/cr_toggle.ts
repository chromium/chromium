// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Number of pixels required to move to consider the pointermove event as
 * intentional.
 */
export const MOVE_THRESHOLD_PX: number = 5;

/**
 * @fileoverview 'cr-toggle' is a component for showing an on/off switch. It
 * fires a 'change' event *only* when its state changes as a result of a user
 * interaction. Besides just clicking the element, its state can be changed by
 * dragging (pointerdown+pointermove) the element towards the desired direction.
 */
import {CrRippleMixin} from '../cr_ripple/cr_ripple_mixin.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {assert} from '//resources/js/assert.js';
import {getCss} from './cr_toggle.css.js';
import {getHtml} from './cr_toggle.html.js';

const CrToggleElementBase = CrRippleMixin(CrLitElement);

export interface CrToggleElement {
  $: {
    knob: HTMLElement,
  };
}

export class CrToggleElement extends CrToggleElementBase {
  static get is() {
    return 'cr-toggle';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      checked: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      disabled: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  checked: boolean = false;
  disabled: boolean = false;

  private boundPointerMove_: ((e: PointerEvent) => void)|null = null;
  /**
   * Whether the state of the toggle has already taken into account by
   * |pointeremove| handlers. Used in the 'click' handler.
   */
  private handledInPointerMove_: boolean = false;
  private pointerDownX_: number = 0;

  override firstUpdated() {
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'button');
    }
    if (!this.hasAttribute('tabindex')) {
      this.setAttribute('tabindex', '0');
    }
    this.setAttribute('aria-pressed', this.checked ? 'true' : 'false');
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');

    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.addEventListener('keyup', this.onKeyUp_.bind(this));
    this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
    this.addEventListener('pointerup', this.onPointerUp_.bind(this));
  }

  override connectedCallback() {
    super.connectedCallback();

    const direction =
        this.matches(':host-context([dir=rtl]) cr-toggle') ? -1 : 1;
    this.boundPointerMove_ = (e: PointerEvent) => {
      // Prevent unwanted text selection to occur while moving the pointer, this
      // is important.
      e.preventDefault();

      const diff = e.clientX - this.pointerDownX_;
      if (Math.abs(diff) < MOVE_THRESHOLD_PX) {
        return;
      }

      this.handledInPointerMove_ = true;

      const shouldToggle = (diff * direction < 0 && this.checked) ||
          (diff * direction > 0 && !this.checked);
      if (shouldToggle) {
        this.toggleState_(/* fromKeyboard= */ false);
      }
    };
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('checked')) {
      this.setAttribute('aria-pressed', this.checked ? 'true' : 'false');
    }

    if (changedProperties.has('disabled')) {
      this.setAttribute('tabindex', this.disabled ? '-1' : '0');
      this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    }
  }

  private hideRipple_() {
    this.getRipple().clear();
  }

  private onPointerUp_() {
    assert(this.boundPointerMove_);
    this.removeEventListener('pointermove', this.boundPointerMove_);
    this.hideRipple_();
  }

  private onPointerDown_(e: PointerEvent) {
    // Don't do anything if this was not a primary button click or touch event.
    if (e.button !== 0) {
      return;
    }

    // This is necessary to have follow up pointer events fire on |this|, even
    // if they occur outside of its bounds.
    this.setPointerCapture(e.pointerId);
    this.pointerDownX_ = e.clientX;
    this.handledInPointerMove_ = false;
    assert(this.boundPointerMove_);
    this.addEventListener('pointermove', this.boundPointerMove_);
  }

  private onClick_(e: Event) {
    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    e.stopPropagation();
    e.preventDefault();

    // User gesture has already been taken care of inside |pointermove|
    // handlers, Do nothing here.
    if (this.handledInPointerMove_) {
      return;
    }

    // If no pointermove event fired, then user just clicked on the
    // toggle button and therefore it should be toggled.
    this.toggleState_(/* fromKeyboard= */ false);
  }

  private async toggleState_(fromKeyboard: boolean) {
    // Ignore cases where the 'click' or 'keypress' handlers are triggered while
    // disabled.
    if (this.disabled) {
      return;
    }

    if (!fromKeyboard) {
      this.hideRipple_();
    }

    this.checked = !this.checked;

    // Yield, so that 'checked-changed' (originating from `notify: 'true'`) fire
    // before the 'change' event below, which guarantees that any Polymer parent
    // with 2-way bindings on the `checked` attribute are updated first.
    await this.updateComplete;

    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: this.checked}));
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
      this.toggleState_(/* fromKeyboard= */ true);
    }
  }

  private onKeyUp_(e: KeyboardEvent) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    if (e.key === ' ') {
      this.toggleState_(/* fromKeyboard= */ true);
    }
  }

  // Overridden from CrRippleMixin
  override createRipple() {
    this.rippleContainer = this.$.knob;
    const ripple = super.createRipple();
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toggle': CrToggleElement;
  }
}

customElements.define(CrToggleElement.is, CrToggleElement);
