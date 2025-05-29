// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.developerPrivate API */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace developerPrivate {

      export enum ItemType {
        HOSTED_APP = 'hosted_app',
        PACKAGED_APP = 'packaged_app',
        LEGACY_PACKAGED_APP = 'legacy_packaged_app',
        EXTENSION = 'extension',
        THEME = 'theme',
      }

      export interface ItemInspectView {
        path: string;
        render_process_id: number;
        render_view_id: number;
        incognito: boolean;
        generatedBackgroundPage: boolean;
      }

      export interface InstallWarning {
        message: string;
      }

      export enum ExtensionType {
        HOSTED_APP = 'HOSTED_APP',
        PLATFORM_APP = 'PLATFORM_APP',
        LEGACY_PACKAGED_APP = 'LEGACY_PACKAGED_APP',
        EXTENSION = 'EXTENSION',
        THEME = 'THEME',
        SHARED_MODULE = 'SHARED_MODULE',
      }

      export enum Location {
        FROM_STORE = 'FROM_STORE',
        UNPACKED = 'UNPACKED',
        THIRD_PARTY = 'THIRD_PARTY',
        INSTALLED_BY_DEFAULT = 'INSTALLED_BY_DEFAULT',
        UNKNOWN = 'UNKNOWN',
      }

      export enum ViewType {
        APP_WINDOW = 'APP_WINDOW',
        BACKGROUND_CONTENTS = 'BACKGROUND_CONTENTS',
        COMPONENT = 'COMPONENT',
        EXTENSION_BACKGROUND_PAGE = 'EXTENSION_BACKGROUND_PAGE',
        EXTENSION_DIALOG = 'EXTENSION_DIALOG',
        EXTENSION_GUEST = 'EXTENSION_GUEST',
        EXTENSION_POPUP = 'EXTENSION_POPUP',
        EXTENSION_SERVICE_WORKER_BACKGROUND =
            'EXTENSION_SERVICE_WORKER_BACKGROUND',
        TAB_CONTENTS = 'TAB_CONTENTS',
        EXTENSION_SIDE_PANEL = 'EXTENSION_SIDE_PANEL',
        DEVELOPER_TOOLS = 'DEVELOPER_TOOLS',
      }

      export enum ErrorType {
        MANIFEST = 'MANIFEST',
        RUNTIME = 'RUNTIME',
      }

      export enum ErrorLevel {
        LOG = 'LOG',
        WARN = 'WARN',
        ERROR = 'ERROR',
      }

      export enum ExtensionState {
        ENABLED = 'ENABLED',
        DISABLED = 'DISABLED',
        TERMINATED = 'TERMINATED',
        BLOCKLISTED = 'BLOCKLISTED',
      }

      export enum ComandScope {
        GLOBAL = 'GLOBAL',
        CHROME = 'CHROME',
      }

      export interface GetExtensionsInfoOptions {
        includeDisabled?: boolean;
        includeTerminated?: boolean;
      }

      export enum CommandScope {
        GLOBAL = 'GLOBAL',
        CHROME = 'CHROME',
      }

      export enum SafetyCheckWarningReason {
        UNPUBLISHED = 'UNPUBLISHED',
        POLICY = 'POLICY',
        MALWARE = 'MALWARE',
        OFFSTORE = 'OFFSTORE',
        UNWANTED = 'UNWANTED',
        NO_PRIVACY_PRACTICE = 'NO_PRIVACY_PRACTICE',
      }

      export interface AccessModifier {
        isEnabled: boolean;
        isActive: boolean;
      }

      export interface StackFrame {
        lineNumber: number;
        columnNumber: number;
        url: string;
        functionName: string;
      }

      export interface ManifestError {
        type: ErrorType;
        extensionId: string;
        fromIncognito: boolean;
        source: string;
        message: string;
        id: number;
        manifestKey: string;
        manifestSpecific?: string;
      }

      export interface RuntimeError {
        type: ErrorType;
        extensionId: string;
        fromIncognito: boolean;
        source: string;
        message: string;
        id: number;
        severity: ErrorLevel;
        contextUrl: string;
        occurrences: number;
        renderViewId: number;
        renderProcessId: number;
        canInspect: boolean;
        stackTrace: StackFrame[];
      }

      export interface DisableReasons {
        suspiciousInstall: boolean;
        corruptInstall: boolean;
        updateRequired: boolean;
        publishedInStoreRequired: boolean;
        blockedByPolicy: boolean;
        reloading: boolean;
        custodianApprovalRequired: boolean;
        parentDisabledPermissions: boolean;
        unsupportedManifestVersion: boolean;
      }

      export interface OptionsPage {
        openInTab: boolean;
        url: string;
      }

      export interface HomePage {
        url: string;
        specified: boolean;
      }

      export interface ExtensionView {
        url: string;
        renderProcessId: number;
        renderViewId: number;
        incognito: boolean;
        isIframe: boolean;
        type: ViewType;
      }

      export enum HostAccess {
        ON_CLICK = 'ON_CLICK',
        ON_SPECIFIC_SITES = 'ON_SPECIFIC_SITES',
        ON_ALL_SITES = 'ON_ALL_SITES',
      }

      export interface  SafetyCheckStrings {
        panelString?: string;
        detailString?: string;
      }

      export interface ControlledInfo {
        text: string;
      }

      export interface Command {
        description: string;
        keybinding: string;
        name: string;
        isActive: boolean;
        scope: CommandScope;
        isExtensionAction: boolean;
      }

      export interface DependentExtension {
        id: string;
        name: string;
      }

      export interface Permission {
        message: string;
        submessages: string[];
      }

      export interface SiteControl {
        host: string;
        granted: boolean;
      }

      export interface RuntimeHostPermissions {
        hasAllHosts: boolean;
        hostAccess: HostAccess;
        hosts: chrome.developerPrivate.SiteControl[];
      }

      export interface Permissions {
        simplePermissions: chrome.developerPrivate.Permission[];
        runtimeHostPermissions?: RuntimeHostPermissions;
        canAccessSiteData: boolean;
      }

      export interface ExtensionInfo {
        blocklistText?: string;
        safetyCheckText?: SafetyCheckStrings;
        commands: Command[];
        controlledInfo?: ControlledInfo;
        dependentExtensions: DependentExtension[];
        description: string;
        disableReasons: DisableReasons;
        errorCollection: AccessModifier;
        fileAccess: AccessModifier;
        homePage: HomePage;
        iconUrl: string;
        id: string;
        incognitoAccess: AccessModifier;
        installWarnings: string[];
        launchUrl?: string;
        location: Location;
        locationText?: string;
        manifestErrors: ManifestError[];
        manifestHomePageUrl: string;
        mustRemainInstalled: boolean;
        name: string;
        offlineEnabled: boolean;
        optionsPage?: OptionsPage;
        path?: string;
        permissions: Permissions;
        prettifiedPath?: string;
        recommendationsUrl?: string;
        runtimeErrors: RuntimeError[];
        runtimeWarnings: string[];
        state: ExtensionState;
        type: ExtensionType;
        updateUrl: string;
        userMayModify: boolean;
        version: string;
        views: ExtensionView[];
        webStoreUrl: string;
        showSafeBrowsingAllowlistWarning: boolean;
        showAccessRequestsInToolbar: boolean;
        safetyCheckWarningReason: SafetyCheckWarningReason;
        pinnedToToolbar?: boolean;
        isAffectedByMV2Deprecation: boolean;
        didAcknowledgeMV2DeprecationNotice: boolean;
      }

      export interface ProfileInfo {
        canLoadUnpacked: boolean;
        inDeveloperMode: boolean;
        isDeveloperModeControlledByPolicy: boolean;
        isIncognitoAvailable: boolean;
        isChildAccount: boolean;
        isMv2DeprecationNoticeDismissed: boolean;
      }

      export interface ExtensionConfigurationUpdate {
        extensionId: string;
        fileAccess?: boolean;
        incognitoAccess?: boolean;
        errorCollection?: boolean;
        hostAccess?: HostAccess;
        showAccessRequestsInToolbar?: boolean;
        acknowledgeSafetyCheckWarningReason?: SafetyCheckWarningReason;
        pinnedToToolbar?: boolean;
      }

      export interface ProfileConfigurationUpdate {
        inDeveloperMode?: boolean;
        isMv2DeprecationNoticeDismissed?: boolean;
      }

      export interface ExtensionCommandUpdate {
        extensionId: string;
        commandName: string;
        scope?: CommandScope;
        keybinding?: string;
      }

      export interface ReloadOptions {
        failQuietly?: boolean;
        populateErrorForUnpacked?: boolean;
      }

      export interface LoadUnpackedOptions {
        failQuietly?: boolean;
        populateError?: boolean;
        retryGuid?: string;
        useDraggedPath?: boolean;
      }

      export enum PackStatus {
        SUCCESS = 'SUCCESS',
        ERROR = 'ERROR',
        WARNING = 'WARNING',
      }

      export enum FileType {
        LOAD = 'LOAD',
        PEM = 'PEM',
      }

      export enum SelectType {
        FILE = 'FILE',
        FOLDER = 'FOLDER',
      }

      export enum EventType {
        INSTALLED = 'INSTALLED',
        UNINSTALLED = 'UNINSTALLED',
        LOADED = 'LOADED',
        UNLOADED = 'UNLOADED',
        VIEW_REGISTERED = 'VIEW_REGISTERED',
        VIEW_UNREGISTERED = 'VIEW_UNREGISTERED',
        ERROR_ADDED = 'ERROR_ADDED',
        ERRORS_REMOVED = 'ERRORS_REMOVED',
        PREFS_CHANGED = 'PREFS_CHANGED',
        WARNINGS_CHANGED = 'WARNINGS_CHANGED',
        COMMAND_ADDED = 'COMMAND_ADDED',
        COMMAND_REMOVED = 'COMMAND_REMOVED',
        PERMISSIONS_CHANGED = 'PERMISSIONS_CHANGED',
        SERVICE_WORKER_STARTED = 'SERVICE_WORKER_STARTED',
        SERVICE_WORKER_STOPPED = 'SERVICE_WORKER_STOPPED',
        CONFIGURATION_CHANGED = 'CONFIGURATION_CHANGED',
        PINNED_ACTIONS_CHANGED = 'PINNED_ACTIONS_CHANGED',
      }

      export enum SiteSet {
        USER_PERMITTED = 'USER_PERMITTED',
        USER_RESTRICTED = 'USER_RESTRICTED',
        EXTENSION_SPECIFIED = 'EXTENSION_SPECIFIED',
      }

      export interface PackDirectoryResponse {
        message: string;
        item_path: string;
        pem_path: string;
        override_flags: number;
        status: PackStatus;
      }

      export interface EventData {
        event_type: EventType;
        item_id: string;
        extensionInfo?: ExtensionInfo;
      }

      export interface ErrorFileSource {
        beforeHighlight: string;
        highlight: string;
        afterHighlight: string;
      }

      export interface LoadError {
        error: string;
        path: string;
        source?: ErrorFileSource;
        retryGuid: string;
      }

      export interface RequestFileSourceProperties {
        extensionId: string;
        pathSuffix: string;
        message: string;
        manifestKey?: string;
        manifestSpecific?: string;
        lineNumber?: number;
      }

      export interface RequestFileSourceResponse {
        highlight: string;
        beforeHighlight: string;
        afterHighlight: string;
        title: string;
        message: string;
      }

      export interface OpenDevToolsProperties {
        extensionId?: string;
        renderViewId: number;
        renderProcessId: number;
        isServiceWorker?: boolean;
        incognito?: boolean;
        url?: string;
        lineNumber?: number;
        columnNumber?: number;
      }

      export interface DeleteExtensionErrorsProperties {
        extensionId: string;
        errorIds?: number[];
        type?: ErrorType;
      }

      export interface UserSiteSettings {
        permittedSites: string[];
        restrictedSites: string[];
      }

      export interface UserSiteSettingsOptions {
        siteSet: SiteSet;
        hosts: string[];
      }

      export interface SiteInfo {
        siteSet: SiteSet;
        numExtensions: number;
        site: string;
      }

      export interface SiteGroup {
        etldPlusOne: string;
        numExtensions: number;
        sites: SiteInfo[];
      }

      export interface MatchingExtensionInfo {
        id: string;
        siteAccess: HostAccess;
        canRequestAllSites: boolean;
      }

      export interface ExtensionSiteAccessUpdate {
        id: string;
        siteAccess: HostAccess;
      }

      type VoidCallback = () => void;
      type StringCallback = (s: string) => void;

      export function addHostPermission(extensionId: string, host: string):
          Promise<void>;
      export function autoUpdate(): Promise<void>;
      export function choosePath(selectType: SelectType, fileType: FileType):
          Promise<string>;
      export function deleteExtensionErrors(
          properties: DeleteExtensionErrorsProperties): Promise<void>;
      export function getExtensionsInfo(options: GetExtensionsInfoOptions):
          Promise<ExtensionInfo[]>;
      export function getExtensionSize(id: string): Promise<string>;
      export function getProfileConfiguration(): Promise<ProfileInfo>;
      export function installDroppedFile(): Promise<void>;
      export function loadUnpacked(options: LoadUnpackedOptions):
          Promise<LoadError|null>;
      export function notifyDragInstallInProgress(): void;
      export function openDevTools(properties: OpenDevToolsProperties):
          Promise<void>;
      export function packDirectory(
          path: string, privateKeyPath: string,
          flags?: number): Promise<PackDirectoryResponse>;
      export function reload(extensionId: string, options?: ReloadOptions):
          Promise<LoadError|null>;
      export function removeHostPermission(extensionId: string, host: string):
          Promise<void>;
      export function removeMultipleExtensions(extensionIds: string[]):
          Promise<void>;
      export function repairExtension(extensionId: string): Promise<void>;
      export function requestFileSource(properties:
                                            RequestFileSourceProperties):
          Promise<RequestFileSourceResponse>;
      export function setShortcutHandlingSuspended(isSuspended: boolean):
          Promise<void>;
      export function showOptions(extensionId: string): Promise<void>;
      export function showPath(extensionId: string): Promise<void>;
      export function updateExtensionCommand(update: ExtensionCommandUpdate):
          Promise<void>;
      export function updateExtensionConfiguration(
          update: ExtensionConfigurationUpdate): Promise<void>;
      export function updateProfileConfiguration(
          update: ProfileConfigurationUpdate): Promise<void>;
      export function getUserSiteSettings(): Promise<UserSiteSettings>;
      export function addUserSpecifiedSites(options: UserSiteSettingsOptions):
          Promise<void>;
      export function removeUserSpecifiedSites(
          options: UserSiteSettingsOptions): Promise<void>;
      export function getUserAndExtensionSitesByEtld(): Promise<SiteGroup[]>;
      export function getMatchingExtensionsForSite(site: string):
          Promise<MatchingExtensionInfo[]>;
      export function updateSiteAccess(
          site: string, updates: ExtensionSiteAccessUpdate[]): Promise<void>;
      export function dismissSafetyHubExtensionsMenuNotification(): void;
      export function dismissMv2DeprecationPanel(): void;
      export function dismissMv2DeprecationNoticeForExtension(
          extensionId: string): Promise<void>;

      export const onItemStateChanged: ChromeEvent<(data: EventData) => void>;
      export const onProfileStateChanged:
          ChromeEvent<(info: ProfileInfo) => void>;
      export const onUserSiteSettingsChanged:
          ChromeEvent<(settings: UserSiteSettings) => void>;
    }
  }
}
