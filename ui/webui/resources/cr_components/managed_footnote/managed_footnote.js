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

(function() {
Polymer({
  is: 'managed-footnote',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Whether the user is managed by their organization through enterprise
     * policies.
     * @type {boolean}
     * @private
     */
    isManaged_: {
      reflectToAttribute: true,
      type: Boolean,
      value: function() {
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
  },

  /** @override */
  ready: function() {
    this.addWebUIListener('is-managed-changed', managed => {
      loadTimeData.overrideValues({isManaged: managed});
      this.isManaged_ = managed;
    });
  },

  /**
   * @return {string} Message to display to the user.
   * @private
   */
  getManagementString_: function() {
    // <if expr="chromeos">
    if (this.showDeviceInfo) {
      return this.i18nAdvanced('deviceManagedByOrg');
    }
    // </if>
    return this.i18nAdvanced('browserManagedByOrg');
  },
});

chrome.send('observeManagedUI');
})();
