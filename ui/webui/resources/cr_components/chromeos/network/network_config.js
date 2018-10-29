// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'network-config' provides configuration of authentication properties for new
 * and existing networks.
 */

/**
 * Combination of CrOnc.VPNType + AuthenticationType for IPsec.
 * Note: closure does not always recognize this if inside function() {}.
 * @enum {string}
 */
var VPNConfigType = {
  L2TP_IPSEC_PSK: 'L2TP_IPsec_PSK',
  L2TP_IPSEC_CERT: 'L2TP_IPsec_Cert',
  OPEN_VPN: 'OpenVPN',
};

(function() {
'use strict';

/** @const */ var DEFAULT_HASH = 'default';
/** @const */ var DO_NOT_CHECK_HASH = 'do-not-check';
/** @const */ var NO_CERTS_HASH = 'no-certs';
/** @const */ var NO_USER_CERT_HASH = 'no-user-cert';

// Used to indicate a saved but unknown PSK value. Will appear as *'s in the
// PSK field by default.
/** @const */ var UNKNOWN_PSK = '        ';

Polymer({
  is: 'network-config',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Interface for networkingPrivate calls, passed from host.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /** @type {!chrome.networkingPrivate.GlobalPolicy|undefined} */
    globalPolicy: Object,

    /**
     * The GUID when an existing network is being configured. This will be
     * empty when configuring a new network.
     * @private
     */
    guid: String,

    /**
     * The type of network being configured.
     * @private {!chrome.networkingPrivate.NetworkType}
     */
    type: String,

    /** True if the user configuring the network can toggle the shared state. */
    shareAllowEnable: Boolean,

    /** The default shared state. */
    shareDefault: Boolean,

    /** @private */
    enableConnect: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /** @private */
    enableSave: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /** Set to any error from the last configuration result. */
    error: {
      type: String,
      notify: true,
    },

    /**
     * The managed properties of an existing network.
     * This is used for determination of managed fields.
     * This will be undefined when configuring a new network.
     * @private {!chrome.networkingPrivate.ManagedProperties|undefined}
     */
    managedProperties: {
      type: Object,
      notify: true,
    },

    /**
     * Managed EAP properties used for determination of managed EAP fields.
     * @private {?chrome.networkingPrivate.ManagedEAPProperties}
     */
    managedEapProperties_: {
      type: Object,
      value: null,
    },

    /** Set if |guid| is not empty once managedProperties are received. */
    propertiesReceived_: Boolean,

    /** Set once managedProperties have been sent; prevents multiple saves. */
    propertiesSent_: Boolean,

    /**
     * The configuration properties for the network. |configProperties_.Type|
     * will always be defined as the network type being configured.
     * @private {!chrome.networkingPrivate.NetworkConfigProperties}
     */
    configProperties_: Object,

    /**
     * Reference to the EAP properties for the current type or null if all EAP
     * properties should be hidden (e.g. WiFi networks with non EAP Security).
     * Note: even though this references an entry in configProperties_, we
     * need to send a separate notification when it changes for data binding
     * (e.g. by using 'set').
     * @private {?chrome.networkingPrivate.EAPProperties}
     */
    eapProperties_: {
      type: Object,
      value: null,
    },

    /**
     * Used to populate the 'Server CA certificate' dropdown.
     * @private {!Array<!chrome.networkingPrivate.Certificate>}
     */
    serverCaCerts_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @private {string|undefined} */
    selectedServerCaHash_: String,

    /**
     * Used to populate the 'User certificate' dropdown.
     * @private {!Array<!chrome.networkingPrivate.Certificate>}
     */
    userCerts_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @private {string|undefined} */
    selectedUserCertHash_: String,

    /**
     * Set to true when the PSK is saved but the value is unknown.
     * @private
     */
    pskSavedUnknown_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether all required properties have been set.
     * @private
     */
    isConfigured_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether this network should be shared with other users of the device.
     * @private
     */
    shareNetwork_: {
      type: Boolean,
      value: true,
    },

    /**
     * Security value, used for Ethernet and Wifi and to detect when Security
     * changes.
     * @private
     */
    security_: {
      type: String,
      value: '',
    },

    /**
     * 'SaveCredentials' value used for VPN (OpenVPN, IPsec, and L2TP).
     * @private
     */
    vpnSaveCredentials_: {
      type: Boolean,
      value: false,
    },

    /**
     * VPN Type from vpnTypeItems_. Combines VPN.Type and
     * VPN.IPsec.AuthenticationType.
     * @private {VPNConfigType|undefined}
     */
    vpnType_: {
      type: String,
      value: '',
    },

    /**
     * Dictionary of boolean values determining which EAP properties to show,
     * or null to hide all EAP settings.
     * @private {?{
     *   Outer: (boolean|undefined),
     *   Inner: (boolean|undefined),
     *   ServerCA: (boolean|undefined),
     *   SubjectMatch: (boolean|undefined),
     *   UserCert: (boolean|undefined),
     *   Identity: (boolean|undefined),
     *   Password: (boolean|undefined),
     *   AnonymousIdentity: (boolean|undefined),
     * }}
     */
    showEap_: {
      type: Object,
      value: null,
    },

    /**
     * Dictionary of boolean values determining which VPN properties to show,
     * or null to hide all VPN settings.
     * @private {?{
     *   OpenVPN: (boolean|undefined),
     *   Cert: (boolean|undefined),
     * }}
     */
    showVpn_: {
      type: Object,
      value: null,
    },

    /**
     * Object providing network type values for data binding. Note: Currently
     * we only support WiFi, but support for other types will be following
     * shortly.
     * @const
     * @private
     */
    NetworkType_: {
      type: Object,
      value: {
        ETHERNET: CrOnc.Type.ETHERNET,
        VPN: CrOnc.Type.VPN,
        WI_FI: CrOnc.Type.WI_FI,
        WI_MAX: CrOnc.Type.WI_MAX,
      },
      readOnly: true
    },

    /**
     * Array of values for the EAP Method (Outer) dropdown.
     * @private {!Array<string>}
     */
    eapOuterItems_: {
      type: Array,
      readOnly: true,
      computed: 'computeEapOuterItems_(' +
          'guid, shareNetwork_, shareAllowEnable, shareDefault)',
    },

    /**
     * Array of values for the EAP EAP Phase 2 authentication (Inner) dropdown
     * when the Outer type is PEAP.
     * @private {!Array<string>}
     * @const
     */
    eapInnerItemsPeap_: {
      type: Array,
      readOnly: true,
      value: ['Automatic', 'MD5', 'MSCHAPv2'],
    },

    /**
     * Array of values for the EAP EAP Phase 2 authentication (Inner) dropdown
     * when the Outer type is EAP-TTLS.
     * @private {!Array<string>}
     * @const
     */
    eapInnerItemsTtls_: {
      type: Array,
      readOnly: true,
      value: ['Automatic', 'MD5', 'MSCHAP', 'MSCHAPv2', 'PAP', 'CHAP', 'GTC'],
    },

    /**
     * Array of values for the VPN Type dropdown. For L2TP-IPSec, the
     * IPsec AuthenticationType ('PSK' or 'Cert') is included in the type.
     * Note: closure does not recognize Array<VPNConfigType> here.
     * @private {!Array<string>}
     * @const
     */
    vpnTypeItems_: {
      type: Array,
      readOnly: true,
      value: [
        VPNConfigType.L2TP_IPSEC_PSK,
        VPNConfigType.L2TP_IPSEC_CERT,
        VPNConfigType.OPEN_VPN,
      ],
    },

  },

  observers: [
    'setEnableConnect_(isConfigured_, propertiesSent_)',
    'setEnableSave_(isConfigured_, propertiesReceived_)',
    'updateConfigProperties_(managedProperties)',
    'updateSecurity_(configProperties_, security_)',
    'updateEapOuter_(eapProperties_.Outer)',
    'updateEapCerts_(eapProperties_.*, serverCaCerts_, userCerts_)',
    'updateShowEap_(configProperties_.*, eapProperties_.*, security_)',
    'updateVpnType_(configProperties_, vpnType_)',
    'updateVpnIPsecCerts_(vpnType_, configProperties_.VPN.IPsec.*)',
    'updateOpenVPNCerts_(vpnType_, configProperties_.VPN.OpenVPN.*)',
    // Multiple updateIsConfigured observers for different configurations.
    'updateIsConfigured_(configProperties_.*, security_)',
    'updateIsConfigured_(configProperties_, eapProperties_.*)',
    'updateIsConfigured_(configProperties_.WiFi.*)',
    'updateIsConfigured_(configProperties_.VPN.*, vpnType_)',
    'updateIsConfigured_(selectedUserCertHash_)',
  ],

  /** @const */
  MIN_PASSPHRASE_LENGTH: 5,

  /**
   * Listener function for chrome.networkingPrivate.onCertificateListsChanged.
   * @type {?function()}
   * @private
   */
  certificateListsChangedListener_: null,

  /** @override */
  attached: function() {
    this.certificateListsChangedListener_ =
        this.onCertificateListsChanged_.bind(this);
    this.networkingPrivate.onCertificateListsChanged.addListener(
        this.certificateListsChangedListener_);
  },

  /** @override */
  detached: function() {
    assert(this.certificateListsChangedListener_);
    this.networkingPrivate.onCertificateListsChanged.removeListener(
        this.certificateListsChangedListener_);
    this.certificateListsChangedListener_ = null;
  },

  init: function() {
    this.propertiesSent_ = false;
    this.selectedServerCaHash_ = undefined;
    this.selectedUserCertHash_ = undefined;
    this.guid = this.managedProperties.GUID;
    this.type = this.managedProperties.Type;
    if (this.guid) {
      this.networkingPrivate.getManagedProperties(
          this.guid, (managedProperties) => {
            this.getManagedPropertiesCallback_(managedProperties);
            this.focusFirstInput_();
          });
    } else {
      this.async(() => {
        this.focusFirstInput_();
      });
    }
    this.onCertificateListsChanged_();
    this.updateIsConfigured_();
    this.setShareNetwork_();
  },

  save: function() {
    this.saveAndConnect_(false /* connect */);
  },

  connect: function() {
    this.saveAndConnect_(true /* connect */);
  },

  /**
   * @param {boolean} connect If true, connect after save.
   * @private
   */
  saveAndConnect_: function(connect) {
    if (this.propertiesSent_)
      return;
    this.propertiesSent_ = true;
    this.error = '';

    var propertiesToSet = this.getPropertiesToSet_();
    if (this.getSource_() == CrOnc.Source.NONE) {
      // Set 'AutoConnect' to false for VPN or if prohibited by policy.
      // Note: Do not set AutoConnect to true, the connection manager will do
      // that on a successful connection (unless set to false here).
      if (this.type == CrOnc.Type.VPN ||
          (this.globalPolicy &&
           this.globalPolicy.AllowOnlyPolicyNetworksToConnect)) {
        CrOnc.setTypeProperty(propertiesToSet, 'AutoConnect', false);
      }
      this.networkingPrivate.createNetwork(
          this.shareNetwork_, propertiesToSet, (guid) => {
            this.createNetworkCallback_(connect, guid);
          });
    } else {
      this.networkingPrivate.setProperties(this.guid, propertiesToSet, () => {
        this.setPropertiesCallback_(connect);
      });
    }
  },

  /** @private */
  focusFirstInput_: function() {
    Polymer.dom.flush();
    var e = this.$$(
        'network-config-input:not([readonly]),' +
        'network-password-input:not([disabled]),' +
        'network-config-select:not([disabled])');
    if (e)
      e.focus();
  },

  /** @private */
  connectIfConfigured_: function() {
    if (!this.isConfigured_)
      return;
    this.connect();
  },

  /** @private */
  close_: function() {
    this.fire('close');
  },

  /**
   * @return {boolean}
   * @private
   */
  hasGuid_: function() {
    return !!this.guid;
  },

  /**
   * Returns a valid CrOnc.Source.
   * @private
   * @return {!CrOnc.Source}
   */
  getSource_: function() {
    if (!this.guid)
      return CrOnc.Source.NONE;
    var source = this.managedProperties.Source;
    return source ? /** @type {!CrOnc.Source} */ (source) : CrOnc.Source.NONE;
  },

  /** @private */
  onCertificateListsChanged_: function() {
    this.networkingPrivate.getCertificateLists(function(certificateLists) {
      var isOpenVpn = this.type == CrOnc.Type.VPN &&
          this.get('VPN.Type', this.configProperties_) ==
              CrOnc.VPNType.OPEN_VPN;

      var caCerts = certificateLists.serverCaCertificates.slice();
      if (!isOpenVpn) {
        // 'Default' is the same as 'Do not check' except it sets
        // eap.UseSystemCAs (which does not apply to OpenVPN).
        caCerts.unshift(this.getDefaultCert_(
            this.i18n('networkCAUseDefault'), DEFAULT_HASH));
      }
      caCerts.push(this.getDefaultCert_(
          this.i18n('networkCADoNotCheck'), DO_NOT_CHECK_HASH));
      this.set('serverCaCerts_', caCerts);

      var userCerts = certificateLists.userCertificates.slice();
      // Only hardware backed user certs are supported.
      userCerts.forEach(function(cert) {
        if (!cert.hardwareBacked)
          cert.hash = '';  // Clear the hash to invalidate the certificate.
      });
      if (isOpenVpn) {
        // OpenVPN allows but does not require a user certificate.
        userCerts.unshift(this.getDefaultCert_(
            this.i18n('networkNoUserCert'), NO_USER_CERT_HASH));
      }
      if (!userCerts.length) {
        userCerts = [this.getDefaultCert_(
            this.i18n('networkCertificateNoneInstalled'), NO_CERTS_HASH)];
      }
      this.set('userCerts_', userCerts);

      this.updateSelectedCerts_();
      this.updateCertError_();
    }.bind(this));
  },

  /**
   * @param {string} desc
   * @param {string} hash
   * @return {!chrome.networkingPrivate.Certificate}
   * @private
   */
  getDefaultCert_: function(desc, hash) {
    return {hardwareBacked: false, hash: hash, issuedBy: desc, issuedTo: ''};
  },

  /**
   * networkingPrivate.getManagedProperties callback.
   * @param {!chrome.networkingPrivate.ManagedProperties} managedProperties
   * @private
   */
  getManagedPropertiesCallback_: function(managedProperties) {
    if (!managedProperties) {
      // If |managedProperties| is null,
      // the network no longer exists; close the page.
      console.error('Network no longer exists: ' + this.guid);
      this.close_();
      return;
    }

    if (managedProperties.Type == CrOnc.Type.ETHERNET &&
        CrOnc.getActiveValue(
            /** @type {chrome.networkingPrivate.ManagedDOMString|undefined} */
            (this.get('Ethernet.Authentication', managedProperties))) !=
            CrOnc.Authentication.WEP_8021X) {
      // Ethernet may have EAP properties set in a separate EthernetEap
      // configuration. Request that before calling |setManagedProperties_|.
      this.networkingPrivate.getNetworks(
          {networkType: CrOnc.Type.ETHERNET, visible: false, configured: true},
          this.getEthernetEap_.bind(this, managedProperties));
      return;
    }

    if (managedProperties.Type == CrOnc.Type.VPN) {
      this.vpnSaveCredentials_ =
          !!CrOnc.getActiveValue(
              /** @type {chrome.networkingPrivate.ManagedBoolean|undefined} */
              (this.get('VPN.OpenVPN.SaveCredentials', managedProperties))) ||
          !!CrOnc.getActiveValue(
              /** @type {chrome.networkingPrivate.ManagedBoolean|undefined} */
              (this.get('VPN.IPsec.SaveCredentials', managedProperties))) ||
          !!CrOnc.getActiveValue(
              /** @type {chrome.networkingPrivate.ManagedBoolean|undefined} */
              (this.get('VPN.L2TP.SaveCredentials', managedProperties)));
      if (CrOnc.getActiveValue(
              /** @type {chrome.networkingPrivate.ManagedDOMString|undefined} */
              (this.get('VPN.IPsec.PSK', managedProperties))) === '') {
        // If an empty PSK is provided, show a blank value in the UI to indicate
        // that the PSK has a saved value.
        this.pskSavedUnknown_ = true;
      } else {
        this.pskSavedUnknown_ = false;
      }
    }
    this.setManagedProperties_(managedProperties);
  },

  /**
   * If the IPsec.PSK field is focused and the PSK value is saved but unknown,
   * clear the pseudo value set in getPropertiesCallback_.
   * @param {!InputEvent} e
   * @private
   */
  onPskFocus_: function(e) {
    if (this.pskSavedUnknown_) {
      // We can not rely on data binding to update the target value when a
      // field is focused.
      e.target.value = '';
      this.set('VPN.IPsec.PSK', '', this.configProperties_);
    }
  },

  /**
   * If the IPsec.PSK field is in the saved-but-unknown state, restore the
   * pseudo value when the field is unfocused.
   * @param {!InputEvent} e
   * @private
   */
  onPskBlur_: function(e) {
    if (this.pskSavedUnknown_) {
      // The target is still focused so we can not rely on data binding to
      // update the target value.
      e.target.value = UNKNOWN_PSK;
      this.set('VPN.IPsec.PSK', UNKNOWN_PSK, this.configProperties_);
    }
  },

  /**
   * When the IPsec.PSK field is changed, clear pskSavedUnknown_.
   * @param {!InputEvent} e
   * @private
   */
  onPskInput_: function(e) {
    this.pskSavedUnknown_ = false;
  },

  /**
   * @param {!chrome.networkingPrivate.ManagedProperties} managedProperties
   * @private
   */
  setManagedProperties_: function(managedProperties) {
    this.propertiesReceived_ = true;
    this.managedProperties = managedProperties;
    this.managedEapProperties_ = this.getManagedEap_(managedProperties);
    this.setError_(managedProperties.ErrorState);
    this.updateCertError_();

    // Set the current shareNetwork_ value when properties are received.
    this.setShareNetwork_();
  },

  /**
   * networkingPrivate.getNetworks callback. Expects an array of Ethernet
   * networks and looks for an EAP configuration to apply.
   * @param {!chrome.networkingPrivate.ManagedProperties} managedProperties
   * @param {!Array<chrome.networkingPrivate.NetworkStateProperties>} networks
   * @private
   */
  getEthernetEap_: function(managedProperties, networks) {
    if (this.getRuntimeError_()) {
      this.setManagedProperties_(managedProperties);
      return;
    }

    // Look for an existing EAP configuration. This may be stored in a
    // separate 'Ethernet EAP Parameters' configuration.
    var ethernetEap = networks.find(function(network) {
      return !!network.Ethernet &&
          network.Ethernet.Authentication == CrOnc.Authentication.WEP_8021X;
    });
    if (!ethernetEap) {
      this.setManagedProperties_(managedProperties);
      return;
    }

    this.networkingPrivate.getManagedProperties(
        ethernetEap.GUID, (eapProperties) => {
          if (!this.getRuntimeError_() && eapProperties.Ethernet.EAP) {
            this.guid = eapProperties.GUID;
            this.security_ = CrOnc.Security.WPA_EAP;
            managedProperties.GUID = eapProperties.GUID;
            managedProperties.Ethernet.EAP = eapProperties.Ethernet.EAP;
          }
          this.setManagedProperties_(managedProperties);
        });
  },

  /**
   * @return {!Array<string>}
   * @private
   */
  getSecurityItems_() {
    if (this.managedProperties.Type == CrOnc.Type.WI_FI) {
      return [
        CrOnc.Security.NONE, CrOnc.Security.WEP_PSK, CrOnc.Security.WPA_PSK,
        CrOnc.Security.WPA_EAP
      ];
    }
    return [CrOnc.Security.NONE, CrOnc.Security.WPA_EAP];
  },

  /** @private */
  setShareNetwork_: function() {
    var source = this.getSource_();
    if (source != CrOnc.Source.NONE) {
      // Configured networks can not change whether they are shared.
      this.shareNetwork_ =
          source == CrOnc.Source.DEVICE || source == CrOnc.Source.DEVICE_POLICY;
      return;
    }
    if (!this.shareIsVisible_()) {
      this.shareNetwork_ = false;
      return;
    }
    if (this.shareAllowEnable) {
      // New insecure WiFi networks are always shared.
      if (this.managedProperties.Type == CrOnc.Type.WI_FI &&
          this.security_ == CrOnc.Security.NONE) {
        this.shareNetwork_ = true;
        return;
      }
      // Networks requiring a user certificate cannot be shared.
      var eap = this.eapProperties_;
      if (eap && eap.Outer == CrOnc.EAPType.EAP_TLS) {
        this.shareNetwork_ = false;
        return;
      }
    }
    this.shareNetwork_ = this.shareDefault;
  },

  /**
   * Updates the config properties when |this.managedProperties| changes.
   * This gets called once when navigating to the page when default properties
   * are set, and again for existing networks when the properties are received.
   * @private
   */
  updateConfigProperties_: function() {
    this.showEap_ = null;
    this.showVpn_ = null;
    this.vpnType_ = undefined;

    var managedProperties = this.managedProperties;
    var configProperties =
        /** @type {chrome.networkingPrivate.NetworkConfigProperties} */ ({
          Name: CrOnc.getActiveValue(managedProperties.Name) || '',
          Type: managedProperties.Type,
        });
    switch (managedProperties.Type) {
      case CrOnc.Type.WI_FI:
        if (managedProperties.WiFi) {
          configProperties.WiFi = {
            AutoConnect:
                /** @type {boolean|undefined} */ (
                    CrOnc.getActiveValue(managedProperties.WiFi.AutoConnect)),
            EAP: Object.assign(
                {}, CrOnc.getActiveProperties(managedProperties.WiFi.EAP)),
            Passphrase: /** @type {string|undefined} */ (
                CrOnc.getActiveValue(managedProperties.WiFi.Passphrase)),
            SSID: /** @type {string|undefined} */ (
                CrOnc.getActiveValue(managedProperties.WiFi.SSID)),
            Security: /** @type {string|undefined} */ (
                CrOnc.getActiveValue(managedProperties.WiFi.Security))
          };
        } else {
          configProperties.WiFi = {
            AutoConnect: false,
            SSID: '',
            Security: CrOnc.Security.NONE,
          };
        }
        this.security_ = configProperties.WiFi.Security || CrOnc.Security.NONE;
        // updateSecurity_ will ensure that EAP properties are set correctly.
        break;
      case CrOnc.Type.ETHERNET:
        configProperties.Ethernet = {
          AutoConnect: !!CrOnc.getActiveValue(
              /** @type {chrome.networkingPrivate.ManagedBoolean|undefined} */ (
                  this.get('Ethernet.AutoConnect', managedProperties)))
        };
        if (managedProperties.Ethernet && managedProperties.Ethernet.EAP) {
          configProperties.Ethernet.EAP = Object.assign(
              {}, CrOnc.getActiveProperties(managedProperties.Ethernet.EAP)),
          configProperties.Ethernet.EAP.Outer =
              configProperties.Ethernet.EAP.Outer || CrOnc.EAPType.LEAP;
        }
        this.security_ = configProperties.Ethernet.EAP ?
            CrOnc.Security.WPA_EAP :
            CrOnc.Security.NONE;
        break;
      case CrOnc.Type.WI_MAX:
        if (managedProperties.WiMAX) {
          configProperties.WiMAX = {
            AutoConnect:
                /** @type {boolean|undefined} */ (
                    CrOnc.getActiveValue(managedProperties.WiMAX.AutoConnect)),
            EAP: Object.assign(
                {}, CrOnc.getActiveProperties(managedProperties.WiMAX.EAP)),
          };
          // WiMAX has no EAP.Outer property, only Identity and Password.
        } else {
          configProperties.WiMAX = {
            AutoConnect: false,
          };
        }
        this.security_ = CrOnc.Security.WPA_EAP;
        break;
      case CrOnc.Type.VPN:
        if (managedProperties.VPN) {
          var vpn = {
            Host: /** @type {string|undefined} */ (
                CrOnc.getActiveValue(managedProperties.VPN.Host)),
            Type: /** @type {string|undefined} */ (
                CrOnc.getActiveValue(managedProperties.VPN.Type)),
          };
          if (vpn.Type == CrOnc.VPNType.L2TP_IPSEC) {
            vpn.IPsec =
                /** @type {chrome.networkingPrivate.IPSecProperties} */ (
                    Object.assign(
                        {AuthenticationType: CrOnc.IPsecAuthenticationType.PSK},
                        CrOnc.getActiveProperties(
                            managedProperties.VPN.IPsec)));
            if (this.pskSavedUnknown_) {
              this.set('IPsec.PSK', UNKNOWN_PSK, vpn);
            }
            vpn.L2TP = Object.assign(
                {Username: ''},
                CrOnc.getActiveProperties(managedProperties.VPN.L2TP));
          } else {
            assert(vpn.Type == CrOnc.VPNType.OPEN_VPN);
            vpn.OpenVPN = Object.assign(
                {}, CrOnc.getActiveProperties(managedProperties.VPN.OpenVPN));
          }
          configProperties.VPN = vpn;
        } else {
          configProperties.VPN = {
            Type: CrOnc.VPNType.L2TP_IPSEC,
            IPsec: {AuthenticationType: CrOnc.IPsecAuthenticationType.PSK},
            L2TP: {Username: ''},
          };
        }
        this.security_ = CrOnc.Security.NONE;
        break;
    }
    this.configProperties_ = configProperties;
    this.set('eapProperties_', this.getEap_(this.configProperties_));
    if (!this.eapProperties_)
      this.showEap_ = null;
    if (managedProperties.Type == CrOnc.Type.VPN)
      this.vpnType_ = this.getVpnTypeFromProperties_(this.configProperties_);
  },

  /**
   * Ensures that the appropriate properties are set or deleted when |security_|
   * changes.
   * @private
   */
  updateSecurity_: function() {
    if (this.type == CrOnc.Type.WI_FI) {
      this.set('WiFi.Security', this.security_, this.configProperties_);
      // Set the share value to its default when the security type changes.
      this.setShareNetwork_();
    } else if (this.type == CrOnc.Type.ETHERNET) {
      var auth = this.security_ == CrOnc.Security.WPA_EAP ?
          CrOnc.Authentication.WEP_8021X :
          CrOnc.Authentication.NONE;
      this.set('Ethernet.Authentication', auth, this.configProperties_);
    }
    if (this.security_ == CrOnc.Security.WPA_EAP) {
      var eap = this.getEap_(this.configProperties_, true);
      eap.Outer = eap.Outer || CrOnc.EAPType.LEAP;
      this.setEap_(eap);
    } else {
      this.setEap_(null);
    }
  },

  /**
   * Ensures that the appropriate EAP properties are created (or deleted when
   * the EAP.Outer property changes.
   * @private
   */
  updateEapOuter_: function() {
    var eap = this.eapProperties_;
    if (!eap || !eap.Outer)
      return;
    var innerItems = this.getEapInnerItems_(eap.Outer);
    if (innerItems.length > 0) {
      if (!eap.Inner || innerItems.indexOf(eap.Inner) < 0)
        this.set('eapProperties_.Inner', innerItems[0]);
    } else {
      this.set('eapProperties_.Inner', undefined);
    }
    // Set the share value to its default when the EAP.Outer value changes.
    this.setShareNetwork_();
  },

  /** @private */
  updateEapCerts_: function() {
    // EAP is used for all configurable types except VPN.
    if (this.type == CrOnc.Type.VPN)
      return;
    var eap = this.eapProperties_;
    var pem = eap && eap.ServerCAPEMs ? eap.ServerCAPEMs[0] : '';
    var certId =
        eap && eap.ClientCertType == 'PKCS11Id' ? eap.ClientCertPKCS11Id : '';
    this.setSelectedCerts_(pem, certId);
  },

  /** @private */
  updateShowEap_: function() {
    if (!this.eapProperties_ || this.security_ == CrOnc.Security.NONE) {
      this.showEap_ = null;
      this.updateCertError_();
      return;
    }
    var outer = this.eapProperties_.Outer;
    switch (this.type) {
      case CrOnc.Type.WI_MAX:
        this.showEap_ = {
          Identity: true,
          Password: true,
        };
        break;
      case CrOnc.Type.WI_FI:
      case CrOnc.Type.ETHERNET:
        this.showEap_ = {
          Outer: true,
          Inner: outer == CrOnc.EAPType.PEAP || outer == CrOnc.EAPType.EAP_TTLS,
          ServerCA: outer != CrOnc.EAPType.LEAP,
          SubjectMatch: outer == CrOnc.EAPType.EAP_TLS,
          UserCert: outer == CrOnc.EAPType.EAP_TLS,
          Identity: true,
          Password: outer != CrOnc.EAPType.EAP_TLS,
          AnonymousIdentity:
              outer == CrOnc.EAPType.PEAP || outer == CrOnc.EAPType.EAP_TTLS,
        };
        break;
    }
    this.updateCertError_();
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} properties
   * @param {boolean=} opt_create
   * @return {?chrome.networkingPrivate.EAPProperties}
   * @private
   */
  getEap_: function(properties, opt_create) {
    var eap;
    switch (properties.Type) {
      case CrOnc.Type.WI_FI:
        eap = properties.WiFi && properties.WiFi.EAP;
        break;
      case CrOnc.Type.ETHERNET:
        eap = properties.Ethernet && properties.Ethernet.EAP;
        break;
      case CrOnc.Type.WI_MAX:
        eap = properties.WiMAX && properties.WiMAX.EAP;
        break;
    }
    if (opt_create)
      return eap || {};
    if (eap)
      eap.SaveCredentials = eap.SaveCredentials || false;
    return eap || null;
  },

  /**
   * @param {?chrome.networkingPrivate.EAPProperties} eapProperties
   * @private
   */
  setEap_: function(eapProperties) {
    switch (this.type) {
      case CrOnc.Type.WI_FI:
        this.set('WiFi.EAP', eapProperties, this.configProperties_);
        break;
      case CrOnc.Type.ETHERNET:
        this.set('Ethernet.EAP', eapProperties, this.configProperties_);
        break;
      case CrOnc.Type.WI_MAX:
        this.set('WiMAX.EAP', eapProperties, this.configProperties_);
        break;
    }
    this.set('eapProperties_', eapProperties);
  },

  /**
   * @param {!chrome.networkingPrivate.ManagedProperties} managedProperties
   * @return {?chrome.networkingPrivate.ManagedEAPProperties}
   * @private
   */
  getManagedEap_: function(managedProperties) {
    var managedEap;
    switch (managedProperties.Type) {
      case CrOnc.Type.WI_FI:
        managedEap = managedProperties.WiFi && managedProperties.WiFi.EAP;
        break;
      case CrOnc.Type.ETHERNET:
        managedEap =
            managedProperties.Ethernet && managedProperties.Ethernet.EAP;
        break;
      case CrOnc.Type.WI_MAX:
        managedEap = managedProperties.WiMAX && managedProperties.WiMAX.EAP;
        break;
    }
    return managedEap || null;
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} properties
   * @private
   */
  getVpnTypeFromProperties_: function(properties) {
    var vpn = properties.VPN;
    assert(vpn);
    if (vpn.Type == CrOnc.VPNType.L2TP_IPSEC) {
      return vpn.IPsec.AuthenticationType ==
              CrOnc.IPsecAuthenticationType.CERT ?
          VPNConfigType.L2TP_IPSEC_CERT :
          VPNConfigType.L2TP_IPSEC_PSK;
    }
    return VPNConfigType.OPEN_VPN;
  },

  /** @private */
  updateVpnType_: function() {
    if (this.configProperties_ === undefined)
      return;

    var vpn = this.configProperties_.VPN;
    if (!vpn) {
      this.showVpn_ = null;
      this.updateCertError_();
      return;
    }
    switch (this.vpnType_) {
      case VPNConfigType.L2TP_IPSEC_PSK:
        vpn.Type = CrOnc.VPNType.L2TP_IPSEC;
        if (vpn.IPsec)
          vpn.IPsec.AuthenticationType = CrOnc.IPsecAuthenticationType.PSK;
        else
          vpn.IPsec = {AuthenticationType: CrOnc.IPsecAuthenticationType.PSK};
        this.showVpn_ = {Cert: false, OpenVPN: false};
        break;
      case VPNConfigType.L2TP_IPSEC_CERT:
        vpn.Type = CrOnc.VPNType.L2TP_IPSEC;
        if (vpn.IPsec)
          vpn.IPsec.AuthenticationType = CrOnc.IPsecAuthenticationType.CERT;
        else
          vpn.IPsec = {AuthenticationType: CrOnc.IPsecAuthenticationType.CERT};
        this.showVpn_ = {Cert: true, OpenVPN: false};
        break;
      case VPNConfigType.OPEN_VPN:
        vpn.Type = CrOnc.VPNType.OPEN_VPN;
        vpn.OpenVPN = vpn.OpenVPN || {};
        this.showVpn_ = {Cert: true, OpenVPN: true};
        break;
    }
    this.updateCertError_();
    this.onCertificateListsChanged_();
  },

  /** @private */
  updateVpnIPsecCerts_: function() {
    if (this.vpnType_ != VPNConfigType.L2TP_IPSEC_CERT)
      return;
    var pem, certId;
    var ipsec = /** @type {chrome.networkingPrivate.IPSecProperties} */ (
        this.get('VPN.IPsec', this.configProperties_));
    if (ipsec) {
      pem = ipsec.ServerCAPEMs && ipsec.ServerCAPEMs[0];
      certId =
          ipsec.ClientCertType == 'PKCS11Id' ? ipsec.ClientCertPKCS11Id : '';
    }
    this.setSelectedCerts_(pem, certId);
  },

  /** @private */
  updateOpenVPNCerts_: function() {
    if (this.vpnType_ != VPNConfigType.OPEN_VPN)
      return;
    var pem, certId;
    var openvpn = /** @type {chrome.networkingPrivate.OpenVPNProperties} */ (
        this.get('VPN.OpenVPN', this.configProperties_));
    if (openvpn) {
      pem = openvpn.ServerCAPEMs && openvpn.ServerCAPEMs[0];
      certId = openvpn.ClientCertType == 'PKCS11Id' ?
          openvpn.ClientCertPKCS11Id :
          '';
    }
    this.setSelectedCerts_(pem, certId);
  },

  /** @private */
  updateCertError_: function() {
    // If |this.error| was set to something other than a cert error, do not
    // change it.
    /** @const */ var noCertsError = 'networkErrorNoUserCertificate';
    /** @const */ var noValidCertsError = 'networkErrorNotHardwareBacked';
    if (this.error && this.error != noCertsError &&
        this.error != noValidCertsError) {
      return;
    }

    var requireCerts = (this.showEap_ && this.showEap_.UserCert) ||
        (this.showVpn_ && this.showVpn_.UserCert);
    if (!requireCerts) {
      this.setError_('');
      return;
    }
    if (!this.userCerts_.length || this.userCerts_[0].hash == NO_CERTS_HASH) {
      this.setError_(noCertsError);
      return;
    }
    var validUserCert = this.userCerts_.find(function(cert) {
      return !!cert.hash;
    });
    if (!validUserCert) {
      this.setError_(noValidCertsError);
      return;
    }
    this.setError_('');
    return;
  },

  /**
   * Sets the selected cert if |pem| (serverCa) or |certId| (user) is specified.
   * Otherwise sets a default value if no certificate is selected.
   * @param {string|undefined} pem
   * @param {string|undefined} certId
   * @private
   */
  setSelectedCerts_: function(pem, certId) {
    if (pem) {
      var serverCa = this.serverCaCerts_.find(function(cert) {
        return cert.pem == pem;
      });
      if (serverCa)
        this.selectedServerCaHash_ = serverCa.hash;
    }

    if (certId) {
      var userCert = this.userCerts_.find(function(cert) {
        return cert.PKCS11Id == certId;
      });
      if (userCert)
        this.selectedUserCertHash_ = userCert.hash;
    }
    this.updateSelectedCerts_();
    this.updateIsConfigured_();
  },

  /**
   * @param {!Array<!chrome.networkingPrivate.Certificate>} certs
   * @param {string|undefined} hash
   * @private
   * @return {!chrome.networkingPrivate.Certificate|undefined}
   */
  findCert_: function(certs, hash) {
    if (!hash)
      return undefined;
    return certs.find((cert) => {
      return cert.hash == hash;
    });
  },

  /**
   * Called when the certificate list or a selected certificate changes.
   * Ensures that each selected certificate exists in its list, or selects the
   * correct default value.
   * @private
   */
  updateSelectedCerts_: function() {
    if (!this.findCert_(this.serverCaCerts_, this.selectedServerCaHash_))
      this.selectedServerCaHash_ = undefined;
    if (!this.selectedServerCaHash_ ||
        this.selectedServerCaHash_ == DEFAULT_HASH) {
      var eap = this.eapProperties_;
      if (eap && eap.UseSystemCAs === false)
        this.selectedServerCaHash_ = DO_NOT_CHECK_HASH;
    }
    if (!this.selectedServerCaHash_ && this.serverCaCerts_[0])
      this.selectedServerCaHash_ = this.serverCaCerts_[0].hash;

    if (!this.findCert_(this.userCerts_, this.selectedUserCertHash_))
      this.selectedUserCertHash_ = undefined;
    if (!this.selectedUserCertHash_ && this.userCerts_[0])
      this.selectedUserCertHash_ = this.userCerts_[0].hash;
  },

  /**
   * @return {boolean}
   * @private
   */
  getIsConfigured_: function() {
    if (!this.configProperties_)
      return false;

    if (this.configProperties_.Type == CrOnc.Type.VPN)
      return this.vpnIsConfigured_();

    if (this.type == CrOnc.Type.WI_FI) {
      if (!this.get('WiFi.SSID', this.configProperties_))
        return false;
      if (this.configRequiresPassphrase_()) {
        var passphrase = this.get('WiFi.Passphrase', this.configProperties_);
        if (!passphrase || passphrase.length < this.MIN_PASSPHRASE_LENGTH)
          return false;
      }
    }
    if (this.security_ == CrOnc.Security.WPA_EAP)
      return this.eapIsConfigured_();
    return true;
  },

  /** @private */
  updateIsConfigured_: function() {
    this.isConfigured_ = this.getIsConfigured_();
  },

  /**
   * @param {CrOnc.Type} type The type to compare against.
   * @param {CrOnc.Type} networkType The current network type.
   * @return {boolean} True if the network type matches 'type'.
   * @private
   */
  isType_: function(type, networkType) {
    return type == networkType;
  },

  /** @private */
  setEnableSave_: function() {
    this.enableSave = this.isConfigured_ && this.propertiesReceived_;
  },

  /** @private */
  setEnableConnect_: function() {
    this.enableConnect = this.isConfigured_ && !this.propertiesSent_;
  },

  /**
   * @param {string} guid
   * @param {boolean} shareNetwork
   * @param {boolean} shareAllowEnable
   * @param {boolean} shareDefault
   * @return {!Array<string>}
   * @private
   */
  computeEapOuterItems_: function(
      guid, shareNetwork, shareAllowEnable, shareDefault) {
    // If a network must be shared, hide the TLS option. Otherwise selecting
    // TLS will turn off and disable the shared state. NOTE: Ethernet EAP may
    // be set at the Device level, but will be saved as a User configuration.
    if (this.type != CrOnc.Type.ETHERNET &&
        ((this.getSource_() != CrOnc.Source.NONE && shareNetwork) ||
         (!shareAllowEnable && shareDefault))) {
      return [CrOnc.EAPType.LEAP, CrOnc.EAPType.PEAP, CrOnc.EAPType.EAP_TTLS];
    }
    return [
      CrOnc.EAPType.LEAP, CrOnc.EAPType.PEAP, CrOnc.EAPType.EAP_TLS,
      CrOnc.EAPType.EAP_TTLS
    ];
  },

  /**
   * @return {boolean}
   * @private
   */
  securityIsVisible_: function() {
    return this.type == CrOnc.Type.WI_FI || this.type == CrOnc.Type.ETHERNET;
  },

  /**
   * @return {boolean}
   * @private
   */
  securityIsEnabled_: function() {
    // WiFi Security type cannot be changed once configured.
    return !this.guid || this.type == CrOnc.Type.ETHERNET;
  },

  /**
   * @return {boolean}
   * @private
   */
  shareIsVisible_: function() {
    return this.getSource_() == CrOnc.Source.NONE &&
        (this.type == CrOnc.Type.WI_FI || this.type == CrOnc.Type.WI_MAX);
  },

  /**
   * @return {boolean}
   * @private
   */
  shareIsEnabled_: function() {
    if (!this.shareAllowEnable || this.getSource_() != CrOnc.Source.NONE)
      return false;

    if (this.security_ == CrOnc.Security.WPA_EAP) {
      var eap = this.getEap_(this.configProperties_);
      if (eap && eap.Outer == CrOnc.EAPType.EAP_TLS)
        return false;
    }

    if (this.type == CrOnc.Type.WI_FI) {
      // Insecure WiFi networks are always shared.
      if (this.security_ == CrOnc.Security.NONE)
        return false;
    }
    return true;
  },

  /**
   * @return {boolean}
   * @private
   */
  selectedUserCertHashIsValid_: function() {
    return !!this.selectedUserCertHash_ &&
        this.selectedUserCertHash_ != NO_CERTS_HASH;
  },

  /**
   * @return {boolean}
   * @private
   */
  eapIsConfigured_: function() {
    var eap = this.getEap_(this.configProperties_);
    if (!eap)
      return false;
    if (eap.Outer != CrOnc.EAPType.EAP_TLS)
      return true;
    return this.selectedUserCertHashIsValid_();
  },

  /**
   * @return {boolean}
   * @private
   */
  vpnIsConfigured_: function() {
    var vpn = this.configProperties_.VPN;
    if (!this.configProperties_.Name || !vpn || !vpn.Host)
      return false;

    switch (this.vpnType_) {
      case VPNConfigType.L2TP_IPSEC_PSK:
        return !!this.get('L2TP.Username', vpn) &&
            (this.pskSavedUnknown_ || !!this.get('IPsec.PSK', vpn));
      case VPNConfigType.L2TP_IPSEC_CERT:
        return !!this.get('L2TP.Username', vpn) &&
            this.selectedUserCertHashIsValid_();
      case VPNConfigType.OPEN_VPN:
        // OpenVPN should require username + password OR a user cert. However,
        // there may be servers with different requirements so err on the side
        // of permissiveness.
        return true;
    }
    return false;
  },

  /** @private */
  getPropertiesToSet_: function() {
    var propertiesToSet = Object.assign({}, this.configProperties_);
    // Do not set AutoConnect by default, the connection manager will set
    // it to true on a successful connection.
    CrOnc.setTypeProperty(propertiesToSet, 'AutoConnect', undefined);
    if (this.guid)
      propertiesToSet.GUID = this.guid;
    var eap = this.getEap_(propertiesToSet);
    if (eap)
      this.setEapProperties_(eap);
    if (this.configProperties_.Type == CrOnc.Type.VPN) {
      if (this.get('VPN.Type', propertiesToSet) == CrOnc.VPNType.OPEN_VPN)
        this.setOpenVPNProperties_(propertiesToSet);
      else
        this.setVpnIPsecProperties_(propertiesToSet);
    }
    return propertiesToSet;
  },

  /**
   * @return {!Array<string>}
   * @private
   */
  getServerCaPems_: function() {
    var caHash = this.selectedServerCaHash_ || '';
    if (!caHash || caHash == DO_NOT_CHECK_HASH || caHash == DEFAULT_HASH)
      return [];
    var serverCa = this.findCert_(this.serverCaCerts_, caHash);
    return serverCa && serverCa.pem ? [serverCa.pem] : [];
  },

  /**
   * @return {string}
   * @private
   */
  getUserCertPkcs11Id_: function() {
    var userCertHash = this.selectedUserCertHash_ || '';
    if (!this.selectedUserCertHashIsValid_() ||
        userCertHash == NO_USER_CERT_HASH) {
      return '';
    }
    var userCert = this.findCert_(this.userCerts_, userCertHash);
    return (userCert && userCert.PKCS11Id) || '';
  },

  /**
   * @param {!chrome.networkingPrivate.EAPProperties} eap
   * @private
   */
  setEapProperties_: function(eap) {
    eap.UseSystemCAs = this.selectedServerCaHash_ == DEFAULT_HASH;

    eap.ServerCAPEMs = this.getServerCaPems_();

    var pkcs11Id = this.getUserCertPkcs11Id_();
    eap.ClientCertType = pkcs11Id ? 'PKCS11Id' : 'None';
    eap.ClientCertPKCS11Id = pkcs11Id || '';
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} propertiesToSet
   * @private
   */
  setOpenVPNProperties_: function(propertiesToSet) {
    var openvpn = propertiesToSet.VPN.OpenVPN || {};

    openvpn.ServerCAPEMs = this.getServerCaPems_();

    var pkcs11Id = this.getUserCertPkcs11Id_();
    openvpn.ClientCertType = pkcs11Id ? 'PKCS11Id' : 'None';
    openvpn.ClientCertPKCS11Id = pkcs11Id || '';

    if (openvpn.Password) {
      openvpn.UserAuthenticationType = openvpn.OTP ?
          CrOnc.UserAuthenticationType.PASSWORD_AND_OTP :
          CrOnc.UserAuthenticationType.PASSWORD;
    } else if (openvpn.OTP) {
      openvpn.UserAuthenticationType = CrOnc.UserAuthenticationType.OTP;
    } else {
      openvpn.UserAuthenticationType = CrOnc.UserAuthenticationType.NONE;
    }

    openvpn.SaveCredentials = this.vpnSaveCredentials_;

    propertiesToSet.VPN.OpenVPN = openvpn;
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} propertiesToSet
   * @private
   */
  setVpnIPsecProperties_: function(propertiesToSet) {
    var vpn = propertiesToSet.VPN;
    assert(vpn.IPsec);
    if (vpn.IPsec.AuthenticationType == CrOnc.IPsecAuthenticationType.CERT) {
      vpn.IPsec.ClientCertType = 'PKCS11Id';
      vpn.IPsec.ClientCertPKCS11Id = this.getUserCertPkcs11Id_();
      vpn.IPsec.ServerCAPEMs = this.getServerCaPems_();
    }
    vpn.IPsec.IKEVersion = 1;
    vpn.IPsec.SaveCredentials = this.vpnSaveCredentials_;
    vpn.L2TP.SaveCredentials = this.vpnSaveCredentials_;
    if (this.pskSavedUnknown_)
      delete vpn.IPsec.PSK;
  },

  /**
   * @return {string}
   * @private
   */
  getRuntimeError_: function() {
    return (chrome.runtime.lastError && chrome.runtime.lastError.message) || '';
  },

  /**
   * @param {boolean} connect If true, connect after save.
   * @private
   */
  setPropertiesCallback_: function(connect) {
    this.setError_(this.getRuntimeError_());
    if (this.error) {
      console.error('setProperties error: ' + this.guid + ': ' + this.error);
      this.propertiesSent_ = false;
      return;
    }
    var connectState = this.managedProperties.ConnectionState;
    if (connect &&
        (!connectState ||
         connectState == CrOnc.ConnectionState.NOT_CONNECTED)) {
      this.startConnect_(this.guid);
      return;
    }
    this.close_();
  },

  /**
   * @param {boolean} connect If true, connect after save.
   * @param {string} guid
   * @private
   */
  createNetworkCallback_: function(connect, guid) {
    this.setError_(this.getRuntimeError_());
    if (this.error) {
      console.error(
          'createNetworkError, type: ' + this.managedProperties.Type + ': ' +
          'error: ' + this.error);
      this.propertiesSent_ = false;
      return;
    }
    if (connect)
      this.startConnect_(guid);
  },

  /**
   * @param {string} guid
   * @private
   */
  startConnect_: function(guid) {
    this.networkingPrivate.startConnect(guid, () => {
      var error = this.getRuntimeError_();
      if (!error || error == 'connected' || error == 'connect-canceled' ||
          error == 'connecting') {
        // Connect is in progress, completed or canceled, close the dialog.
        this.close_();
        return;
      }
      this.setError_(error);
      console.error('Error connecting to network: ' + error);
      this.propertiesSent_ = false;
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  configRequiresPassphrase_: function() {
    // Note: 'Passphrase' is only used by WiFi; Ethernet and WiMAX use
    // EAP.Password.
    return this.type == CrOnc.Type.WI_FI &&
        (this.security_ == CrOnc.Security.WEP_PSK ||
         this.security_ == CrOnc.Security.WPA_PSK);
  },

  /**
   * @param {string} outer
   * @return {!Array<string>}
   * @private
   */
  getEapInnerItems_: function(outer) {
    if (outer == CrOnc.EAPType.PEAP)
      return this.eapInnerItemsPeap_;
    if (outer == CrOnc.EAPType.EAP_TTLS)
      return this.eapInnerItemsTtls_;
    return [];
  },

  /**
   * @param {string|undefined} error
   * @private
   */
  setError_: function(error) {
    this.error = error || '';
  },

  /**
   * @param {!chrome.networkingPrivate.ManagedProperties} managedProperties
   * @return {chrome.networkingPrivate.ManagedDOMString|undefined}
   * @private
   */
  getManagedSecurity_: function(managedProperties) {
    var managedSecurity = undefined;
    switch (managedProperties.Type) {
      case CrOnc.Type.WI_FI:
        managedSecurity =
            managedProperties.WiFi && managedProperties.WiFi.Security;
        break;
      case CrOnc.Type.ETHERNET:
        managedSecurity = managedProperties.Ethernet &&
            managedProperties.Ethernet.Authentication;
        break;
    }
    return managedSecurity;
  },

  /**
   * @param {!chrome.networkingPrivate.ManagedProperties} managedProperties
   * @return {!chrome.networkingPrivate.ManagedBoolean|undefined}
   * @private
   */
  getManagedVpnSaveCredentials_: function(managedProperties) {
    return /** @type {chrome.networkingPrivate.ManagedBoolean|undefined} */ (
        this.get('VPN.OpenVPN.SaveCredentials', managedProperties) ||
        this.get('VPN.IPsec.SaveCredentials', managedProperties) ||
        this.get('VPN.L2TP.SaveCredentials', managedProperties));
  },

  /**
   * @param {!chrome.networkingPrivate.ManagedProperties} managedProperties
   * @return {!chrome.networkingPrivate.ManagedDOMStringList|undefined}
   * @private
   */
  getManagedVpnServerCaRefs_: function(managedProperties) {
    return /** @type {chrome.networkingPrivate.ManagedDOMStringList|undefined} */ (
        this.get('VPN.OpenVPN.ServerCARefs', managedProperties) ||
        this.get('VPN.IPsec.ServerCARefs', managedProperties));
  },

  /**
   * @param {!chrome.networkingPrivate.ManagedProperties} managedProperties
   * @return {!chrome.networkingPrivate.ManagedDOMString|undefined}
   * @private
   */
  getManagedVpnClientCertType_: function(managedProperties) {
    return /** @type {chrome.networkingPrivate.ManagedDOMString|undefined} */ (
        this.get('VPN.OpenVPN.ClientCertType', managedProperties) ||
        this.get('VPN.IPsec.ClientCertType', managedProperties));
  },
});
})();
