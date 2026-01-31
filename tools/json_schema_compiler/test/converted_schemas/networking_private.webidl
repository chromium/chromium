// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum ActivationStateType {
  "Activated",
  "Activating",
  "NotActivated",
  "PartiallyActivated"
};

enum CaptivePortalStatus {
  "Unknown",
  "Offline",
  "Online",
  "Portal",
  "ProxyAuthRequired"
};

enum ConnectionStateType { "Connected", "Connecting", "NotConnected" };

enum DeviceStateType {
  // Device is available but not initialized.
  "Uninitialized",
  // Device is initialized but not enabled.
  "Disabled",
  // Enabled state has been requested but has not completed.
  "Enabling",
  // Device is enabled.
  "Enabled",
  // Device is prohibited.
  "Prohibited"
};

enum IPConfigType { "DHCP", "Static" };

enum NetworkType {
  "All",
  "Cellular",
  "Ethernet",
  "Tether",
  "VPN",
  "Wireless",
  "WiFi"
};

enum ProxySettingsType { "Direct", "Manual", "PAC", "WPAD" };

enum ApnType { "Default", "Attach", "Tether" };

enum ApnSource { "Modem", "Modb", "Ui", "Admin" };

// Managed property types. These types all share a common structure:
// Active: For properties that are translated from the configuration
//     manager (e.g. Shill), the 'active' value currently in use by the
//     configuration manager.
// Effective: The effective source for the property: UserPolicy, DevicePolicy,
//     UserSetting or SharedSetting.
// UserPolicy: The value provided by the user policy.
// DevicePolicy: The value provided by the device policy.
// UserSetting: The value set by the logged in user. Only provided if
//     UserEditable is true (i.e. no policy affects the property or the
//     policy provided value is recommened only).
// SharedSetting: The value set for all users of the device. Only provided if
//     DeviceEditiable is true (i.e. no policy affects the property or the
//     policy provided value is recommened only).
// UserEditable: True if a UserPolicy exists and allows the property to be
//     edited (i.e. is a recommended value). Defaults to False.
// DeviceEditable: True if a DevicePolicy exists and allows the property to be
//     edited (i.e. is a recommended value). Defaults to False.

dictionary ManagedBoolean {
  boolean Active;
  DOMString Effective;
  boolean UserPolicy;
  boolean DevicePolicy;
  boolean UserSetting;
  boolean SharedSetting;
  boolean UserEditable;
  boolean DeviceEditable;
};

dictionary ManagedLong {
  long Active;
  DOMString Effective;
  long UserPolicy;
  long DevicePolicy;
  long UserSetting;
  long SharedSetting;
  boolean UserEditable;
  boolean DeviceEditable;
};

dictionary ManagedDOMString {
  DOMString Active;
  DOMString Effective;
  DOMString UserPolicy;
  DOMString DevicePolicy;
  DOMString UserSetting;
  DOMString SharedSetting;
  boolean UserEditable;
  boolean DeviceEditable;
};

dictionary ManagedDOMStringList {
  sequence<DOMString> Active;
  DOMString Effective;
  sequence<DOMString> UserPolicy;
  sequence<DOMString> DevicePolicy;
  sequence<DOMString> UserSetting;
  sequence<DOMString> SharedSetting;
  boolean UserEditable;
  boolean DeviceEditable;
};

dictionary ManagedIPConfigType {
  IPConfigType Active;
  DOMString Effective;
  IPConfigType UserPolicy;
  IPConfigType DevicePolicy;
  IPConfigType UserSetting;
  IPConfigType SharedSetting;
  boolean UserEditable;
  boolean DeviceEditable;
};

dictionary ManagedProxySettingsType {
  ProxySettingsType Active;
  DOMString Effective;
  ProxySettingsType UserPolicy;
  ProxySettingsType DevicePolicy;
  ProxySettingsType UserSetting;
  ProxySettingsType SharedSetting;
  boolean UserEditable;
  boolean DeviceEditable;
};

// Sub-dictionary types.

dictionary APNProperties {
  required DOMString AccessPointName;
  DOMString Authentication;
  DOMString Language;
  DOMString LocalizedName;
  DOMString Name;
  DOMString Password;
  DOMString Username;
  sequence<ApnType> ApnTypes;
  ApnSource Source;
};

dictionary ManagedAPNProperties {
  required ManagedDOMString AccessPointName;
  ManagedDOMString Authentication;
  ManagedDOMString Language;
  ManagedDOMString LocalizedName;
  ManagedDOMString Name;
  ManagedDOMString Password;
  ManagedDOMString Username;
};

dictionary ManagedAPNList {
  sequence<APNProperties> Active;
  DOMString Effective;
  sequence<APNProperties> UserPolicy;
  sequence<APNProperties> DevicePolicy;
  sequence<APNProperties> UserSetting;
  sequence<APNProperties> SharedSetting;
  boolean UserEditable;
  boolean DeviceEditable;
};

dictionary CellularProviderProperties {
  required DOMString Name;
  required DOMString Code;
  DOMString Country;
};

dictionary CellularSimState {
  // Whether or not a PIN should be required.
  required boolean requirePin;

  // The current PIN (required for any change, even when the SIM is unlocked).
  required DOMString currentPin;

  // If provided, change the PIN to |newPin|. |requirePin| must be true.
  DOMString newPin;
};

dictionary IssuerSubjectPattern {
  DOMString CommonName;
  DOMString Locality;
  DOMString Organization;
  DOMString OrganizationalUnit;
};

dictionary ManagedIssuerSubjectPattern {
  ManagedDOMString CommonName;
  ManagedDOMString Locality;
  ManagedDOMString Organization;
  ManagedDOMString OrganizationalUnit;
};

dictionary CertificatePattern {
  sequence<DOMString> EnrollmentURI;
  IssuerSubjectPattern Issuer;
  sequence<DOMString> IssuerCAPEMs;
  sequence<DOMString> IssuerCARef;
  IssuerSubjectPattern Subject;
};

dictionary ManagedCertificatePattern {
  ManagedDOMStringList EnrollmentURI;
  ManagedIssuerSubjectPattern Issuer;
  ManagedDOMStringList IssuerCARef;
  ManagedIssuerSubjectPattern Subject;
};

dictionary EAPProperties {
  DOMString AnonymousIdentity;
  CertificatePattern ClientCertPattern;
  DOMString ClientCertPKCS11Id;
  DOMString ClientCertProvisioningProfileId;
  DOMString ClientCertRef;
  DOMString ClientCertType;
  DOMString Identity;
  DOMString Inner;
  // The outer EAP type. Required by ONC, but may not be provided when
  // translating from Shill.
  DOMString Outer;
  DOMString Password;
  boolean SaveCredentials;
  sequence<DOMString> ServerCAPEMs;
  sequence<DOMString> ServerCARefs;
  DOMString SubjectMatch;
  DOMString TLSVersionMax;
  boolean UseProactiveKeyCaching;
  boolean UseSystemCAs;
};

dictionary ManagedEAPProperties {
  ManagedDOMString AnonymousIdentity;
  ManagedCertificatePattern ClientCertPattern;
  ManagedDOMString ClientCertPKCS11Id;
  ManagedDOMString ClientCertProvisioningProfileId;
  ManagedDOMString ClientCertRef;
  ManagedDOMString ClientCertType;
  ManagedDOMString Identity;
  ManagedDOMString Inner;
  // The outer EAP type. Required by ONC, but may not be provided when
  // translating from Shill.
  ManagedDOMString Outer;
  ManagedDOMString Password;
  ManagedBoolean SaveCredentials;
  ManagedDOMStringList ServerCAPEMs;
  ManagedDOMStringList ServerCARefs;
  ManagedDOMString SubjectMatch;
  ManagedDOMString TLSVersionMax;
  ManagedBoolean UseProactiveKeyCaching;
  ManagedBoolean UseSystemCAs;
};

dictionary FoundNetworkProperties {
  required DOMString Status;
  required DOMString NetworkId;
  required DOMString Technology;
  DOMString ShortName;
  DOMString LongName;
};

dictionary IPConfigProperties {
  DOMString Gateway;
  DOMString IPAddress;
  sequence<DOMString> ExcludedRoutes;
  sequence<DOMString> IncludedRoutes;
  sequence<DOMString> NameServers;
  sequence<DOMString> SearchDomains;
  long RoutingPrefix;
  DOMString Type;
  DOMString WebProxyAutoDiscoveryUrl;
};

dictionary ManagedIPConfigProperties {
  ManagedDOMString Gateway;
  ManagedDOMString IPAddress;
  ManagedDOMStringList NameServers;
  ManagedLong RoutingPrefix;
  ManagedDOMString Type;
  ManagedDOMString WebProxyAutoDiscoveryUrl;
};

dictionary XAUTHProperties {
  DOMString Password;
  boolean SaveCredentials;
  DOMString Username;
};

dictionary ManagedXAUTHProperties {
  ManagedDOMString Password;
  ManagedBoolean SaveCredentials;
  ManagedDOMString Username;
};

dictionary IPSecProperties {
  required DOMString AuthenticationType;
  CertificatePattern ClientCertPattern;
  DOMString ClientCertPKCS11Id;
  DOMString ClientCertProvisioningProfileId;
  DOMString ClientCertRef;
  DOMString ClientCertType;
  EAPProperties EAP;
  DOMString Group;
  long IKEVersion;
  DOMString LocalIdentity;
  DOMString PSK;
  DOMString RemoteIdentity;
  boolean SaveCredentials;
  sequence<DOMString> ServerCAPEMs;
  sequence<DOMString> ServerCARefs;
  XAUTHProperties XAUTH;
};

dictionary ManagedIPSecProperties {
  required ManagedDOMString AuthenticationType;
  ManagedCertificatePattern ClientCertPattern;
  ManagedDOMString ClientCertPKCS11Id;
  ManagedDOMString ClientCertProvisioningProfileId;
  ManagedDOMString ClientCertRef;
  ManagedDOMString ClientCertType;
  ManagedEAPProperties EAP;
  ManagedDOMString Group;
  ManagedLong IKEVersion;
  ManagedDOMString PSK;
  ManagedBoolean SaveCredentials;
  ManagedDOMStringList ServerCAPEMs;
  ManagedDOMStringList ServerCARefs;
  ManagedXAUTHProperties XAUTH;
};

dictionary L2TPProperties {
  boolean LcpEchoDisabled;
  DOMString Password;
  boolean SaveCredentials;
  DOMString Username;
};

dictionary ManagedL2TPProperties {
  ManagedBoolean LcpEchoDisabled;
  ManagedDOMString Password;
  ManagedBoolean SaveCredentials;
  ManagedDOMString Username;
};

dictionary PaymentPortal {
  required DOMString Method;
  DOMString PostData;
  DOMString Url;
};

dictionary ProxyLocation {
  required DOMString Host;
  required long Port;
};

dictionary ManagedProxyLocation {
  required ManagedDOMString Host;
  required ManagedLong Port;
};

dictionary ManualProxySettings {
  ProxyLocation HTTPProxy;
  ProxyLocation SecureHTTPProxy;
  ProxyLocation FTPProxy;
  ProxyLocation SOCKS;
};

dictionary ManagedManualProxySettings {
  ManagedProxyLocation HTTPProxy;
  ManagedProxyLocation SecureHTTPProxy;
  ManagedProxyLocation FTPProxy;
  ManagedProxyLocation SOCKS;
};

dictionary ProxySettings {
  required ProxySettingsType Type;
  ManualProxySettings Manual;
  sequence<DOMString> ExcludeDomains;
  DOMString PAC;
};

dictionary ManagedProxySettings {
  required ManagedProxySettingsType Type;
  ManagedManualProxySettings Manual;
  ManagedDOMStringList ExcludeDomains;
  ManagedDOMString PAC;
};

dictionary VerifyX509 {
  DOMString Name;
  DOMString Type;
};

dictionary ManagedVerifyX509 {
  ManagedDOMString Name;
  ManagedDOMString Type;
};

dictionary OpenVPNProperties {
  DOMString Auth;
  DOMString AuthRetry;
  boolean AuthNoCache;
  DOMString Cipher;
  DOMString ClientCertPKCS11Id;
  CertificatePattern ClientCertPattern;
  DOMString ClientCertProvisioningProfileId;
  DOMString ClientCertRef;
  DOMString ClientCertType;
  DOMString CompLZO;
  boolean CompNoAdapt;
  sequence<DOMString> ExtraHosts;
  boolean IgnoreDefaultRoute;
  DOMString KeyDirection;
  DOMString NsCertType;
  DOMString OTP;
  DOMString Password;
  long Port;
  DOMString Proto;
  boolean PushPeerInfo;
  DOMString RemoteCertEKU;
  sequence<DOMString> RemoteCertKU;
  DOMString RemoteCertTLS;
  long RenegSec;
  boolean SaveCredentials;
  sequence<DOMString> ServerCAPEMs;
  sequence<DOMString> ServerCARefs;
  DOMString ServerCertRef;
  long ServerPollTimeout;
  long Shaper;
  DOMString StaticChallenge;
  DOMString TLSAuthContents;
  DOMString TLSRemote;
  DOMString TLSVersionMin;
  DOMString UserAuthenticationType;
  DOMString Username;
  DOMString Verb;
  DOMString VerifyHash;
  VerifyX509 VerifyX509;
};

dictionary ManagedOpenVPNProperties {
  ManagedDOMString Auth;
  ManagedDOMString AuthRetry;
  ManagedBoolean AuthNoCache;
  ManagedDOMString Cipher;
  ManagedDOMString ClientCertPKCS11Id;
  ManagedCertificatePattern ClientCertPattern;
  ManagedDOMString ClientCertProvisioningProfileId;
  ManagedDOMString ClientCertRef;
  ManagedDOMString ClientCertType;
  ManagedDOMString CompLZO;
  ManagedBoolean CompNoAdapt;
  ManagedDOMStringList ExtraHosts;
  ManagedBoolean IgnoreDefaultRoute;
  ManagedDOMString KeyDirection;
  ManagedDOMString NsCertType;
  ManagedDOMString OTP;
  ManagedDOMString Password;
  ManagedLong Port;
  ManagedDOMString Proto;
  ManagedBoolean PushPeerInfo;
  ManagedDOMString RemoteCertEKU;
  ManagedDOMStringList RemoteCertKU;
  ManagedDOMString RemoteCertTLS;
  ManagedLong RenegSec;
  ManagedBoolean SaveCredentials;
  ManagedDOMStringList ServerCAPEMs;
  ManagedDOMStringList ServerCARefs;
  ManagedDOMString ServerCertRef;
  ManagedLong ServerPollTimeout;
  ManagedLong Shaper;
  ManagedDOMString StaticChallenge;
  ManagedDOMString TLSAuthContents;
  ManagedDOMString TLSRemote;
  ManagedDOMString TLSVersionMin;
  ManagedDOMString UserAuthenticationType;
  ManagedDOMString Username;
  ManagedDOMString Verb;
  ManagedDOMString VerifyHash;
  ManagedVerifyX509 VerifyX509;
};

dictionary SIMLockStatus {
  required DOMString LockType;
  // sim-pin, sim-puk, or ''
  required boolean LockEnabled;
  long RetriesLeft;
};

dictionary ThirdPartyVPNProperties {
  required DOMString ExtensionID;
  DOMString ProviderName;
};

dictionary ManagedThirdPartyVPNProperties {
  required ManagedDOMString ExtensionID;
  DOMString ProviderName;
};

// Network type dictionary types.

dictionary CellularProperties {
  boolean AutoConnect;
  APNProperties APN;
  sequence<APNProperties> APNList;
  DOMString ActivationType;
  ActivationStateType ActivationState;
  boolean AllowRoaming;
  DOMString ESN;
  DOMString Family;
  DOMString FirmwareRevision;
  sequence<FoundNetworkProperties> FoundNetworks;
  DOMString HardwareRevision;
  CellularProviderProperties HomeProvider;
  DOMString ICCID;
  DOMString IMEI;
  APNProperties LastGoodAPN;
  DOMString Manufacturer;
  DOMString MDN;
  DOMString MEID;
  DOMString MIN;
  DOMString ModelID;
  DOMString NetworkTechnology;
  PaymentPortal PaymentPortal;
  DOMString RoamingState;
  boolean Scanning;
  CellularProviderProperties ServingOperator;
  SIMLockStatus SIMLockStatus;
  boolean SIMPresent;
  long SignalStrength;
  boolean SupportNetworkScan;
};

dictionary ManagedCellularProperties {
  ManagedBoolean AutoConnect;
  ManagedAPNProperties APN;
  ManagedAPNList APNList;
  DOMString ActivationType;
  ActivationStateType ActivationState;
  boolean AllowRoaming;
  DOMString ESN;
  DOMString Family;
  DOMString FirmwareRevision;
  sequence<FoundNetworkProperties> FoundNetworks;
  DOMString HardwareRevision;
  CellularProviderProperties HomeProvider;
  DOMString ICCID;
  DOMString IMEI;
  APNProperties LastGoodAPN;
  DOMString Manufacturer;
  DOMString MDN;
  DOMString MEID;
  DOMString MIN;
  DOMString ModelID;
  DOMString NetworkTechnology;
  PaymentPortal PaymentPortal;
  DOMString RoamingState;
  boolean Scanning;
  CellularProviderProperties ServingOperator;
  SIMLockStatus SIMLockStatus;
  boolean SIMPresent;
  long SignalStrength;
  boolean SupportNetworkScan;
};

dictionary CellularStateProperties {
  ActivationStateType ActivationState;
  DOMString EID;
  DOMString ICCID;
  DOMString NetworkTechnology;
  DOMString RoamingState;
  boolean Scanning;
  boolean SIMPresent;
  long SignalStrength;
};

dictionary EAPStateProperties {
  DOMString Outer;
};

dictionary EthernetProperties {
  boolean AutoConnect;
  DOMString Authentication;
  EAPProperties EAP;
};

dictionary ManagedEthernetProperties {
  ManagedBoolean AutoConnect;
  ManagedDOMString Authentication;
  ManagedEAPProperties EAP;
};

dictionary EthernetStateProperties {
  required DOMString Authentication;
};

dictionary TetherProperties {
  long BatteryPercentage;
  DOMString Carrier;
  required boolean HasConnectedToHost;
  long SignalStrength;
};

dictionary VPNProperties {
  boolean AutoConnect;
  DOMString Host;
  IPSecProperties IPsec;
  L2TPProperties L2TP;
  OpenVPNProperties OpenVPN;
  ThirdPartyVPNProperties ThirdPartyVPN;
  // The VPN type. This cannot be an enum because of 'L2TP-IPSec'.
  // This is optional for NetworkConfigProperties which is passed to
  // setProperties which may be used to set only specific properties.
  DOMString Type;
};

dictionary ManagedVPNProperties {
  ManagedBoolean AutoConnect;
  ManagedDOMString Host;
  ManagedIPSecProperties IPsec;
  ManagedL2TPProperties L2TP;
  ManagedOpenVPNProperties OpenVPN;
  ManagedThirdPartyVPNProperties ThirdPartyVPN;
  ManagedDOMString Type;
};

dictionary VPNStateProperties {
  required DOMString Type;
  IPSecProperties IPsec;
  ThirdPartyVPNProperties ThirdPartyVPN;
};

dictionary WiFiProperties {
  boolean AllowGatewayARPPolling;
  boolean AutoConnect;
  DOMString BSSID;
  EAPProperties EAP;
  long Frequency;
  sequence<long> FrequencyList;
  DOMString HexSSID;
  boolean HiddenSSID;
  DOMString Passphrase;
  DOMString SSID;
  DOMString Security;
  long SignalStrength;
};

dictionary ManagedWiFiProperties {
  ManagedBoolean AllowGatewayARPPolling;
  ManagedBoolean AutoConnect;
  DOMString BSSID;
  ManagedEAPProperties EAP;
  long Frequency;
  sequence<long> FrequencyList;
  ManagedDOMString HexSSID;
  ManagedBoolean HiddenSSID;
  ManagedDOMString Passphrase;
  ManagedDOMString SSID;
  required ManagedDOMString Security;
  long SignalStrength;
};

dictionary WiFiStateProperties {
  DOMString BSSID;
  EAPStateProperties EAP;
  long Frequency;
  DOMString HexSSID;
  required DOMString Security;
  long SignalStrength;
  DOMString SSID;
};

dictionary NetworkConfigProperties {
  CellularProperties Cellular;
  EthernetProperties Ethernet;
  DOMString GUID;
  IPConfigType IPAddressConfigType;
  DOMString Name;
  IPConfigType NameServersConfigType;
  long Priority;
  ProxySettings ProxySettings;
  IPConfigProperties StaticIPConfig;
  NetworkType Type;
  VPNProperties VPN;
  WiFiProperties WiFi;
};

dictionary NetworkProperties {
  CellularProperties Cellular;
  boolean Connectable;
  ConnectionStateType ConnectionState;
  DOMString ErrorState;
  EthernetProperties Ethernet;
  required DOMString GUID;
  IPConfigType IPAddressConfigType;
  sequence<IPConfigProperties> IPConfigs;
  DOMString MacAddress;
  boolean Metered;
  DOMString Name;
  IPConfigType NameServersConfigType;
  long Priority;
  ProxySettings ProxySettings;
  boolean RestrictedConnectivity;
  IPConfigProperties StaticIPConfig;
  IPConfigProperties SavedIPConfig;
  // Indicates whether and how the network is configured.
  // 'Source' can be Device, DevicePolicy, User, UserPolicy or None.
  // 'None' conflicts with extension code generation so we must use a string
  // for 'Source' instead of a SourceType enum.
  DOMString Source;
  TetherProperties Tether;
  double TrafficCounterResetTime;
  required NetworkType Type;
  VPNProperties VPN;
  WiFiProperties WiFi;
};

dictionary ManagedProperties {
  ManagedCellularProperties Cellular;
  boolean Connectable;
  ConnectionStateType ConnectionState;
  DOMString ErrorState;
  ManagedEthernetProperties Ethernet;
  required DOMString GUID;
  ManagedIPConfigType IPAddressConfigType;
  sequence<IPConfigProperties> IPConfigs;
  DOMString MacAddress;
  ManagedBoolean Metered;
  ManagedDOMString Name;
  ManagedIPConfigType NameServersConfigType;
  ManagedLong Priority;
  ManagedProxySettings ProxySettings;
  boolean RestrictedConnectivity;
  ManagedIPConfigProperties StaticIPConfig;
  IPConfigProperties SavedIPConfig;
  // See $(ref:NetworkProperties.Source).
  DOMString Source;
  TetherProperties Tether;
  double TrafficCounterResetTime;
  required NetworkType Type;
  ManagedVPNProperties VPN;
  ManagedWiFiProperties WiFi;
};

dictionary NetworkStateProperties {
  CellularStateProperties Cellular;
  boolean Connectable;
  ConnectionStateType ConnectionState;
  EthernetStateProperties Ethernet;
  DOMString ErrorState;
  required DOMString GUID;
  DOMString Name;
  long Priority;
  // See $(ref:NetworkProperties.Source).
  DOMString Source;
  TetherProperties Tether;
  required NetworkType Type;
  VPNStateProperties VPN;
  WiFiStateProperties WiFi;
};

dictionary DeviceStateProperties {
  // Set if the device is enabled. True if the device is currently scanning.
  boolean Scanning;

  // The SIM lock status if Type = Cellular and SIMPresent = True.
  SIMLockStatus SIMLockStatus;

  // Set to the SIM present state if the device type is Cellular.
  boolean SIMPresent;

  // The current state of the device.
  required DeviceStateType State;

  // The network type associated with the device (Cellular, Ethernet or WiFi).
  required NetworkType Type;

  // Whether or not any managed networks are available/visible.
  boolean ManagedNetworkAvailable;
};

dictionary NetworkFilter {
  // The type of networks to return.
  required NetworkType networkType;

  // If true, only include visible (physically connected or in-range)
  // networks. Defaults to 'false'.
  boolean visible;

  // If true, only include configured (saved) networks. Defaults to 'false'.
  boolean configured;

  // Maximum number of networks to return. Defaults to 1000 if unspecified.
  // Use 0 for no limit.
  long limit;
};

dictionary GlobalPolicy {
  // If true, only policy networks may auto connect. Defaults to false.
  boolean AllowOnlyPolicyNetworksToAutoconnect;

  // If true, only policy networks may be connected to and no new networks may
  // be added or configured. Defaults to false.
  boolean AllowOnlyPolicyNetworksToConnect;

  // If true and a managed network is available in the visible network list,
  // only policy networks may be connected to and no new networks may be added
  // or configured. Defaults to false.
  boolean AllowOnlyPolicyNetworksToConnectIfAvailable;

  // List of blocked networks. Connections to blocked networks are
  // prohibited. Networks can be allowed again by specifying an explicit
  // network configuration. Defaults to an empty list.
  sequence<DOMString> BlockedHexSSIDs;
};

dictionary Certificate {
  // Unique hash for the certificate.
  required DOMString hash;

  // Certificate issuer common name.
  required DOMString issuedBy;

  // Certificate name or nickname.
  required DOMString issuedTo;

  // PEM for server CA certificates.
  DOMString pem;

  // PKCS#11 id for user certificates.
  DOMString PKCS11Id;

  // Whether or not the certificate is hardware backed.
  required boolean hardwareBacked;

  // Whether or not the certificate is device wide.
  required boolean deviceWide;
};

dictionary CertificateLists {
  // List of avaliable server CA certificates.
  required sequence<Certificate> serverCaCertificates;

  // List of available user certificates.
  required sequence<Certificate> userCertificates;
};

// Listener callback for the onNetworksChanged event.
callback OnNetworksChangedListener = undefined(sequence<DOMString> changes);

interface OnNetworksChangedEvent : ExtensionEvent {
  static undefined addListener(OnNetworksChangedListener listener);
  static undefined removeListener(OnNetworksChangedListener listener);
  static boolean hasListener(OnNetworksChangedListener listener);
};

// Listener callback for the onNetworkListChanged event.
callback OnNetworkListChangedListener = undefined(sequence<DOMString> changes);

interface OnNetworkListChangedEvent : ExtensionEvent {
  static undefined addListener(OnNetworkListChangedListener listener);
  static undefined removeListener(OnNetworkListChangedListener listener);
  static boolean hasListener(OnNetworkListChangedListener listener);
};

// Listener callback for the onDeviceStateListChanged event.
callback OnDeviceStateListChangedListener = undefined();

interface OnDeviceStateListChangedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceStateListChangedListener listener);
  static undefined removeListener(OnDeviceStateListChangedListener listener);
  static boolean hasListener(OnDeviceStateListChangedListener listener);
};

// Listener callback for the onPortalDetectionCompleted event.
callback OnPortalDetectionCompletedListener =
    undefined(DOMString networkGuid, CaptivePortalStatus status);

interface OnPortalDetectionCompletedEvent : ExtensionEvent {
  static undefined addListener(OnPortalDetectionCompletedListener listener);
  static undefined removeListener(OnPortalDetectionCompletedListener listener);
  static boolean hasListener(OnPortalDetectionCompletedListener listener);
};

// Listener callback for the onCertificateListsChanged event.
callback OnCertificateListsChangedListener = undefined();

interface OnCertificateListsChangedEvent : ExtensionEvent {
  static undefined addListener(OnCertificateListsChangedListener listener);
  static undefined removeListener(OnCertificateListsChangedListener listener);
  static boolean hasListener(OnCertificateListsChangedListener listener);
};

// The <code>chrome.networkingPrivate</code> API is used for configuring
// network connections (Cellular, Ethernet, VPN or WiFi). This private
// API is only valid if called from a browser or app associated with the
// primary user. See the Open Network Configuration (ONC) documentation for
// descriptions of properties:
// <a
// href="https://source.chromium.org/chromium/chromium/src/+/HEAD:components/onc/docs/onc_spec.md">
// src/components/onc/docs/onc_spec.html</a>, or the
// <a
// href="http://www.chromium.org/chromium-os/chromiumos-design-docs/open-network-configuration">
// Open Network Configuration</a> page at chromium.org.
// <br><br>
// NOTE: Most dictionary properties and enum values use UpperCamelCase to match
// the ONC spec instead of the JavaScript lowerCamelCase convention.
// <br><br>
// "State" properties describe just the ONC properties returned by
// $(ref:networkingPrivate.getState) and $(ref:networkingPrivate.getNetworks).
// <br><br>
// "Config" properties describe just the ONC properties that can be configured
// through this API. NOTE: Not all configuration properties are exposed at this
// time, only those currently required by the Chrome Settings UI.
// TODO(stevenjb): Provide all configuration properties and types,
// crbug.com/380937.
// <br><br>
// TODO(stevenjb/pneubeck): Merge the ONC documentation with this document and
// use it as the ONC specification.
interface NetworkingPrivate {
  // Gets all the properties of the network with id networkGuid. Includes all
  // properties of the network (read-only and read/write values).
  // |networkGuid|: The GUID of the network to get properties for.
  // |Returns|: Called with the network properties when received.
  // |PromiseValue|: result
  [requiredCallback] static Promise<NetworkProperties> getProperties(
      DOMString networkGuid);

  // Gets the merged properties of the network with id networkGuid from the
  // sources: User settings, shared settings, user policy, device policy and
  // the currently active settings.
  // |networkGuid|: The GUID of the network to get properties for.
  // |Returns|: Called with the managed network properties when received.
  // |PromiseValue|: result
  [requiredCallback] static Promise<ManagedProperties> getManagedProperties(
      DOMString networkGuid);

  // Gets the cached read-only properties of the network with id networkGuid.
  // This is meant to be a higher performance function than
  // $(ref:getProperties), which requires a round trip to query the networking
  // subsystem. The following properties are returned for all networks: GUID,
  // Type, Name, WiFi.Security. Additional properties are provided for visible
  // networks: ConnectionState, ErrorState, WiFi.SignalStrength,
  // Cellular.NetworkTechnology, Cellular.ActivationState,
  // Cellular.RoamingState.
  // |networkGuid|: The GUID of the network to get properties for.
  // |Returns|: Called immediately with the network state properties.
  // |PromiseValue|: result
  [requiredCallback] static Promise<NetworkStateProperties> getState(
      DOMString networkGuid);

  // Sets the properties of the network with id |networkGuid|. This is only
  // valid for configured networks (Source != None). Unconfigured visible
  // networks should use createNetwork instead.
  // |networkGuid|: The GUID of the network to set properties for.
  // |properties|: The properties to set.
  // |Returns|: Called when the operation has completed.
  static Promise<undefined> setProperties(DOMString networkGuid,
                                          NetworkConfigProperties properties);

  // Creates a new network configuration from properties. If a matching
  // configured network already exists, this will fail. Otherwise returns the
  // guid of the new network.
  // |shared|: If true, share this network configuration with other users.
  // |properties|: The properties to configure the new network with.
  // |Returns|: Called with the GUID for the new network configuration once
  //     the network has been created.
  // |PromiseValue|: result
  static Promise<DOMString> createNetwork(boolean shared,
                                          NetworkConfigProperties properties);

  // Forgets a network configuration by clearing any configured properties for
  // the network with GUID 'networkGuid'. This may also include any other
  // networks with matching identifiers (e.g. WiFi SSID and Security). If no
  // such configuration exists, an error will be set and the operation will
  // fail.
  // |networkGuid|: The GUID of the network to forget.
  // |Returns|: Called when the operation has completed.
  static Promise<undefined> forgetNetwork(DOMString networkGuid);

  // Returns a list of network objects with the same properties provided by
  // $(ref:networkingPrivate.getState). A filter is provided to specify the
  // type of networks returned and to limit the number of networks. Networks
  // are ordered by the system based on their priority, with connected or
  // connecting networks listed first.
  // |filter|: Describes which networks to return.
  // |Returns|: Called with a dictionary of networks and their state
  //     properties when received.
  // |PromiseValue|: result
  [requiredCallback] static Promise<sequence<NetworkStateProperties>>
  getNetworks(NetworkFilter filter);

  // Deprecated. Please use $(ref:networkingPrivate.getNetworks) with
  // filter.visible = true instead.
  // |PromiseValue|: result
  [ deprecated = "Use getNetworks.",
    requiredCallback ] static Promise<sequence<NetworkStateProperties>>
  getVisibleNetworks(NetworkType networkType);

  // Deprecated. Please use $(ref:networkingPrivate.getDeviceStates) instead.
  // |PromiseValue|: result
  [ deprecated = "Use getDeviceStates.",
    requiredCallback ] static Promise<sequence<NetworkType>>
  getEnabledNetworkTypes();

  // Returns a list of $(ref:networkingPrivate.DeviceStateProperties) objects.
  // |Returns|: Called with a list of devices and their state.
  // |PromiseValue|: result
  [requiredCallback] static Promise<sequence<DeviceStateProperties>>
  getDeviceStates();

  // Enables any devices matching the specified network type. Note, the type
  // might represent multiple network types (e.g. 'Wireless').
  // |networkType|: The type of network to enable.
  static undefined enableNetworkType(NetworkType networkType);

  // Disables any devices matching the specified network type. See note for
  // $(ref:networkingPrivate.enableNetworkType).
  // |networkType|: The type of network to disable.
  static undefined disableNetworkType(NetworkType networkType);

  // Requests that the networking subsystem scan for new networks and
  // update the list returned by $(ref:getVisibleNetworks). This is only a
  // request: the network subsystem can choose to ignore it.  If the list
  // is updated, then the $(ref:onNetworkListChanged) event will be fired.
  // |networkType|: If provided, requests a scan specific to the type.
  //     For Cellular a mobile network scan will be requested if supported.
  static undefined requestNetworkScan(optional NetworkType networkType);

  // Starts a connection to the network with networkGuid.
  // |networkGuid|: The GUID of the network to connect to.
  // |Returns|: Called when the connect request has been sent. Note: the
  //     connection may not have completed. Observe $(ref:onNetworksChanged)
  //     to be notified when a network state changes. If the connect request
  //     immediately failed (e.g. the network is unconfigured),
  //     $(ref:runtime.lastError) will be set with a failure reason.
  static Promise<undefined> startConnect(DOMString networkGuid);

  // Starts a disconnect from the network with networkGuid.
  // |networkGuid|: The GUID of the network to disconnect from.
  // |Returns|: Called when the disconnect request has been sent. See note
  //     for $(ref:startConnect).
  static Promise<undefined> startDisconnect(DOMString networkGuid);

  // Starts activation of the Cellular network with networkGuid. If called
  // for a network that is already activated, or for a network with a carrier
  // that can not be directly activated, this will show the account details
  // page for the carrier if possible.
  // |networkGuid|: The GUID of the Cellular network to activate.
  // |carrier|: Optional name of carrier to activate.
  // |Returns|: Called when the activation request has been sent. See note
  //     for $(ref:startConnect).
  static Promise<undefined> startActivate(DOMString networkGuid,
                                          optional DOMString carrier);

  // Returns captive portal status for the network matching 'networkGuid'.
  // |networkGuid|: The GUID of the network to get captive portal status for.
  // |Returns|: A callback function that returns the results of the query for
  //     network captive portal status.
  // |PromiseValue|: result
  [requiredCallback] static Promise<CaptivePortalStatus> getCaptivePortalStatus(
      DOMString networkGuid);

  // Unlocks a Cellular SIM card.
  // * If the SIM is PIN locked, |pin| will be used to unlock the SIM and
  //   the |puk| argument will be ignored if provided.
  // * If the SIM is PUK locked, |puk| and |pin| must be provided. If the
  //   operation succeeds (|puk| is valid), the PIN will be set to |pin|.
  //   (If |pin| is empty or invalid the operation will fail).
  // |networkGuid|: The GUID of the cellular network to unlock.
  //     If empty, the default cellular device will be used.
  // |pin|: The current SIM PIN, or the new PIN if PUK is provided.
  // |puk|: The operator provided PUK for unblocking a blocked SIM.
  // |Returns|: Called when the operation has completed.
  static Promise<undefined> unlockCellularSim(
      DOMString networkGuid, DOMString pin, optional DOMString puk);

  // Sets whether or not SIM locking is enabled (i.e a PIN will be required
  // when the device is powered) and changes the PIN if a new PIN is
  // specified. If the new PIN is provided but not valid (e.g. too short)
  // the operation will fail. This will not lock the SIM; that is handled
  // automatically by the device. NOTE: If the SIM is locked, it must first be
  // unlocked with unlockCellularSim() before this can be called (otherwise it
  // will fail and $(ref:runtime.lastError) will be set to Error.SimLocked).
  // |networkGuid|: The GUID of the cellular network to set the SIM state of.
  //     If empty, the default cellular device will be used.
  // |simState|: The SIM state to set.
  // |Returns|: Called when the operation has completed.
  static Promise<undefined> setCellularSimState(DOMString networkGuid,
                                                CellularSimState simState);

  // Selects which Cellular Mobile Network to use. |networkId| must be the
  // NetworkId property of a member of Cellular.FoundNetworks from the
  // network properties for the specified Cellular network.
  // |networkGuid|: The GUID of the cellular network to select the network
  //     for. If empty, the default cellular device will be used.
  // |networkId|: The networkId to select.
  // |Returns|: Called when the operation has completed.
  static Promise<undefined> selectCellularMobileNetwork(DOMString networkGuid,
                                                        DOMString networkId);

  // Gets the global policy properties. These properties are not expected to
  // change during a session.
  // |PromiseValue|: result
  [requiredCallback] static Promise<GlobalPolicy> getGlobalPolicy();

  // Gets the lists of certificates available for network configuration.
  // |PromiseValue|: result
  [requiredCallback] static Promise<CertificateLists> getCertificateLists();

  // Fired when the properties change on any of the networks.  Sends a list of
  // GUIDs for networks whose properties have changed.
  static attribute OnNetworksChangedEvent onNetworksChanged;

  // Fired when the list of networks has changed.  Sends a complete list of
  // GUIDs for all the current networks.
  static attribute OnNetworkListChangedEvent onNetworkListChanged;

  // Fired when the list of devices has changed or any device state properties
  // have changed.
  static attribute OnDeviceStateListChangedEvent onDeviceStateListChanged;

  // Fired when a portal detection for a network completes. Sends the guid of
  // the network and the corresponding captive portal status.
  static attribute OnPortalDetectionCompletedEvent onPortalDetectionCompleted;

  // Fired when any certificate list has changed.
  static attribute OnCertificateListsChangedEvent onCertificateListsChanged;
};

partial interface Browser {
  static attribute NetworkingPrivate networkingPrivate;
};
