// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './shared_style.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '//resources/cr_elements/icons.m.js';

import {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export interface AppManagementToggleRowElement {
  $: {toggle: CrToggleElement}
}

export class AppManagementToggleRowElement extends PolymerElement {
  static get is() {
    return 'app-management-toggle-row';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      icon: String,
      label: String,
      managed: {type: Boolean, value: false, reflectToAttribute: true},
      value: {type: Boolean, value: false, reflectToAttribute: true},
      description: String,
    };
  }

  override ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
  }

  isChecked(): boolean {
    return this.$.toggle.checked;
  }

  setToggle(value: boolean) {
    this.$.toggle.checked = value;
  }

  private onClick_(event: Event) {
    event.stopPropagation();
    this.$.toggle.click();
  }
}

customElements.define(
    AppManagementToggleRowElement.is, AppManagementToggleRowElement);
