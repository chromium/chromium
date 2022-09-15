// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_management_shared_style.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';
import '//resources/cr_elements/icons.html.js';

import {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './toggle_row.html.js';

export interface AppManagementToggleRowElement {
  $: {toggle: CrToggleElement};
}

export class AppManagementToggleRowElement extends PolymerElement {
  static get is() {
    return 'app-management-toggle-row';
  }

  static get template() {
    return getTemplate();
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
