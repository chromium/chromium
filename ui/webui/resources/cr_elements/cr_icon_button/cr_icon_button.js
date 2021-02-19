// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-icon-button' is a button which displays an icon with a
 * ripple. It can be interacted with like a normal button using click as well as
 * space and enter to effectively click the button and fire a 'click' event.
 *
 * There are two sources to icons, cr-icons and iron-iconset-svg. The cr-icon's
 * are defined as background images with a reference to a resource file
 * associated with a CSS class name. The iron-icon's are defined as inline SVG's
 * under a key that is stored in a global map that is accessible to the
 * iron-icon element.
 *
 * Example of using a cr-icon:
 * <link rel="import" href="chrome://resources/cr_elements/cr_icons_css.html">
 * <dom-module id="module">
 *   <template>
 *     <style includes="cr-icons"></style>
 *     <cr-icon-button class="icon-class-name"></cr-icon-button>
 *   </template>
 * </dom-module>
 *
 * In general when an icon is specified using a class, the expectation is the
 * class will set an image to the --cr-icon-image variable.
 *
 * Example of using an iron-icon:
 * <link rel="import" href="chrome://resources/cr_elements/icons.html">
 * <cr-icon-button iron-icon="cr:icon-key"></cr-icon-button>
 *
 * The color of the icon can be overridden using CSS variables. When using
 * iron-icon both the fill and stroke can be overridden the variables:
 * --cr-icon-button-fill-color
 * --cr-icon-button-stroke-color
 *
 * When not using iron-icon (ie. specifying --cr-icon-image), the icons support
 * one color and the 'stroke' variables are ignored.
 *
 * When using iron-icon's, more than one icon can be specified by setting
 * the |ironIcon| property to a comma-delimited list of keys.
 */
Polymer({
  is: 'cr-icon-button',

  behaviors: [
    Polymer.PaperRippleBehavior,
  ],

  properties: {
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

    ironIcon: {
      type: String,
      observer: 'onIronIconChanged_',
      reflectToAttribute: true,
    },

    noRippleOnFocus: {
      type: Boolean,
      value: false,
    },

    /** @private */
    multipleIcons_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private */
    rippleShowing_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  hostAttributes: {
    'aria-disabled': 'false',
    role: 'button',
    tabindex: 0,
  },

  listeners: {
    blur: 'onBlur_',
    click: 'onClick_',
    down: 'showRipple_',
    focus: 'onFocus_',
    keydown: 'onKeyDown_',
    keyup: 'onKeyUp_',
    pointerdown: 'ensureRipple',
    up: 'hideRipple_',
  },

  /**
   * It is possible to activate a tab when the space key is pressed down. When
   * this element has focus, the keyup event for the space key should not
   * perform a 'click'. |spaceKeyDown_| tracks when a space pressed and handled
   * by this element. Space keyup will only result in a 'click' when
   * |spaceKeyDown_| is true. |spaceKeyDown_| is set to false when element loses
   * focus.
   * @private {boolean}
   */
  spaceKeyDown_: false,

  /** @private */
  hideRipple_() {
    if (this.hasRipple()) {
      this.getRipple().clear();
      this.rippleShowing_ = false;
    }
  },

  /** @private */
  showRipple_() {
    if (!this.noink && !this.disabled) {
      this.getRipple().showAndHoldDown();
      this.rippleShowing_ = true;
    }
  },

  /**
   * @param {boolean} newValue
   * @param {boolean} oldValue
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
  },

  /**
   * Updates the tabindex HTML attribute to the actual value.
   * @private
   */
  applyTabIndex_() {
    let value = this.customTabIndex;
    if (value === undefined) {
      value = this.disabled ? -1 : 0;
    }
    this.setAttribute('tabindex', value);
  },

  /** @private */
  onFocus_() {
    if (!this.noRippleOnFocus) {
      this.showRipple_();
    }
  },

  /** @private */
  onBlur_() {
    this.spaceKeyDown_ = false;

    if (!this.noRippleOnFocus) {
      this.hideRipple_();
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onClick_(e) {
    if (this.disabled) {
      e.stopImmediatePropagation();
    }
  },

  /** @private */
  onIronIconChanged_() {
    this.shadowRoot.querySelectorAll('iron-icon').forEach(el => el.remove());
    if (!this.ironIcon) {
      return;
    }
    const icons = (this.ironIcon || '').split(',');
    this.multipleIcons_ = icons.length > 1;
    icons.forEach(icon => {
      const ironIcon = document.createElement('iron-icon');
      ironIcon.icon = icon;
      this.$.icon.appendChild(ironIcon);
      if (ironIcon.shadowRoot) {
        ironIcon.shadowRoot.querySelectorAll('svg', 'img')
            .forEach(child => child.setAttribute('role', 'none'));
      }
    });
    if (!this.hasRipple()) {
      return;
    }
    if (icons.length > 1) {
      this.getRipple().classList.remove('circle');
    } else {
      this.getRipple().classList.add('circle');
    }
  },

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
      this.click();
    } else if (e.key === ' ') {
      this.spaceKeyDown_ = true;
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyUp_(e) {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
    }

    if (this.spaceKeyDown_ && e.key === ' ') {
      this.spaceKeyDown_ = false;
      this.click();
    }
  },

  // customize the element's ripple
  _createRipple() {
    this._rippleContainer = this.$.icon;
    const ripple = Polymer.PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    if (!(this.ironIcon || '').includes(',')) {
      ripple.classList.add('circle');
    }
    return ripple;
  },
});
/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');
