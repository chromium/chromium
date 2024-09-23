// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element for indicating that this user is managed by
 * their organization. This component uses the |isManaged| boolean in
 * loadTimeData, and the |managedByOrg| i18n string.
 *
 * If |isManaged| is false, this component is hidden. If |isManaged| is true, it
 * becomes visible.
 */

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons_lit.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_footnote.css.js';
import {getHtml} from './managed_footnote.html.js';

const ManagedFootnoteElementBase =
    I18nMixinLit(WebUiListenerMixinLit(CrLitElement));

export class ManagedFootnoteElement extends ManagedFootnoteElementBase {
  static get is() {
    return 'managed-footnote';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Whether the user is managed by their organization through enterprise
       * policies.
       */
      isManaged_: {
        reflect: true,
        type: Boolean,
      },

      // <if expr="chromeos_ash">
      /**
       * Whether the device should be indicated as managed rather than the
       * browser.
       */
      showDeviceInfo: {
        type: Boolean,
      },
      // </if>

      /**
       * The name of the icon to display in the footer.
       * Should only be read if isManaged_ is true.
       */
      managedByIcon_: {
        reflect: true,
        type: String,
      },

    };
  }

  protected isManaged_: boolean = loadTimeData.getBoolean('isManaged');
  protected managedByIcon_: string = loadTimeData.getString('managedByIcon');

  // <if expr="chromeos_ash">
  showDeviceInfo: boolean = false;
  // </if>

  override firstUpdated() {
    this.addWebUiListener('is-managed-changed', (managed: boolean) => {
      loadTimeData.overrideValues({isManaged: managed});
      this.isManaged_ = managed;
    });
  }

  /** @return Message to display to the user. */
  protected getManagementString_(): TrustedHTML {
    // <if expr="chromeos_ash">
    if (this.showDeviceInfo) {
      return this.i18nAdvanced('deviceManagedByOrg');
    }
    // </if>
    return this.i18nAdvanced('browserManagedByOrg');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-footnote': ManagedFootnoteElement;
  }
}

customElements.define(ManagedFootnoteElement.is, ManagedFootnoteElement);

chrome.send('observeManagedUI');
