// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Possible states a particular device setting can be in.
enum SettingValue {
  "UNKNOWN",
  "DISABLED",
  "ENABLED"
};

// Device info fields returned by the getDeviceInfo API.
dictionary DeviceInfo {
  required DOMString osName;
  required DOMString osVersion;
  required DOMString deviceHostName;
  required DOMString deviceModel;
  required DOMString serialNumber;
  required SettingValue screenLockSecured;
  required SettingValue diskEncrypted;
  required sequence<DOMString> macAddresses;
  DOMString windowsMachineDomain;
  DOMString windowsUserDomain;
  required DOMString securityPatchLevel;

  // This value is only returned on Windows for now.
  SettingValue secureBootEnabled;
};

// Possible states for the EnterpriseRealTimeUrlCheckMode policy.
enum RealtimeUrlCheckMode {
  "DISABLED",
  "ENABLED_MAIN_FRAME"
};

// Possible states for the SafeBrowsingProtectionLevel policy.
enum SafeBrowsingLevel {
  "DISABLED",
  "STANDARD",
  "ENHANCED"
};

// Possible states for the PasswordProtectionWarningTrigger policy
enum PasswordProtectionTrigger {
  "PASSWORD_PROTECTION_OFF",
  "PASSWORD_REUSE",
  "PHISHING_REUSE",
  "POLICY_UNSET"
};

// Context info fields returned by the getContextInfo API.
dictionary ContextInfo {
  required sequence<DOMString> browserAffiliationIds;
  required sequence<DOMString> profileAffiliationIds;
  required sequence<DOMString> onFileAttachedProviders;
  required sequence<DOMString> onFileDownloadedProviders;
  required sequence<DOMString> onBulkDataEntryProviders;
  required sequence<DOMString> onPrintProviders;
  required RealtimeUrlCheckMode realtimeUrlCheckMode;
  required sequence<DOMString> onSecurityEventProviders;
  required DOMString browserVersion;
  required SafeBrowsingLevel safeBrowsingProtectionLevel;
  required boolean siteIsolationEnabled;
  required boolean builtInDnsClientEnabled;
  required PasswordProtectionTrigger passwordProtectionWarningTrigger;
  required boolean chromeRemoteDesktopAppBlocked;
  required SettingValue osFirewall;
  required sequence<DOMString> systemDnsServers;
  DOMString enterpriseProfileId;
};

// The status passed to the callback of <code>getCertificate</code> to
// indicate if the required policy is set.
enum CertificateStatus {
  "OK",
  "POLICY_UNSET"
};

// The certificate, if one meets the requirements, returned by the
// $(ref:getCertificate) API. <code>encodedCertificate</code> will be
// the DER encoding (binary encoding following X.690 Distinguished Encoding
// Rules) of the X.509 certificate.
dictionary Certificate {
  required CertificateStatus status;
  ArrayBuffer encodedCertificate;
};

// Captures the type of event so it can be associated with user or device in
// Chrome for reporting purposes
enum EventType {
  "DEVICE",
  "USER"
};

// Composite object that captures the information we need to report events.
// Some fields like the record and priority are serialized to avoid any
// dependency on proto definitions here, given the fact that they will likely
// change in the future. These will be deserialized and validated in Chrome.
dictionary EnqueueRecordRequest {
  // Serialized record data binary based on the proto definition in
  // //components/reporting/proto/synced/record.proto.
  required Uint8Array recordData;
  // Serialized priority based on the proto definition in
  // //components/reporting/proto/synced/record_constants.proto. Used to
  // determine which records are shed first.
  required long priority;
  required EventType eventType;
};

// Context object containing the content-area user's ID for whom the signals
// collection request is for. This will be used to identify the organization
// in which the user is, and can then be used to determine their affiliation
// with the current browser management state.
dictionary UserContext {
  required DOMString userId;
};

// Enumeration of the various states an AntiVirus software product can be in.
enum AntiVirusProductState {
  "ON",
  "OFF",
  "SNOOZED",
  "EXPIRED"
};

// Metadata about a specific AntiVirus software product.
dictionary AntiVirusSignal {
  required DOMString displayName;
  required DOMString productId;
  required AntiVirusProductState state;
};

// ID of an installed hotfix system update.
dictionary HotfixSignal {
  required DOMString hotfixId;
};

// Used to indicate whether a given signal was correctly found or not, or
// indicate a reason for not being able to find it.
enum PresenceValue {
  // Was unable to determine whether the signal source exists or not. This
  // typically indicates that a failure occurred before even trying to assess
  // its presence.
  "UNSPECIFIED",

  // Current user does not have access to the signal's source.
  "ACCESS_DENIED",

  // The resource was not found.
  "NOT_FOUND",

  // The resource was found.
  "FOUND"
};

// Parameter used to collect information about a specific file system
// resource.
dictionary GetFileSystemInfoOptions {
  required DOMString path;
  required boolean computeSha256;
  required boolean computeExecutableMetadata;
};

dictionary GetFileSystemInfoRequest {
  // Information about the for whom the signal collection request is for.
  required UserContext userContext;

  // Collection of parameters used to conduct signals collection about
  // specific file system resources.
  required sequence<GetFileSystemInfoOptions> options;
};

dictionary GetFileSystemInfoResponse {
  // Path to the file system object for whom those signals were collected.
  required DOMString path;

  // Value indicating whether the specific resource could be found or not.
  required PresenceValue presence;

  // Sha256 hash of a file's bytes. Ignored when path points to a
  // directory. Collected only when computeSha256 is set to true in the
  // given signals collection parameters.
  DOMString sha256Hash;

  // Set of properties only relevant for executable files. Will only be
  // collected if computeExecutableMetadata is set to true in the given
  // signals collection parameters and if path points to an executable file.

  // Is true if a currently running process was spawned from this file.
  boolean isRunning;

  // SHA-256 hashes of the public keys of the certificates used to sign the
  // executable. A hash is computed over the DER-encoded SubjectPublicKeyInfo
  // representation of the key.
  sequence<DOMString> publicKeysHashes;

  // Product name of this executable.
  DOMString productName;

  // Version of this executable.
  DOMString version;
};

enum RegistryHive {
  "HKEY_CLASSES_ROOT",
  "HKEY_LOCAL_MACHINE",
  "HKEY_CURRENT_USER"
};

dictionary GetSettingsOptions {
  // Path to a collection of settings.
  // On Windows it would be the path to the reg key inside the hive.
  // On Mac it would be the path to the plist file.
  required DOMString path;

  // Key specifying the setting entry we're looking for.
  // On Windows, that will be the registry key itself.
  // On Mac, this is a key path used to retrieve a value from
  // valueForKeyPath:.
  required DOMString key;

  // When set to true, the retrieved signal will also include the setting's
  // value. When false, the signal will only contain the setting's
  // presence.
  // Supported setting types on Windows:
  // - REG_SZ
  // - REG_DWORD
  // - REG_QWORD
  // Supported setting types on Mac:
  // - NSString
  // - NSNumber
  required boolean getValue;

  // Windows registry hive containing the desired value.
  // Required value on Windows, will be ignored on other platforms.
  RegistryHive hive;
};

dictionary GetSettingsRequest {
  // Information about the for whom the signal collection request is for.
  required UserContext userContext;

  // Collection of parameters used to conduct signals collection about
  // specific settings of the system.
  required sequence<GetSettingsOptions> options;
};

dictionary GetSettingsResponse {
  // Path as given in the corresponding <code>GetSettingsOptions</code>
  // request.
  required DOMString path;

  // Key as given in the corresponding <code>GetSettingsOptions</code>
  // request.
  required DOMString key;

  // Hive as given in the corresponding <code>GetSettingsOptions</code>
  // request.
  // Present on Windows only.
  RegistryHive hive;

  // Value indicating whether the specific resource could be found or not.
  required PresenceValue presence;

  // JSON-stringified value of the setting. Only set if <code>getValue</code>
  // was true in the corresponding request, and if the setting value was
  // retrievable.
  DOMString value;
};

// Indicates what resulted from an event sent through a `DataMaskingEvent`.
enum EventResult {
  "EVENT_RESULT_DATA_MASKED",
  "EVENT_RESULT_DATA_UNMASKED"
};

// Indicates the type of detector that was used match against data by the data
// masking extension.
enum DetectorType {
  "PREDEFINED_DLP",
  "USER_DEFINED"
};

// Information for a data detector used to apply data masking functionality.
// The fields of this dictionary correspond to the proto fields of
// `MatchedUrlNavigationRule::DataMaskingAction`.
dictionary MatchedDetector {
  required DOMString detectorId;
  required DOMString displayName;
  DOMString maskType;
  DOMString pattern;
  DetectorType detectorType;
  DOMString maskText;
};

// Information for a data leak prevention rule that was used to mask data.
dictionary TriggeredRuleInfo {
  required DOMString ruleId;
  required DOMString ruleName;
  required sequence<MatchedDetector> matchedDetectors;
};

// Event representing that something happened in the data masking extension.
dictionary DataMaskingEvent {
  required DOMString url;
  required EventResult eventResult;
  required sequence<TriggeredRuleInfo> triggeredRuleInfo;
};

dictionary DataMaskingRules {
  // The URL being navigated to that triggered the rules.
  required DOMString url;

  required sequence<TriggeredRuleInfo> triggeredRuleInfo;
};

callback OnDataMaskingRulesTriggeredListener =
    undefined (DataMaskingRules rules);

interface OnDataMaskingRulesTriggeredEvent : ExtensionEvent {
  static undefined addListener(OnDataMaskingRulesTriggeredListener listener);
  static undefined removeListener(OnDataMaskingRulesTriggeredListener listener);
  static boolean hasListener(OnDataMaskingRulesTriggeredListener listener);
};

// Private API for reporting Chrome browser status to admin console.
interface ReportingPrivate {
  // Gets the identity of device that Chrome browser is running on. The ID is
  // retrieved from the local device and used by the Google admin console.
  // |Returns|: Invoked by <code>getDeviceId</code> to return the ID.
  // |PromiseValue|: id
  [platforms=("win", "mac", "linux")]
  static Promise<DOMString> getDeviceId();

  // Gets a randomly generated persistent secret (symmetric key) that
  // can be used to encrypt the data stored with |setDeviceData|. If the
  // optional parameter |forceCreation| is set to true the secret is recreated
  // in case of any failure to retrieve the currently stored one. Sets
  // $(ref:runtime.lastError) on failure.
  // |Returns|: Invoked by <code>getPersistentSecret</code> to return the
  // secret.
  // |PromiseValue|: secret
  [platforms=("win", "mac"), requiredCallback]
  static Promise<ArrayBuffer> getPersistentSecret(
      optional boolean resetSecret);

  // Gets the device data for |id|. Sets $(ref:runtime.lastError) on failure.
  // |Returns|: Invoked by <code>getDeviceDataCallback</code> to return the
  // device data.
  // |PromiseValue|: data
  [platforms=("win", "mac", "linux"), requiredCallback]
  static Promise<ArrayBuffer> getDeviceData(DOMString id);

  // Sets the device data for |id|. Sets $(ref:runtime.lastError) on failure.
  // If the |data| parameter is undefined and there is already data
  // associated with |id| it will be cleared.
  // |Returns|: Invoked by <code>UploadChromeDesktopReport</code> when the
  // upload is finished. Also Invoked by <code>setDeviceData</code> when data
  // is stored.
  [platforms=("win", "mac", "linux")]
  static Promise<undefined> setDeviceData(
      DOMString id,
      optional ArrayBuffer data);

  // Gets the device information (including disk encryption status,
  // screen lock status, serial number, model).
  // |Returns|: Invoked by <code>getDeviceInfo</code> to return device
  // information.
  // |PromiseValue|: deviceInfo
  [platforms=("win", "mac", "linux"), requiredCallback]
  static Promise<DeviceInfo> getDeviceInfo();

  // Gets the context information (including management status of the browser,
  // state of key security policies, browser version).
  // |Returns|: Invoked by <code>getContextInfo</code> to return context
  // information.
  // |PromiseValue|: contextInfo
  [requiredCallback] static Promise<ContextInfo> getContextInfo();

  // Returns the certificate that would be selected by the filters in the
  // AutoSelectCertificateForUrls policy for <code>url</code>.
  // |Returns|: Invoked by <code>getCertificate</code> to return the selected
  // certificate.
  // |PromiseValue|: certificate
  [requiredCallback] static Promise<Certificate> getCertificate(DOMString url);

  // Enqueues a record for upload to the reporting service
  // |request|: Composite object that captures everything we need for uploading
  // records.
  // |Returns|: Callback that is triggered upon completion
  [platforms=("chromeos")]
  static Promise<undefined> enqueueRecord(EnqueueRecordRequest request);

  // Gets information about file system resources, specified by the contents
  // of <code>request</code>, on the current device. <code>request</code> must
  // hold a user context to be used to verify the affiliation between the
  // user's organization and the organization managing the browser. If the
  // management or affiliation states are not suitable, no results will be
  // returned.
  // |PromiseValue|: fileSystemSignals
  [platforms=("win", "mac", "linux"), requiredCallback]
  static Promise<sequence<GetFileSystemInfoResponse>> getFileSystemInfo(
      GetFileSystemInfoRequest request);

  // Gets information about system settings specified by the contents of
  // <code>request</code>. <code>request</code> must hold a user context to be
  // used to verify the affiliation between the user's organization and the
  // organization managing the browser. If the management or affiliation
  // states are not suitable, no results will be returned.
  // |PromiseValue|: settings
  [platforms=("win", "mac"), requiredCallback]
  static Promise<sequence<GetSettingsResponse>> getSettings(
      GetSettingsRequest request);

  // Gets information about AntiVirus software installed on the current
  // device. <code>userContext</code> is used to verify the affiliation
  // between the user's organization and the organization managing the
  // browser. If the management, or affiliation, state is not suitable, no
  // results will be returned.
  // |Returns|: Invoked by <code>getAvInfo</code> to return information about
  // installed AntiVirus software.
  // |PromiseValue|: avSignals
  [platforms=("win"), requiredCallback]
  static Promise<sequence<AntiVirusSignal>> getAvInfo(
      UserContext userContext);

  // Gets information about hotfix system updates installed on the current
  // device. <code>userContext</code> is used to verify the affiliation
  // between the user's organization and the organization managing the
  // browser. If the management, or affiliation, state is not suitable, no
  // results will be returned.
  // |Returns|: Invoked by <code>getHotfixes</code> to return the IDs of
  // installed hotfix system updates.
  // |PromiseValue|: hotfixSignals
  [platforms=("win"), requiredCallback]
  static Promise<sequence<HotfixSignal>> getHotfixes(
      UserContext userContext);

  // Sends the passed `event` to the reporting service if the browser or
  // profile is managed and the "OnSecurityEventEnterpriseConnector" policy is
  // enabled.
  // |Returns|: Invoked by <code>UploadChromeDesktopReport</code> when the
  // upload is finished. Also Invoked by <code>setDeviceData</code> when data is
  // stored.
  [requiredCallback] static Promise<undefined> reportDataMaskingEvent(
      DataMaskingEvent event);

  static attribute OnDataMaskingRulesTriggeredEvent onDataMaskingRulesTriggered;
};

partial interface Enterprise {
  static attribute ReportingPrivate reportingPrivate;
};

partial interface Browser {
  static attribute Enterprise enterprise;
};
