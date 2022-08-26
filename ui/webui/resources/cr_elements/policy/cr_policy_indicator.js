// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Polymer element for indicating policies by type. */
import '../hidden_style_css.m.js';
import './cr_tooltip_icon.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyIndicatorBehavior, CrPolicyIndicatorBehaviorInterface, CrPolicyIndicatorType} from './cr_policy_indicator_behavior.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrPolicyIndicatorBehaviorInterface}
 */
const CrPolicyIndicatorElementBase =
    mixinBehaviors([CrPolicyIndicatorBehavior], PolymerElement);

/** @polymer */
class CrPolicyIndicatorElement extends CrPolicyIndicatorElementBase {
  static get is() {
    return 'cr-policy-indicator';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      iconAriaLabel: String,

      /** @private {string} */
      indicatorTooltip_: {
        type: String,
        computed: 'getIndicatorTooltip_(indicatorType, indicatorSourceName)',
      },
    };
  }

  /**
   * @param {!CrPolicyIndicatorType} indicatorType
   * @param {string} indicatorSourceName The name associated with the indicator.
   *     See chrome.settingsPrivate.PrefObject.controlledByName
   * @return {string} The tooltip text for |type|.
   */
  getIndicatorTooltip_(indicatorType, indicatorSourceName) {
    return this.getIndicatorTooltip(indicatorType, indicatorSourceName);
  }
}

customElements.define(CrPolicyIndicatorElement.is, CrPolicyIndicatorElement);
