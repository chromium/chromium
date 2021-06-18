// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating that this user is managed by
 * their organization. This component uses the |isManaged| boolean in
 * loadTimeData, and the |managedByOrg| i18n string.
 *
 * If |isManaged| is false, this component is hidden. If |isManaged| is true, it
 * becomes visible.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../../cr_elements/icons.m.js';
import '../../cr_elements/shared_vars_css.m.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, I18nBehaviorInterface} from '../../js/i18n_behavior.m.js';
import {loadTimeData} from '../../js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '../../js/web_ui_listener_behavior.m.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const ManagedFootnoteElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
class ManagedFootnoteElement extends ManagedFootnoteElementBase {
  static get is() {
    return 'managed-footnote';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Whether the user is managed by their organization through enterprise
       * policies.
       * @type {boolean}
       * @private
       */
      isManaged_: {
        reflectToAttribute: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isManaged');
        },
      },

      /**
       * Whether the device should be indicated as managed rather than the
       * browser.
       * @type {boolean}
       */
      showDeviceInfo: {
        type: Boolean,
        value: false,
      }
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.addWebUIListener('is-managed-changed', managed => {
      loadTimeData.overrideValues({isManaged: managed});
      this.isManaged_ = managed;
    });
  }

  /**
   * @return {string} Message to display to the user.
   * @private
   */
  getManagementString_() {
    // <if expr="chromeos">
    if (this.showDeviceInfo) {
      return this.i18nAdvanced('deviceManagedByOrg');
    }
    // </if>
    return this.i18nAdvanced('browserManagedByOrg');
  }
}

customElements.define(ManagedFootnoteElement.is, ManagedFootnoteElement);

chrome.send('observeManagedUI');
