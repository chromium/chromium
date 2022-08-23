// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-button' is a button which displays slotted elements. It can
 * be interacted with like a normal button using click as well as space and
 * enter to effectively click the button and fire a 'click' event.
 */
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../hidden_style_css.m.js';
import '../shared_vars_css.m.js';

import {PaperRippleBehavior} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FocusOutlineManager} from '../../js/cr/ui/focus_outline_manager.m.js';

/** @interface */
class PaperRippleBehaviorInterface {
  /** @return {Element} */
  getRipple() {}

  ensureRipple() {}
}

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {PaperRippleBehaviorInterface}
 */
const CrButtonElementBase =
    mixinBehaviors([PaperRippleBehavior], PolymerElement);

/** @polymer */
export class CrButtonElement extends CrButtonElementBase {
  static get is() {
    return 'cr-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'disabledChanged_',
      },

      /**
       * Use this property in order to configure the "tabindex" attribute.
       */
      customTabIndex: {
        type: Number,
        observer: 'applyTabIndex_',
      },

      /**
       * Flag used for formatting ripples on circle shaped cr-buttons.
       * @private
       */
      circleRipple: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /**
     * It is possible to activate a tab when the space key is pressed down. When
     * this element has focus, the keyup event for the space key should not
     * perform a 'click'. |spaceKeyDown_| tracks when a space pressed and
     * handled by this element. Space keyup will only result in a 'click' when
     * |spaceKeyDown_| is true. |spaceKeyDown_| is set to false when element
     * loses focus.
     * @private {boolean}
     */
    this.spaceKeyDown_ = false;

    /** @private {Set<number>} */
    this.timeoutIds_ = null;

    this.addEventListener('blur', this.onBlur_.bind(this));
    // Must be added in constructor so that stopImmediatePropagation() works as
    // expected.
    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
    this.addEventListener(
        'keyup', e => this.onKeyUp_(/** @type {!KeyboardEvent} */ (e)));
    this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
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
    if (!this.hasAttribute('aria-disabled')) {
      this.setAttribute('aria-disabled', 'false');
    }

    FocusOutlineManager.forDocument(document);
    this.timeoutIds_ = new Set();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.timeoutIds_.forEach(clearTimeout);
    this.timeoutIds_.clear();
  }

  /**
   * @param {!Function} fn
   * @param {number=} delay
   * @private
   */
  setTimeout_(fn, delay) {
    if (!this.isConnected) {
      return;
    }
    const id = setTimeout(() => {
      this.timeoutIds_.delete(id);
      fn();
    }, delay);
    this.timeoutIds_.add(id);
  }

  /**
   * @param {boolean} newValue
   * @param {boolean|undefined} oldValue
   * @private
   */
  disabledChanged_(newValue, oldValue) {
    if (!newValue && oldValue === undefined) {
      return;
    }
    if (this.disabled) {
      this.blur();
    }
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    this.applyTabIndex_();
  }

  /**
   * Updates the tabindex HTML attribute to the actual value.
   * @private
   */
  applyTabIndex_() {
    let value = this.customTabIndex;
    if (value === undefined) {
      value = this.disabled ? -1 : 0;
    }
    this.setAttribute('tabindex', value.toString());
  }

  /** @private */
  onBlur_() {
    this.spaceKeyDown_ = false;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onClick_(e) {
    if (this.disabled) {
      e.stopImmediatePropagation();
    }
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

    this.getRipple().uiDownAction();
    if (e.key === 'Enter') {
      this.click();
      // Delay was chosen manually as a good time period for the ripple to be
      // visible.
      this.setTimeout_(() => this.getRipple().uiUpAction(), 100);
    } else if (e.key === ' ') {
      this.spaceKeyDown_ = true;
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

    if (this.spaceKeyDown_ && e.key === ' ') {
      this.spaceKeyDown_ = false;
      this.click();
      this.getRipple().uiUpAction();
    }
  }

  /** @private */
  onPointerDown_() {
    this.ensureRipple();
  }

  /**
   * Customize the element's ripple. Overriding the '_createRipple' function
   * from PaperRippleBehavior.
   * @return {PaperRippleElement}
   */
  _createRipple() {
    const ripple = super._createRipple();

    if (this.circleRipple) {
      ripple.setAttribute('center', '');
      ripple.classList.add('circle');
    }

    return ripple;
  }
}

customElements.define(CrButtonElement.is, CrButtonElement);
