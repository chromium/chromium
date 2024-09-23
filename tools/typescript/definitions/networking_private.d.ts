// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.networkingPrivate API
 * Generated from: extensions/common/api/networking_private.idl
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/networking_private.idl -g ts_definitions` to
 * regenerate.
 */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {

    export namespace networkingPrivate {

      export enum ActivationStateType {
        ACTIVATED = 'Activated',
        ACTIVATING = 'Activating',
        NOT_ACTIVATED = 'NotActivated',
        PARTIALLY_ACTIVATED = 'PartiallyActivated',
      }

      export enum CaptivePortalStatus {
        UNKNOWN = 'Unknown',
        OFFLINE = 'Offline',
        ONLINE = 'Online',
        PORTAL = 'Portal',
        PROXY_AUTH_REQUIRED = 'ProxyAuthRequired',
      }

      export enum ConnectionStateType {
        CONNECTED = 'Connected',
        CONNECTING = 'Connecting',
        NOT_CONNECTED = 'NotConnected',
      }

      export enum DeviceStateType {
        UNINITIALIZED = 'Uninitialized',
        DISABLED = 'Disabled',
        ENABLING = 'Enabling',
        ENABLED = 'Enabled',
        PROHIBITED = 'Prohibited',
      }

      export enum IPConfigType {
        DHCP = 'DHCP',
        STATIC = 'Static',
      }

      export enum NetworkType {
        ALL = 'All',
        CELLULAR = 'Cellular',
        ETHERNET = 'Ethernet',
        TETHER = 'Tether',
        VPN = 'VPN',
        WIRELESS = 'Wireless',
        WI_FI = 'WiFi',
      }

      export enum ProxySettingsType {
        DIRECT = 'Direct',
        MANUAL = 'Manual',
        PAC = 'PAC',
        WPAD = 'WPAD',
      }

      export interface ManagedBoolean {
        Active?: boolean;
        Effective?: string;
        UserPolicy?: boolean;
        DevicePolicy?: boolean;
        UserSetting?: boolean;
        SharedSetting?: boolean;
        UserEditable?: boolean;
        DeviceEditable?: boolean;
      }

      export interface ManagedLong {
        Active?: number;
        Effective?: string;
        UserPolicy?: number;
        DevicePolicy?: number;
        UserSetting?: number;
        SharedSetting?: number;
        UserEditable?: boolean;
        DeviceEditable?: boolean;
      }

      export interface ManagedDOMString {
        Active?: string;
        Effective?: string;
        UserPolicy?: string;
        DevicePolicy?: string;
        UserSetting?: string;
        SharedSetting?: string;
        UserEditable?: boolean;
        DeviceEditable?: boolean;
      }

      export interface ManagedDOMStringList {
        Active?: string[];
        Effective?: string;
        UserPolicy?: string[];
        DevicePolicy?: string[];
        UserSetting?: string[];
        SharedSetting?: string[];
        UserEditable?: boolean;
        DeviceEditable?: boolean;
      }

      export interface ManagedIPConfigType {
        Active?: IPConfigType;
        Effective?: string;
        UserPolicy?: IPConfigType;
        DevicePolicy?: IPConfigType;
        UserSetting?: IPConfigType;
        SharedSetting?: IPConfigType;
        UserEditable?: boolean;
        DeviceEditable?: boolean;
      }

      export interface ManagedProxySettingsType {
        Active?: ProxySettingsType;
        Effective?: string;
        UserPolicy?: ProxySettingsType;
        DevicePolicy?: ProxySettingsType;
        UserSetting?: ProxySettingsType;
        SharedSetting?: ProxySettingsType;
        UserEditable?: boolean;
        DeviceEditable?: boolean;
      }

      export interface APNProperties {
        AccessPointName: string;
        Authentication?: string;
        Language?: string;
        LocalizedName?: string;
        Name?: string;
        Password?: string;
        Username?: string;
      }

      export interface ManagedAPNProperties {
        AccessPointName: ManagedDOMString;
        Authentication?: ManagedDOMString;
        Language?: ManagedDOMString;
        LocalizedName?: ManagedDOMString;
        Name?: ManagedDOMString;
        Password?: ManagedDOMString;
        Username?: ManagedDOMString;
      }

      export interface ManagedAPNList {
        Active?: APNProperties[];
        Effective?: string;
        UserPolicy?: APNProperties[];
        DevicePolicy?: APNProperties[];
        UserSetting?: APNProperties[];
        SharedSetting?: APNProperties[];
        UserEditable?: boolean;
        DeviceEditable?: boolean;
      }

      export interface CellularProviderProperties {
        Name: string;
        Code: string;
        Country?: string;
      }

      export interface CellularSimState {
        requirePin: boolean;
        currentPin: string;
        newPin?: string;
      }

      export interface IssuerSubjectPattern {
        CommonName?: string;
        Locality?: string;
        Organization?: string;
        OrganizationalUnit?: string;
      }

      export interface ManagedIssuerSubjectPattern {
        CommonName?: ManagedDOMString;
        Locality?: ManagedDOMString;
        Organization?: ManagedDOMString;
        OrganizationalUnit?: ManagedDOMString;
      }

      export interface CertificatePattern {
        EnrollmentURI?: string[];
        Issuer?: IssuerSubjectPattern;
        IssuerCAPEMs?: string[];
        IssuerCARef?: string[];
        Subject?: IssuerSubjectPattern;
      }

      export interface ManagedCertificatePattern {
        EnrollmentURI?: ManagedDOMStringList;
        Issuer?: ManagedIssuerSubjectPattern;
        IssuerCARef?: ManagedDOMStringList;
        Subject?: ManagedIssuerSubjectPattern;
      }

      export interface EAPProperties {
        AnonymousIdentity?: string;
        ClientCertPattern?: CertificatePattern;
        ClientCertPKCS11Id?: string;
        ClientCertProvisioningProfileId?: string;
        ClientCertRef?: string;
        ClientCertType?: string;
        Identity?: string;
        Inner?: string;
        Outer?: string;
        Password?: string;
        SaveCredentials?: boolean;
        ServerCAPEMs?: string[];
        ServerCARefs?: string[];
        SubjectMatch?: string;
        TLSVersionMax?: string;
        UseProactiveKeyCaching?: boolean;
        UseSystemCAs?: boolean;
      }

      export interface ManagedEAPProperties {
        AnonymousIdentity?: ManagedDOMString;
        ClientCertPattern?: ManagedCertificatePattern;
        ClientCertPKCS11Id?: ManagedDOMString;
        ClientCertProvisioningProfileId?: ManagedDOMString;
        ClientCertRef?: ManagedDOMString;
        ClientCertType?: ManagedDOMString;
        Identity?: ManagedDOMString;
        Inner?: ManagedDOMString;
        Outer?: ManagedDOMString;
        Password?: ManagedDOMString;
        SaveCredentials?: ManagedBoolean;
        ServerCAPEMs?: ManagedDOMStringList;
        ServerCARefs?: ManagedDOMStringList;
        SubjectMatch?: ManagedDOMString;
        TLSVersionMax?: ManagedDOMString;
        UseProactiveKeyCaching?: ManagedBoolean;
        UseSystemCAs?: ManagedBoolean;
      }

      export interface FoundNetworkProperties {
        Status: string;
        NetworkId: string;
        Technology: string;
        ShortName?: string;
        LongName?: string;
      }

      export interface IPConfigProperties {
        Gateway?: string;
        IPAddress?: string;
        ExcludedRoutes?: string[];
        IncludedRoutes?: string[];
        NameServers?: string[];
        SearchDomains?: string[];
        RoutingPrefix?: number;
        Type?: string;
        WebProxyAutoDiscoveryUrl?: string;
      }

      export interface ManagedIPConfigProperties {
        Gateway?: ManagedDOMString;
        IPAddress?: ManagedDOMString;
        NameServers?: ManagedDOMStringList;
        RoutingPrefix?: ManagedLong;
        Type?: ManagedDOMString;
        WebProxyAutoDiscoveryUrl?: ManagedDOMString;
      }

      export interface XAUTHProperties {
        Password?: string;
        SaveCredentials?: boolean;
        Username?: string;
      }

      export interface ManagedXAUTHProperties {
        Password?: ManagedDOMString;
        SaveCredentials?: ManagedBoolean;
        Username?: ManagedDOMString;
      }

      export interface IPSecProperties {
        AuthenticationType: string;
        ClientCertPattern?: CertificatePattern;
        ClientCertPKCS11Id?: string;
        ClientCertProvisioningProfileId?: string;
        ClientCertRef?: string;
        ClientCertType?: string;
        EAP?: EAPProperties;
        Group?: string;
        IKEVersion?: number;
        LocalIdentity?: string;
        PSK?: string;
        RemoteIdentity?: string;
        SaveCredentials?: boolean;
        ServerCAPEMs?: string[];
        ServerCARefs?: string[];
        XAUTH?: XAUTHProperties;
      }

      export interface ManagedIPSecProperties {
        AuthenticationType: ManagedDOMString;
        ClientCertPattern?: ManagedCertificatePattern;
        ClientCertPKCS11Id?: ManagedDOMString;
        ClientCertProvisioningProfileId?: ManagedDOMString;
        ClientCertRef?: ManagedDOMString;
        ClientCertType?: ManagedDOMString;
        EAP?: ManagedEAPProperties;
        Group?: ManagedDOMString;
        IKEVersion?: ManagedLong;
        PSK?: ManagedDOMString;
        SaveCredentials?: ManagedBoolean;
        ServerCAPEMs?: ManagedDOMStringList;
        ServerCARefs?: ManagedDOMStringList;
        XAUTH?: ManagedXAUTHProperties;
      }

      export interface L2TPProperties {
        LcpEchoDisabled?: boolean;
        Password?: string;
        SaveCredentials?: boolean;
        Username?: string;
      }

      export interface ManagedL2TPProperties {
        LcpEchoDisabled?: ManagedBoolean;
        Password?: ManagedDOMString;
        SaveCredentials?: ManagedBoolean;
        Username?: ManagedDOMString;
      }

      export interface PaymentPortal {
        Method: string;
        PostData?: string;
        Url?: string;
      }

      export interface ProxyLocation {
        Host: string;
        Port: number;
      }

      export interface ManagedProxyLocation {
        Host: ManagedDOMString;
        Port: ManagedLong;
      }

      export interface ManualProxySettings {
        HTTPProxy?: ProxyLocation;
        SecureHTTPProxy?: ProxyLocation;
        FTPProxy?: ProxyLocation;
        SOCKS?: ProxyLocation;
      }

      export interface ManagedManualProxySettings {
        HTTPProxy?: ManagedProxyLocation;
        SecureHTTPProxy?: ManagedProxyLocation;
        FTPProxy?: ManagedProxyLocation;
        SOCKS?: ManagedProxyLocation;
      }

      export interface ProxySettings {
        Type: ProxySettingsType;
        Manual?: ManualProxySettings;
        ExcludeDomains?: string[];
        PAC?: string;
      }

      export interface ManagedProxySettings {
        Type: ManagedProxySettingsType;
        Manual?: ManagedManualProxySettings;
        ExcludeDomains?: ManagedDOMStringList;
        PAC?: ManagedDOMString;
      }

      export interface VerifyX509 {
        Name?: string;
        Type?: string;
      }

      export interface ManagedVerifyX509 {
        Name?: ManagedDOMString;
        Type?: ManagedDOMString;
      }

      export interface OpenVPNProperties {
        Auth?: string;
        AuthRetry?: string;
        AuthNoCache?: boolean;
        Cipher?: string;
        ClientCertPKCS11Id?: string;
        ClientCertPattern?: CertificatePattern;
        ClientCertProvisioningProfileId?: string;
        ClientCertRef?: string;
        ClientCertType?: string;
        CompLZO?: string;
        CompNoAdapt?: boolean;
        ExtraHosts?: string[];
        IgnoreDefaultRoute?: boolean;
        KeyDirection?: string;
        NsCertType?: string;
        OTP?: string;
        Password?: string;
        Port?: number;
        Proto?: string;
        PushPeerInfo?: boolean;
        RemoteCertEKU?: string;
        RemoteCertKU?: string[];
        RemoteCertTLS?: string;
        RenegSec?: number;
        SaveCredentials?: boolean;
        ServerCAPEMs?: string[];
        ServerCARefs?: string[];
        ServerCertRef?: string;
        ServerPollTimeout?: number;
        Shaper?: number;
        StaticChallenge?: string;
        TLSAuthContents?: string;
        TLSRemote?: string;
        TLSVersionMin?: string;
        UserAuthenticationType?: string;
        Username?: string;
        Verb?: string;
        VerifyHash?: string;
        VerifyX509?: VerifyX509;
      }

      export interface ManagedOpenVPNProperties {
        Auth?: ManagedDOMString;
        AuthRetry?: ManagedDOMString;
        AuthNoCache?: ManagedBoolean;
        Cipher?: ManagedDOMString;
        ClientCertPKCS11Id?: ManagedDOMString;
        ClientCertPattern?: ManagedCertificatePattern;
        ClientCertProvisioningProfileId?: ManagedDOMString;
        ClientCertRef?: ManagedDOMString;
        ClientCertType?: ManagedDOMString;
        CompLZO?: ManagedDOMString;
        CompNoAdapt?: ManagedBoolean;
        ExtraHosts?: ManagedDOMStringList;
        IgnoreDefaultRoute?: ManagedBoolean;
        KeyDirection?: ManagedDOMString;
        NsCertType?: ManagedDOMString;
        OTP?: ManagedDOMString;
        Password?: ManagedDOMString;
        Port?: ManagedLong;
        Proto?: ManagedDOMString;
        PushPeerInfo?: ManagedBoolean;
        RemoteCertEKU?: ManagedDOMString;
        RemoteCertKU?: ManagedDOMStringList;
        RemoteCertTLS?: ManagedDOMString;
        RenegSec?: ManagedLong;
        SaveCredentials?: ManagedBoolean;
        ServerCAPEMs?: ManagedDOMStringList;
        ServerCARefs?: ManagedDOMStringList;
        ServerCertRef?: ManagedDOMString;
        ServerPollTimeout?: ManagedLong;
        Shaper?: ManagedLong;
        StaticChallenge?: ManagedDOMString;
        TLSAuthContents?: ManagedDOMString;
        TLSRemote?: ManagedDOMString;
        TLSVersionMin?: ManagedDOMString;
        UserAuthenticationType?: ManagedDOMString;
        Username?: ManagedDOMString;
        Verb?: ManagedDOMString;
        VerifyHash?: ManagedDOMString;
        VerifyX509?: ManagedVerifyX509;
      }

      export interface SIMLockStatus {
        LockType: string;
        LockEnabled: boolean;
        RetriesLeft?: number;
      }

      export interface ThirdPartyVPNProperties {
        ExtensionID: string;
        ProviderName?: string;
      }

      export interface ManagedThirdPartyVPNProperties {
        ExtensionID: ManagedDOMString;
        ProviderName?: string;
      }

      export interface CellularProperties {
        AutoConnect?: boolean;
        APN?: APNProperties;
        APNList?: APNProperties[];
        ActivationType?: string;
        ActivationState?: ActivationStateType;
        AllowRoaming?: boolean;
        ESN?: string;
        Family?: string;
        FirmwareRevision?: string;
        FoundNetworks?: FoundNetworkProperties[];
        HardwareRevision?: string;
        HomeProvider?: CellularProviderProperties;
        ICCID?: string;
        IMEI?: string;
        LastGoodAPN?: APNProperties;
        Manufacturer?: string;
        MDN?: string;
        MEID?: string;
        MIN?: string;
        ModelID?: string;
        NetworkTechnology?: string;
        PaymentPortal?: PaymentPortal;
        RoamingState?: string;
        Scanning?: boolean;
        ServingOperator?: CellularProviderProperties;
        SIMLockStatus?: SIMLockStatus;
        SIMPresent?: boolean;
        SignalStrength?: number;
        SupportNetworkScan?: boolean;
      }

      export interface ManagedCellularProperties {
        AutoConnect?: ManagedBoolean;
        APN?: ManagedAPNProperties;
        APNList?: ManagedAPNList;
        ActivationType?: string;
        ActivationState?: ActivationStateType;
        AllowRoaming?: boolean;
        ESN?: string;
        Family?: string;
        FirmwareRevision?: string;
        FoundNetworks?: FoundNetworkProperties[];
        HardwareRevision?: string;
        HomeProvider?: CellularProviderProperties;
        ICCID?: string;
        IMEI?: string;
        LastGoodAPN?: APNProperties;
        Manufacturer?: string;
        MDN?: string;
        MEID?: string;
        MIN?: string;
        ModelID?: string;
        NetworkTechnology?: string;
        PaymentPortal?: PaymentPortal;
        RoamingState?: string;
        Scanning?: boolean;
        ServingOperator?: CellularProviderProperties;
        SIMLockStatus?: SIMLockStatus;
        SIMPresent?: boolean;
        SignalStrength?: number;
        SupportNetworkScan?: boolean;
      }

      export interface CellularStateProperties {
        ActivationState?: ActivationStateType;
        EID?: string;
        ICCID?: string;
        NetworkTechnology?: string;
        RoamingState?: string;
        Scanning?: boolean;
        SIMPresent?: boolean;
        SignalStrength?: number;
      }

      export interface EAPStateProperties {
        Outer?: string;
      }

      export interface EthernetProperties {
        AutoConnect?: boolean;
        Authentication?: string;
        EAP?: EAPProperties;
      }

      export interface ManagedEthernetProperties {
        AutoConnect?: ManagedBoolean;
        Authentication?: ManagedDOMString;
        EAP?: ManagedEAPProperties;
      }

      export interface EthernetStateProperties {
        Authentication: string;
      }

      export interface TetherProperties {
        BatteryPercentage?: number;
        Carrier?: string;
        HasConnectedToHost: boolean;
        SignalStrength?: number;
      }

      export interface VPNProperties {
        AutoConnect?: boolean;
        Host?: string;
        IPsec?: IPSecProperties;
        L2TP?: L2TPProperties;
        OpenVPN?: OpenVPNProperties;
        ThirdPartyVPN?: ThirdPartyVPNProperties;
        Type?: string;
      }

      export interface ManagedVPNProperties {
        AutoConnect?: ManagedBoolean;
        Host?: ManagedDOMString;
        IPsec?: ManagedIPSecProperties;
        L2TP?: ManagedL2TPProperties;
        OpenVPN?: ManagedOpenVPNProperties;
        ThirdPartyVPN?: ManagedThirdPartyVPNProperties;
        Type?: ManagedDOMString;
      }

      export interface VPNStateProperties {
        Type: string;
        IPsec?: IPSecProperties;
        ThirdPartyVPN?: ThirdPartyVPNProperties;
      }

      export interface WiFiProperties {
        AllowGatewayARPPolling?: boolean;
        AutoConnect?: boolean;
        BSSID?: string;
        EAP?: EAPProperties;
        Frequency?: number;
        FrequencyList?: number[];
        HexSSID?: string;
        HiddenSSID?: boolean;
        Passphrase?: string;
        SSID?: string;
        Security?: string;
        SignalStrength?: number;
      }

      export interface ManagedWiFiProperties {
        AllowGatewayARPPolling?: ManagedBoolean;
        AutoConnect?: ManagedBoolean;
        BSSID?: string;
        EAP?: ManagedEAPProperties;
        Frequency?: number;
        FrequencyList?: number[];
        HexSSID?: ManagedDOMString;
        HiddenSSID?: ManagedBoolean;
        Passphrase?: ManagedDOMString;
        SSID?: ManagedDOMString;
        Security: ManagedDOMString;
        SignalStrength?: number;
      }

      export interface WiFiStateProperties {
        BSSID?: string;
        EAP?: EAPStateProperties;
        Frequency?: number;
        HexSSID?: string;
        Security: string;
        SignalStrength?: number;
        SSID?: string;
      }

      export interface NetworkConfigProperties {
        Cellular?: CellularProperties;
        Ethernet?: EthernetProperties;
        GUID?: string;
        IPAddressConfigType?: IPConfigType;
        Name?: string;
        NameServersConfigType?: IPConfigType;
        Priority?: number;
        ProxySettings?: ProxySettings;
        StaticIPConfig?: IPConfigProperties;
        Type?: NetworkType;
        VPN?: VPNProperties;
        WiFi?: WiFiProperties;
      }

      export interface NetworkProperties {
        Cellular?: CellularProperties;
        Connectable?: boolean;
        ConnectionState?: ConnectionStateType;
        ErrorState?: string;
        Ethernet?: EthernetProperties;
        GUID: string;
        IPAddressConfigType?: IPConfigType;
        IPConfigs?: IPConfigProperties[];
        MacAddress?: string;
        Metered?: boolean;
        Name?: string;
        NameServersConfigType?: IPConfigType;
        Priority?: number;
        ProxySettings?: ProxySettings;
        RestrictedConnectivity?: boolean;
        StaticIPConfig?: IPConfigProperties;
        SavedIPConfig?: IPConfigProperties;
        Source?: string;
        Tether?: TetherProperties;
        TrafficCounterResetTime?: number;
        Type: NetworkType;
        VPN?: VPNProperties;
        WiFi?: WiFiProperties;
      }

      export interface ManagedProperties {
        Cellular?: ManagedCellularProperties;
        Connectable?: boolean;
        ConnectionState?: ConnectionStateType;
        ErrorState?: string;
        Ethernet?: ManagedEthernetProperties;
        GUID: string;
        IPAddressConfigType?: ManagedIPConfigType;
        IPConfigs?: IPConfigProperties[];
        MacAddress?: string;
        Metered?: ManagedBoolean;
        Name?: ManagedDOMString;
        NameServersConfigType?: ManagedIPConfigType;
        Priority?: ManagedLong;
        ProxySettings?: ManagedProxySettings;
        RestrictedConnectivity?: boolean;
        StaticIPConfig?: ManagedIPConfigProperties;
        SavedIPConfig?: IPConfigProperties;
        Source?: string;
        Tether?: TetherProperties;
        TrafficCounterResetTime?: number;
        Type: NetworkType;
        VPN?: ManagedVPNProperties;
        WiFi?: ManagedWiFiProperties;
      }

      export interface NetworkStateProperties {
        Cellular?: CellularStateProperties;
        Connectable?: boolean;
        ConnectionState?: ConnectionStateType;
        Ethernet?: EthernetStateProperties;
        ErrorState?: string;
        GUID: string;
        Name?: string;
        Priority?: number;
        Source?: string;
        Tether?: TetherProperties;
        Type: NetworkType;
        VPN?: VPNStateProperties;
        WiFi?: WiFiStateProperties;
      }

      export interface DeviceStateProperties {
        Scanning?: boolean;
        SIMLockStatus?: SIMLockStatus;
        SIMPresent?: boolean;
        State: DeviceStateType;
        Type: NetworkType;
        ManagedNetworkAvailable?: boolean;
      }

      export interface NetworkFilter {
        networkType: NetworkType;
        visible?: boolean;
        configured?: boolean;
        limit?: number;
      }

      export interface GlobalPolicy {
        AllowOnlyPolicyNetworksToAutoconnect?: boolean;
        AllowOnlyPolicyNetworksToConnect?: boolean;
        AllowOnlyPolicyNetworksToConnectIfAvailable?: boolean;
        BlockedHexSSIDs?: string[];
      }

      export interface Certificate {
        hash: string;
        issuedBy: string;
        issuedTo: string;
        pem?: string;
        PKCS11Id?: string;
        hardwareBacked: boolean;
        deviceWide: boolean;
      }

      export interface CertificateLists {
        serverCaCertificates: Certificate[];
        userCertificates: Certificate[];
      }

      export function getProperties(networkGuid: string):
          Promise<NetworkProperties>;

      export function getManagedProperties(networkGuid: string):
          Promise<ManagedProperties>;

      export function getState(networkGuid: string):
          Promise<NetworkStateProperties>;

      export function setProperties(
          networkGuid: string,
          properties: NetworkConfigProperties): Promise<void>;

      export function createNetwork(
          shared: boolean,
          properties: NetworkConfigProperties): Promise<string>;

      export function forgetNetwork(networkGuid: string): Promise<void>;

      export function getNetworks(filter: NetworkFilter):
          Promise<NetworkStateProperties[]>;

      export function getVisibleNetworks(networkType: NetworkType):
          Promise<NetworkStateProperties[]>;

      export function getEnabledNetworkTypes(): Promise<NetworkType[]>;

      export function getDeviceStates(): Promise<DeviceStateProperties[]>;

      export function enableNetworkType(networkType: NetworkType): void;

      export function disableNetworkType(networkType: NetworkType): void;

      export function requestNetworkScan(networkType?: NetworkType): void;

      export function startConnect(networkGuid: string): Promise<void>;

      export function startDisconnect(networkGuid: string): Promise<void>;

      export function startActivate(networkGuid: string, carrier?: string):
          Promise<void>;

      export function getCaptivePortalStatus(networkGuid: string):
          Promise<CaptivePortalStatus>;

      export function unlockCellularSim(
          networkGuid: string, pin: string, puk?: string): Promise<void>;

      export function setCellularSimState(
          networkGuid: string, simState: CellularSimState): Promise<void>;

      export function selectCellularMobileNetwork(
          networkGuid: string, networkId: string): Promise<void>;

      export function getGlobalPolicy(): Promise<GlobalPolicy>;

      export function getCertificateLists(): Promise<CertificateLists>;

      export const onNetworksChanged: ChromeEvent<(changes: string[]) => void>;

      export const onNetworkListChanged:
          ChromeEvent<(changes: string[]) => void>;

      export const onDeviceStateListChanged: ChromeEvent<() => void>;

      export const onPortalDetectionCompleted: ChromeEvent<
          (networkGuid: string, status: CaptivePortalStatus) => void>;

      export const onCertificateListsChanged: ChromeEvent<() => void>;

    }
  }
}

