// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Lit element for indicating policies by type. */
import './cr_tooltip_icon.js';

import {assertNotReached} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_policy_indicator.css.js';
import {getHtml} from './cr_policy_indicator.html.js';
import {CrPolicyIndicatorType} from './cr_policy_types.js';


export class CrPolicyIndicatorElement extends CrLitElement {
  static get is() {
    return 'cr-policy-indicator';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      iconAriaLabel: {type: String},

      /**
       * Which indicator type to show (or NONE).
       */
      indicatorType: {type: String},

      /**
       * The name associated with the policy source. See
       * chrome.settingsPrivate.PrefObject.controlledByName.
       */
      indicatorSourceName: {type: String},
    };
  }

  iconAriaLabel: string = '';
  indicatorType: CrPolicyIndicatorType = CrPolicyIndicatorType.NONE;
  indicatorSourceName: string = '';

  /**
   * @return True if the indicator should be shown.
   */
  protected getIndicatorVisible_(): boolean {
    return this.indicatorType !== CrPolicyIndicatorType.NONE;
  }

  /**
   * @return The iron-icon icon name.
   */
  protected getIndicatorIcon_(): string {
    switch (this.indicatorType) {
      case CrPolicyIndicatorType.EXTENSION:
        return 'cr:extension';
      case CrPolicyIndicatorType.NONE:
        return '';
      case CrPolicyIndicatorType.PRIMARY_USER:
        return 'cr:group';
      case CrPolicyIndicatorType.OWNER:
        return 'cr:person';
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
      case CrPolicyIndicatorType.RECOMMENDED:
        return 'cr20:domain';
      case CrPolicyIndicatorType.PARENT:
      case CrPolicyIndicatorType.CHILD_RESTRICTION:
        return 'cr20:kite';
      default:
        assertNotReached();
    }
  }

  /**
   * @param indicatorSourceName The name associated with the indicator.
   *     See chrome.settingsPrivate.PrefObject.controlledByName
   * @return The tooltip text for |type|.
   */
  protected getIndicatorTooltip_(): string {
    if (!window.CrPolicyStrings) {
      return '';
    }  // Tooltips may not be defined, e.g. in OOBE.

    const CrPolicyStrings = window.CrPolicyStrings;
    switch (this.indicatorType) {
      case CrPolicyIndicatorType.EXTENSION:
        return this.indicatorSourceName.length > 0 ?
            CrPolicyStrings.controlledSettingExtension!.replace(
                '$1', this.indicatorSourceName) :
            CrPolicyStrings.controlledSettingExtensionWithoutName!;
      // <if expr="chromeos_ash">
      case CrPolicyIndicatorType.PRIMARY_USER:
        return CrPolicyStrings.controlledSettingShared!.replace(
            '$1', this.indicatorSourceName);
      case CrPolicyIndicatorType.OWNER:
        return this.indicatorSourceName.length > 0 ?
            CrPolicyStrings.controlledSettingWithOwner!.replace(
                '$1', this.indicatorSourceName) :
            CrPolicyStrings.controlledSettingNoOwner!;
      // </if>
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
        return CrPolicyStrings.controlledSettingPolicy!;
      case CrPolicyIndicatorType.RECOMMENDED:
        return CrPolicyStrings.controlledSettingRecommendedDiffers!;
      case CrPolicyIndicatorType.PARENT:
        return CrPolicyStrings.controlledSettingParent!;
      case CrPolicyIndicatorType.CHILD_RESTRICTION:
        return CrPolicyStrings.controlledSettingChildRestriction!;
    }
    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-policy-indicator': CrPolicyIndicatorElement;
  }
}

customElements.define(CrPolicyIndicatorElement.is, CrPolicyIndicatorElement);
