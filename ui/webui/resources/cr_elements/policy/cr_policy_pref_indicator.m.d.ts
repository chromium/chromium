// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/tools/typescript/definitions/settings_private.js';

import {CrPolicyIndicatorBehavior} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrPolicyPrefIndicatorElement extends CrPolicyIndicatorBehavior,
                                               LegacyElementMixin, HTMLElement {
  indicatorTooltip: string;
  pref: chrome.settingsPrivate.PrefObject;
}

export {CrPolicyPrefIndicatorElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-policy-pref-indicator': CrPolicyPrefIndicatorElement;
  }
}
