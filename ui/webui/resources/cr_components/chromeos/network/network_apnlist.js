// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying a list of cellular
 * access points.
 */
(function() {
'use strict';

const kDefaultAccessPointName = 'NONE';
const kOtherAccessPointName = 'Other';

Polymer({
  is: 'network-apnlist',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: {
      type: Object,
      observer: 'managedPropertiesChanged_',
    },

    /**
     * accessPointName value of the selected APN.
     * @private
     */
    selectedApn_: {
      type: String,
      value: '',
    },

    /**
     * Selectable list of APN dictionaries for the UI. Includes an entry
     * corresponding to |otherApn| (see below).
     * @private {!Array<!chromeos.networkConfig.mojom.ApnProperties>}
     */
    apnSelectList_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * The user settable properties for a new ('other') APN. The values for
     * accessPointName, username, and password will be set to the currently
     * active APN if it does not match an existing list entry.
     * @private {chromeos.networkConfig.mojom.ApnProperties|undefined}
     */
    otherApn_: {
      type: Object,
    },

    /**
     * Array of property names to pass to the Other APN property list.
     * @private {!Array<string>}
     */
    otherApnFields_: {
      type: Array,
      value: function() {
        return ['accessPointName', 'username', 'password'];
      },
      readOnly: true
    },

    /**
     * Array of edit types to pass to the Other APN property list.
     * @private
     */
    otherApnEditTypes_: {
      type: Object,
      value: function() {
        return {
          'accessPointName': 'String',
          'username': 'String',
          'password': 'Password'
        };
      },
      readOnly: true
    },
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedApnProperties} apn
   * @return {!chromeos.networkConfig.mojom.ApnProperties}
   * @private
   */
  getApnFromManaged_: function(apn) {
    return {
      accessPointName: OncMojo.getActiveString(apn.accessPointName),
      authentication: OncMojo.getActiveString(apn.authentication),
      language: OncMojo.getActiveString(apn.language),
      localizedName: OncMojo.getActiveString(apn.localizedName),
      name: OncMojo.getActiveString(apn.name),
      password: OncMojo.getActiveString(apn.password),
      username: OncMojo.getActiveString(apn.username),
    };
  },

  /** @private*/
  managedPropertiesChanged_: function() {
    const cellular = this.managedProperties.typeProperties.cellular;
    /** @type {!chromeos.networkConfig.mojom.ApnProperties|undefined} */ let
        activeApn;
    if (cellular.apn) {
      activeApn = this.getApnFromManaged_(cellular.apn);
    } else if (cellular.lastGoodApn && cellular.lastGoodApn.accessPointName) {
      activeApn = cellular.lastGoodApn;
    }
    this.setApnSelectList_(activeApn);
  },

  /**
   * Sets the list of selectable APNs for the UI. Appends an 'Other' entry
   * (see comments for |otherApn_| above).
   * @param {chromeos.networkConfig.mojom.ApnProperties|undefined} activeApn The
   *     currently active APN properties.
   * @private
   */
  setApnSelectList_: function(activeApn) {
    // Copy the list of APNs from this.managedProperties.
    const apnList = this.getApnList_().slice();

    // Test whether |activeApn| is in the current APN list in managedProperties.
    const activeApnInList = activeApn && apnList.some(function(a) {
      return a.accessPointName == activeApn.accessPointName;
    });

    // If |activeApn| is specified and not in the list, use the active
    // properties for 'other'. Otherwise use any existing 'other' properties.
    const otherApnProperties =
        (activeApn && !activeApnInList) ? activeApn : this.otherApn_;
    const otherApn = this.createApnObject_(otherApnProperties);

    // Always use 'Other' for the name of custom APN entries (the name does
    // not get saved).
    otherApn.name = kOtherAccessPointName;

    // If no 'active' or 'other' AccessPointName was provided, use the default.
    otherApn.accessPointName =
        otherApn.accessPointName || kDefaultAccessPointName;

    // Save the 'other' properties.
    this.otherApn_ = otherApn;

    // Append 'other' to the end of the list of APNs.
    apnList.push(otherApn);

    this.apnSelectList_ = apnList;

    // Set selectedApn_ after dom-repeat has been stamped.
    this.async(() => {
      this.selectedApn_ =
          (activeApn && activeApn.accessPointName) || otherApn.accessPointName;
    });
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ApnProperties|undefined=}
   *     apnProperties
   * @return {!chromeos.networkConfig.mojom.ApnProperties} A new APN object with
   *     properties from |apnProperties| if provided.
   * @private
   */
  createApnObject_: function(apnProperties) {
    const newApn = {accessPointName: ''};
    if (apnProperties) {
      Object.assign(newApn, apnProperties);
    }
    return newApn;
  },

  /**
   * @return {!Array<!chromeos.networkConfig.mojom.ApnProperties>} The list of
   *     APN properties in |managedProperties| or an empty list if the property
   *     is not set.
   * @private
   */
  getApnList_: function() {
    if (!this.managedProperties) {
      return [];
    }
    const apnList = this.managedProperties.typeProperties.cellular.apnList;
    if (!apnList) {
      return [];
    }
    return apnList.activeValue;
  },

  /**
   * Event triggered when the selectApn selection changes.
   * @param {!Event} event
   * @private
   */
  onSelectApnChange_: function(event) {
    const target = /** @type {!HTMLSelectElement} */ (event.target);
    const accessPointName = target.value;
    // When selecting 'Other', don't set a change event unless a valid
    // non-default value has been set for Other.
    if (this.isOtherSelected_(accessPointName) &&
        (!this.otherApn_ || !this.otherApn_.accessPointName ||
         this.otherApn_.accessPointName == kDefaultAccessPointName)) {
      this.selectedApn_ = accessPointName;
      return;
    }
    this.sendApnChange_(accessPointName);
  },

  /**
   * Event triggered when any 'Other' APN network property changes.
   * @param {!CustomEvent<!{field: string, value: string}>} event
   * @private
   */
  onOtherApnChange_: function(event) {
    // TODO(benchan/stevenjb): Move the toUpperCase logic to shill or
    // onc_translator_onc_to_shill.cc.
    const value = (event.detail.field == 'accessPointName') ?
        event.detail.value.toUpperCase() :
        event.detail.value;
    this.set('otherApn_.' + event.detail.field, value);
    // Don't send a change event for 'Other' until the 'Save' button is tapped.
  },

  /**
   * Event triggered when the Other APN 'Save' button is tapped.
   * @param {!Event} event
   * @private
   */
  onSaveOtherTap_: function(event) {
    this.sendApnChange_(this.selectedApn_);
  },

  /**
   * Send the apn-change event.
   * @param {string} accessPointName
   * @private
   */
  sendApnChange_: function(accessPointName) {
    const apnList = this.getApnList_();
    let apn = this.findApnInList_(apnList, accessPointName);
    if (apn == undefined) {
      apn = this.createApnObject_();
      if (this.otherApn_) {
        apn.accessPointName = this.otherApn_.accessPointName;
        apn.username = this.otherApn_.username;
        apn.password = this.otherApn_.password;
      }
    }
    this.fire('apn-change', apn);
  },

  /**
   * @param {string} accessPointName
   * @return {boolean} True if the 'other' APN is currently selected.
   * @private
   */
  isOtherSelected_: function(accessPointName) {
    if (!this.managedProperties) {
      return false;
    }
    const apnList = this.getApnList_();
    const apn = this.findApnInList_(apnList, accessPointName);
    return apn == undefined;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ApnProperties} apn
   * @return {string} The most descriptive name for the access point.
   * @private
   */
  apnDesc_: function(apn) {
    return apn.localizedName || apn.name || apn.accessPointName;
  },

  /**
   * @param {!Array<!chromeos.networkConfig.mojom.ApnProperties>} apnList
   * @param {string} accessPointName
   * @return {chromeos.networkConfig.mojom.ApnProperties|undefined} The entry in
   *     |apnList| matching |accessPointName| if it exists, or undefined.
   * @private
   */
  findApnInList_: function(apnList, accessPointName) {
    return apnList.find(function(a) {
      return a.accessPointName == accessPointName;
    });
  }
});
})();
