// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies that apply to an
 * element controlling a settings preference.
 */
import '../cr_hidden_style.css.js';
import './cr_tooltip_icon.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyIndicatorBehavior, CrPolicyIndicatorBehaviorInterface, CrPolicyIndicatorType} from './cr_policy_indicator_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrPolicyIndicatorBehaviorInterface}
 */
const CrPolicyPrefIndicatorElementBase =
    mixinBehaviors([CrPolicyIndicatorBehavior], PolymerElement);

/** @polymer */
class CrPolicyPrefIndicatorElement extends CrPolicyPrefIndicatorElementBase {
  static get is() {
    return 'cr-policy-pref-indicator';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      iconAriaLabel: String,

      /**
       * Which indicator type to show (or NONE).
       * @type {CrPolicyIndicatorType}
       * @override
       */
      indicatorType: {
        type: String,
        value: CrPolicyIndicatorType.NONE,
        computed: 'getIndicatorTypeForPref_(pref.*, associatedValue)',
      },

      indicatorTooltip: {
        type: String,
        computed: 'getIndicatorTooltipForPref_(indicatorType, pref.*)',
      },

      /**
       * Optional preference object associated with the indicator. Initialized
       * to null so that computed functions will get called if this is never
       * set.
       * @type {!chrome.settingsPrivate.PrefObject|undefined}
       */
      pref: Object,

      /**
       * Optional value for the preference value this indicator is associated
       * with. If this is set, no indicator will be shown if it is a member
       * of |pref.userSelectableValues| and is not |pref.recommendedValue|.
       * @type {*}
       */
      associatedValue: Object,
    };
  }

  /**
   * @return {CrPolicyIndicatorType} The indicator type based on |pref| and
   *    |associatedValue|.
   */
  getIndicatorTypeForPref_() {
    const {enforcement, userSelectableValues, controlledBy, recommendedValue} =
        this.pref;
    if (enforcement === chrome.settingsPrivate.Enforcement.RECOMMENDED) {
      if (this.associatedValue !== undefined &&
          this.associatedValue !== recommendedValue) {
        return CrPolicyIndicatorType.NONE;
      }
      return CrPolicyIndicatorType.RECOMMENDED;
    }
    if (enforcement === chrome.settingsPrivate.Enforcement.ENFORCED) {
      // An enforced preference may also have some values still available for
      // the user to select from.
      if (userSelectableValues !== undefined) {
        if (recommendedValue && this.associatedValue === recommendedValue) {
          return CrPolicyIndicatorType.RECOMMENDED;
        } else if (userSelectableValues.includes(this.associatedValue)) {
          return CrPolicyIndicatorType.NONE;
        }
      }
      switch (controlledBy) {
        case chrome.settingsPrivate.ControlledBy.EXTENSION:
          return CrPolicyIndicatorType.EXTENSION;
        case chrome.settingsPrivate.ControlledBy.PRIMARY_USER:
          return CrPolicyIndicatorType.PRIMARY_USER;
        case chrome.settingsPrivate.ControlledBy.OWNER:
          return CrPolicyIndicatorType.OWNER;
        case chrome.settingsPrivate.ControlledBy.USER_POLICY:
          return CrPolicyIndicatorType.USER_POLICY;
        case chrome.settingsPrivate.ControlledBy.DEVICE_POLICY:
          return CrPolicyIndicatorType.DEVICE_POLICY;
        case chrome.settingsPrivate.ControlledBy.PARENT:
          return CrPolicyIndicatorType.PARENT;
        case chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION:
          return CrPolicyIndicatorType.CHILD_RESTRICTION;
      }
    }
    if (enforcement === chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED) {
      return CrPolicyIndicatorType.PARENT;
    }
    return CrPolicyIndicatorType.NONE;
  }

  /**
   * @return {string} The tooltip text for |indicatorType|.
   * @private
   */
  getIndicatorTooltipForPref_() {
    if (!this.pref) {
      return '';
    }

    const matches = this.pref && this.pref.value === this.pref.recommendedValue;
    return this.getIndicatorTooltip(
        this.indicatorType, this.pref.controlledByName || '', matches);
  }

  /** @return {!Element} */
  getFocusableElement() {
    return this.shadowRoot.querySelector('cr-tooltip-icon')
        .getFocusableElement();
  }
}

customElements.define(
    CrPolicyPrefIndicatorElement.is, CrPolicyPrefIndicatorElement);
