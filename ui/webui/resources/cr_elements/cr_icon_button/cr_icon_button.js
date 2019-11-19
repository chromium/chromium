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
 * --cr-icon-button-fill-color-focus
 * --cr-icon-button-stroke-color
 * --cr-icon-button-stroke-color-focus
 *
 * When not using iron-icon (ie. specifying --cr-icon-image), the icons support
 * one color and the 'stroke' variables are ignored.
 *
 * The '-focus' variables are used for opaque ripple support. This is enabled
 * when the 'a11y-enhanced' attribute on <html> is present.
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

    ironIcon: {
      type: String,
      observer: 'onIronIconChanged_',
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
    blur: 'hideRipple_',
    click: 'onClick_',
    down: 'showRipple_',
    focus: 'showRipple_',
    keydown: 'onKeyDown_',
    keyup: 'onKeyUp_',
    pointerdown: 'ensureRipple',
    up: 'hideRipple_',
  },

  /** @private */
  hideRipple_: function() {
    if (this.hasRipple()) {
      this.getRipple().clear();
      this.rippleShowing_ = false;
    }
  },

  /** @private */
  showRipple_: function() {
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
  disabledChanged_: function(newValue, oldValue) {
    if (!newValue && oldValue == undefined) {
      return;
    }
    if (this.disabled) {
      this.blur();
    }
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    this.setAttribute('tabindex', this.disabled ? '-1' : '0');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onClick_: function(e) {
    if (this.disabled) {
      e.stopImmediatePropagation();
    }
  },

  /** @private */
  onIronIconChanged_: function() {
    this.shadowRoot.querySelectorAll('iron-icon').forEach(el => el.remove());
    if (!this.ironIcon) {
      return;
    }
    const icons = (this.ironIcon || '').split(',');
    icons.forEach(icon => {
      const element = document.createElement('iron-icon');
      element.icon = icon;
      this.$.icon.appendChild(element);
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
  onKeyDown_: function(e) {
    if (e.key != ' ' && e.key != 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    if (e.repeat) {
      return;
    }

    if (e.key == 'Enter') {
      this.click();
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyUp_: function(e) {
    if (e.key == ' ' || e.key == 'Enter') {
      e.preventDefault();
      e.stopPropagation();
    }

    if (e.key == ' ') {
      this.click();
    }
  },

  // customize the element's ripple
  _createRipple: function() {
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
