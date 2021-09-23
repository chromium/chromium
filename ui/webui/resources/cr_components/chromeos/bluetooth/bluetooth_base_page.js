// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Base template with elements common to all Bluetooth UI sub-pages.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNotReached} from '../../../js/assert.m.js';
import {ButtonBarState, ButtonName, ButtonState} from './bluetooth_types.js';

/**
 * @constructor
 * @implements {I18nBehaviorInterface}
 * @extends {PolymerElement}
 */
const SettingsBluetoothBasePageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothBasePageElement extends
    SettingsBluetoothBasePageElementBase {
  static get is() {
    return 'bluetooth-base-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Object representing the states of each button in the button bar.
       * @type {!ButtonBarState}
       */
      buttonBarState: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      /**
       * Used to access |ButtonName| type in HTML.
       * @type {!ButtonName}
       */
      ButtonName: {
        type: Object,
        value: ButtonName,
      },
    };
  }

  /** @private */
  onCancelClick_() {
    this.dispatchEvent(new CustomEvent('cancel', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @private */
  onPairClick_() {
    this.dispatchEvent(new CustomEvent('pair', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * @param {!ButtonName} buttonName
   * @return {boolean}
   * @private
   */
  shouldShowButton_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state !== ButtonState.HIDDEN;
  }

  /**
   * @param {!ButtonName} buttonName
   * @return {boolean}
   * @private
   */
  isButtonDisabled_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state === ButtonState.DISABLED;
  }

  /**
   * @param {!ButtonName} buttonName
   * @return {!ButtonState}
   * @private
   */
  getButtonBarState_(buttonName) {
    switch (buttonName) {
      case ButtonName.CANCEL:
        return this.buttonBarState.cancel;
      case ButtonName.PAIR:
        return this.buttonBarState.pair;
      default:
        assertNotReached();
        return ButtonState.ENABLED;
    }
  }
}

customElements.define(
    SettingsBluetoothBasePageElement.is, SettingsBluetoothBasePageElement);
