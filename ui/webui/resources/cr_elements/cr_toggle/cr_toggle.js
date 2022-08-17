// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Number of pixels required to move to consider the pointermove event as
 * intentional.
 * @type {number}
 */
export const MOVE_THRESHOLD_PX = 5;

/**
 * @fileoverview 'cr-toggle' is a component for showing an on/off switch. It
 * fires a 'change' event *only* when its state changes as a result of a user
 * interaction. Besides just clicking the element, its state can be changed by
 * dragging (pointerdown+pointermove) the element towards the desired direction.
 */
import {PaperRippleBehavior} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import '../shared_vars_css.m.js';

class PaperRippleBehaviorInterface {
  /** @return {!PaperRippleElement} */
  getRipple() {}
}

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {PaperRippleBehaviorInterface}
 */
const CrToggleElementBase =
    mixinBehaviors([PaperRippleBehavior], PolymerElement);

/** @polymer */
export class CrToggleElement extends CrToggleElementBase {
  static get is() {
    return 'cr-toggle';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'checkedChanged_',
        notify: true,
      },

      dark: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'disabledChanged_',
      },
    };
  }

  constructor() {
    super();
    /** @private {?Function} */
    this.boundPointerMove_ = null;

    /**
     * Whether the state of the toggle has already taken into account by
     * |pointeremove| handlers. Used in the 'click' handler.
     * @private {boolean}
     */
    this.handledInPointerMove_ = false;

    /** @private {?number} */
    this.pointerDownX_ = null;

    /** @type {!Element} */
    this._rippleContainer;
  }

  /** @override */
  ready() {
    super.ready();
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'button');
    }
    if (!this.hasAttribute('tabindex')) {
      this.setAttribute('tabindex', '0');
    }
    this.setAttribute('aria-pressed', 'false');
    this.setAttribute('aria-disabled', 'false');
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.addEventListener('blur', this.hideRipple_.bind(this));
    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener('focus', this.onFocus_.bind(this));
    this.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
    this.addEventListener(
        'keyup', e => this.onKeyUp_(/** @type {!KeyboardEvent} */ (e)));
    this.addEventListener(
        'pointerdown',
        e => this.onPointerDown_(/** @type {!PointerEvent} */ (e)));
    this.addEventListener('pointerup', this.onPointerUp_.bind(this));

    const direction =
        this.matches(':host-context([dir=rtl]) cr-toggle') ? -1 : 1;
    this.boundPointerMove_ = (e) => {
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

  /** @private */
  checkedChanged_() {
    this.setAttribute('aria-pressed', this.checked ? 'true' : 'false');
  }

  /** @private */
  disabledChanged_() {
    this.setAttribute('tabindex', this.disabled ? -1 : 0);
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  }

  /** @private */
  onFocus_() {
    this.getRipple().showAndHoldDown();
  }

  /** @private */
  hideRipple_() {
    this.getRipple().clear();
  }

  /** @private */
  onPointerUp_() {
    this.removeEventListener('pointermove', this.boundPointerMove_);
    this.hideRipple_();
  }

  /**
   * @param {!PointerEvent} e
   * @private
   */
  onPointerDown_(e) {
    // Don't do anything if this was not a primary button click or touch event.
    if (e.button !== 0) {
      return;
    }

    // This is necessary to have follow up pointer events fire on |this|, even
    // if they occur outside of its bounds.
    this.setPointerCapture(e.pointerId);
    this.pointerDownX_ = e.clientX;
    this.handledInPointerMove_ = false;
    this.addEventListener('pointermove', this.boundPointerMove_);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onClick_(e) {
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

  /**
   * @param {boolean} fromKeyboard
   * @private
   */
  toggleState_(fromKeyboard) {
    // Ignore cases where the 'click' or 'keypress' handlers are triggered while
    // disabled.
    if (this.disabled) {
      return;
    }

    if (!fromKeyboard) {
      this.hideRipple_();
    }

    this.checked = !this.checked;
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: this.checked}));
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
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

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyUp_(e) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    if (e.key === ' ') {
      this.toggleState_(/* fromKeyboard= */ true);
    }
  }

  // customize the element's ripple
  _createRipple() {
    this._rippleContainer = this.$.knob;
    const ripple = PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  }
}

customElements.define(CrToggleElement.is, CrToggleElement);
