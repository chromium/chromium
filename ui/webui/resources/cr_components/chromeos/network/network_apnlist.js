// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying a list of cellular
 * access points.
 */
Polymer({
  is: 'network-apnlist',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The current set of properties for the network matching |guid|.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_',
    },

    /**
     * The CrOnc.APNProperties.AccessPointName value of the selected APN.
     * @private
     */
    selectedApn_: {
      type: String,
      value: '',
    },

    /**
     * Selectable list of APN dictionaries for the UI. Includes an entry
     * corresponding to |otherApn| (see below).
     * @private {!Array<!CrOnc.APNProperties>}
     */
    apnSelectList_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * The user settable properties for a new ('other') APN. The values for
     * AccessPointName, Username, and Password will be set to the currently
     * active APN if it does not match an existing list entry.
     * @private {CrOnc.APNProperties|undefined}
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
        return ['AccessPointName', 'Username', 'Password'];
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
          'AccessPointName': 'String',
          'Username': 'String',
          'Password': 'Password'
        };
      },
      readOnly: true
    },
  },

  /** @const */
  DefaultAccessPointName: 'NONE',

  /**
   * Polymer networkProperties changed method.
   */
  networkPropertiesChanged_: function() {
    if (!this.networkProperties || !this.networkProperties.Cellular)
      return;

    /** @type {!CrOnc.APNProperties|undefined} */ var activeApn;
    var cellular = this.networkProperties.Cellular;
    /** @type {!chrome.networkingPrivate.ManagedAPNProperties|undefined} */ var
        apn = cellular.APN;
    if (apn && apn.AccessPointName) {
      activeApn = /** @type {!CrOnc.APNProperties|undefined} */ (
          CrOnc.getActiveProperties(apn));
    } else if (cellular.LastGoodAPN && cellular.LastGoodAPN.AccessPointName) {
      activeApn = cellular.LastGoodAPN;
    }
    this.setApnSelectList_(activeApn);
  },

  /**
   * Sets the list of selectable APNs for the UI. Appends an 'Other' entry
   * (see comments for |otherApn_| above).
   * @param {CrOnc.APNProperties|undefined} activeApn The currently active APN
   *     properties.
   * @private
   */
  setApnSelectList_: function(activeApn) {
    // Copy the list of APNs from this.networkProperties.
    var result = this.getApnList_().slice();

    // Test whether |activeApn| is in the current APN list in networkProperties.
    var activeApnInList = activeApn && result.some(function(a) {
      return a.AccessPointName == activeApn.AccessPointName;
    });

    // If |activeApn| is specified and not in the list, use the active
    // properties for 'other'. Otherwise use any existing 'other' properties.
    var otherApnProperties =
        (activeApn && !activeApnInList) ? activeApn : this.otherApn_;
    var otherApn = this.createApnObject_(otherApnProperties);

    // Always use 'Other' for the name of custom APN entries (the name does
    // not get saved).
    otherApn.Name = 'Other';

    // If no 'active' or 'other' AccessPointName was provided, use the default.
    otherApn.AccessPointName =
        otherApn.AccessPointName || this.DefaultAccessPointName;

    // Save the 'other' properties.
    this.otherApn_ = otherApn;

    // Append 'other' to the end of the list of APNs.
    result.push(otherApn);

    this.apnSelectList_ = result;
    // Set selectedApn_ after dom-repeat has been stamped.
    this.async(() => {
      this.selectedApn_ =
          (activeApn && activeApn.AccessPointName) || otherApn.AccessPointName;
    });
  },

  /**
   * @param {!CrOnc.APNProperties|undefined=} apnProperties
   * @return {!CrOnc.APNProperties} A new APN object with properties from
   *     |apnProperties| if provided.
   * @private
   */
  createApnObject_: function(apnProperties) {
    var newApn = {AccessPointName: ''};
    if (apnProperties)
      Object.assign(newApn, apnProperties);
    return newApn;
  },

  /**
   * @return {!Array<!CrOnc.APNProperties>} The list of APN properties in
   *     |networkProperties| or an empty list if the property is not set.
   * @private
   */
  getApnList_: function() {
    if (!this.networkProperties || !this.networkProperties.Cellular)
      return [];
    /** @type {!chrome.networkingPrivate.ManagedAPNList|undefined} */ var
        apnlist = this.networkProperties.Cellular.APNList;
    if (!apnlist)
      return [];
    return /** @type {!Array<!CrOnc.APNProperties>} */ (
        CrOnc.getActiveValue(apnlist));
  },

  /**
   * Event triggered when the selectApn selection changes.
   * @param {!Event} event
   * @private
   */
  onSelectApnChange_: function(event) {
    var target = /** @type {!HTMLSelectElement} */ (event.target);
    var accessPointName = target.value;
    // When selecting 'Other', don't set a change event unless a valid
    // non-default value has been set for Other.
    if (this.isOtherSelected_(accessPointName) &&
        (!this.otherApn_ || !this.otherApn_.AccessPointName ||
         this.otherApn_.AccessPointName == this.DefaultAccessPointName)) {
      this.selectedApn_ = accessPointName;
      return;
    }
    this.sendApnChange_(accessPointName);
  },

  /**
   * Event triggered when any 'Other' APN network property changes.
   * @param {!{detail: {field: string, value: string}}} event
   * @private
   */
  onOtherApnChange_: function(event) {
    // TODO(benchan/stevenjb): Move this to shill or
    // onc_translator_onc_to_shill.cc.
    var value = (event.detail.field == 'AccessPointName') ?
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
    var apnList = this.getApnList_();
    var apn = this.findApnInList(apnList, accessPointName);
    if (apn == undefined) {
      apn = this.createApnObject_();
      if (this.otherApn_) {
        apn.AccessPointName = this.otherApn_.AccessPointName;
        apn.Username = this.otherApn_.Username;
        apn.Password = this.otherApn_.Password;
      }
    }
    this.fire('apn-change', {field: 'APN', value: apn});
  },

  /**
   * @param {string} accessPointName
   * @return {boolean} True if the 'other' APN is currently selected.
   * @private
   */
  isOtherSelected_: function(accessPointName) {
    if (!this.networkProperties || !this.networkProperties.Cellular)
      return false;
    var apnList = this.getApnList_();
    var apn = this.findApnInList(apnList, accessPointName);
    return apn == undefined;
  },

  /**
   * @param {!CrOnc.APNProperties} apn
   * @return {string} The most descriptive name for the access point.
   * @private
   */
  apnDesc_: function(apn) {
    return apn.LocalizedName || apn.Name || apn.AccessPointName;
  },

  /**
   * @param {!Array<!CrOnc.APNProperties>} apnList
   * @param {string} accessPointName
   * @return {CrOnc.APNProperties|undefined} The entry in |apnList| matching
   *     |accessPointName| if it exists, or undefined.
   * @private
   */
  findApnInList: function(apnList, accessPointName) {
    return apnList.find(function(a) {
      return a.AccessPointName == accessPointName;
    });
  }
});
